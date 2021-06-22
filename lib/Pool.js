const EventEmitter = require('events');
const async = require('async');
const binary = require('@mapbox/node-pre-gyp');
const path = require('path');

const { Connection } = require('./Connection');

const bindingPath = binary.find(path.resolve(path.join(__dirname, '../package.json')));

const odbc = require(bindingPath);

const REUSE_CONNECTIONS_DEFAULT = true;
const INITIAL_SIZE_DEFAULT = 10;
const INCREMENT_SIZE_DEFAULT = 10;
const MAX_SIZE_DEFAULT = Number.MAX_SAFE_INTEGER;
const SHRINK_DEFAULT = true;
const CONNECTION_TIMEOUT_DEFAULT = 0;
const LOGIN_TIMEOUT_DEFAULT = 0;
const MAX_ACTIVELY_CONNECTING = 1;

// A queue for tracking the connections 
class ConnectionQueue
{
  static queuedConnections = [];
  static activelyConnectingCount = 0;
  static maxActivelyConnecting = MAX_ACTIVELY_CONNECTING;

  static enqueue = async function(pool, promiseGenerator, configObject)
  {
      ConnectionQueue.queuedConnections.push({
          pool,
          promiseGenerator,
          configObject
      });
      ConnectionQueue.dequeue();
      return;
  }

  static dequeue = async function()
  {
    if (this.activelyConnectingCount >= this.maxActivelyConnecting) {
      return;
    }
    const item = this.queuedConnections.shift();
    if (!item) {
      return;
    }
    ConnectionQueue.activelyConnectingCount++;
    try {
      await item.promiseGenerator(item.configObject);
    } catch (error) {
      // We don't actually do anything with errors here
    }
    ConnectionQueue.activelyConnectingCount--;
    ConnectionQueue.dequeue();
    return;
  }
}

class Pool {

  constructor(connectionString) {

    this.connectionConfig = {};
    this.waitingConnectionWork = [];
    this.isOpen = false;
    this.freeConnections = [];
    this.connectionsBeingCreatedCount = 0;
    this.poolSize = 0;

    // Keeps track of when connections have sucessfully connected
    this.connectionEmitter = new EventEmitter();

    // Fires when a connection has been made
    this.connectionEmitter.on('connected', (connection) => {

      // A connection has finished connecting, but there is some waiting call
      // to connect() that is waiting for a connection. shift() that work from
      // the front of the waitingConnectionWork queue, then either call the
      // callback function provided by the user, or resolve the Promise that was
      // returned to the user.
      if (this.waitingConnectionWork.length > 0)
      {
        let connectionWork = this.waitingConnectionWork.shift();

        // If the user passed a callback function, then call the function with
        // no error in the first parameter and the connection in the second
        // parameter
        if (typeof connectionWork == 'function')
        {
          return connectionWork(null, connection);
        // If the user didn't pass a callback function, we returned a promise to
        // them. Resolve that promise with the connection that was just created.
        } else {
          // Promise (stored resolve function)
          return connectionWork.resolveFunction(connection);
        }

      // A connection finished connecting, and there was no work waiting for
      // that connection to be made. Simply add it to the array of
      // freeConnections, and the next time work comes in it can simply be
      // retrieved from there.
      } else {
        this.freeConnections.push(connection);
      }
    });

    // connectionString is a string, so use defaults for all of the
    // configuration options.
    if (typeof connectionString === 'string') {
      this.connectionConfig.connectionString = connectionString;

      this.reuseConnections = REUSE_CONNECTIONS_DEFAULT;
      this.initialSize = INITIAL_SIZE_DEFAULT;
      this.incrementSize = INCREMENT_SIZE_DEFAULT;
      this.maxSize = MAX_SIZE_DEFAULT;
      this.shrink = SHRINK_DEFAULT;
      this.connectionConfig.connectionTimeout = CONNECTION_TIMEOUT_DEFAULT;
      this.connectionConfig.loginTimeout = LOGIN_TIMEOUT_DEFAULT;
    }
    // connectionString is an object, so ensure that connectionString is a
    // property on that object and then copy over any configuration options.
    else if (typeof connectionString === 'object')
    {
      const configObject = connectionString;
      if (!Object.prototype.hasOwnProperty.call(configObject, 'connectionString')) {
        throw new TypeError('Pool configuration object must contain "connectionString" key');
      }
      this.connectionConfig.connectionString = configObject.connectionString;

      // reuseConnections
      this.reuseConnections = configObject.reuseConnections !== undefined ? configObject.reuseConnections : REUSE_CONNECTIONS_DEFAULT;

      // initialSize
      this.initialSize = configObject.initialSize !== undefined ? configObject.initialSize : INITIAL_SIZE_DEFAULT;

      // incrementSize
      this.incrementSize = configObject.incrementSize !== undefined ? configObject.incrementSize : INCREMENT_SIZE_DEFAULT;

      // maxSize
      this.maxSize = configObject.maxSize !== undefined ? configObject.maxSize : MAX_SIZE_DEFAULT;

      // shrink
      this.shrink = configObject.shrink !== undefined ? configObject.shrink : SHRINK_DEFAULT;

      // connectionTimeout
      this.connectionConfig.connectionTimeout = configObject.connectionTimeout !== undefined ? configObject.connectionTimeout : CONNECTION_TIMEOUT_DEFAULT;

      // loginTimeout
      this.connectionConfig.loginTimeout = configObject.loginTimeout !== undefined ? configObject.loginTimeout : LOGIN_TIMEOUT_DEFAULT;

      // connectingQueueMax
      // unlike other configuration values, this one is set statically on the
      // ConnectionQueue object and not on the Pool intance
      if (configObject.maxActivelyConnecting !== undefined)
      {
        ConnectionQueue.maxActivelyConnecting = configObject.maxActivelyConnecting
      }
    }
    // connectionString was neither a string nor and object, so throw an error.
    else
    {
      throw TypeError('Pool constructor must passed a connection string or a configuration object');
    }
  }

  // returns a open connection, ready to use.
  // should overwrite the 'close' function of the connection, and rename it is 'nativeClose', so
  // that that close can still be called.
  async connect(callback = undefined) {

    let connection;

    if (this.freeConnections.length == 0) {

      // If the number of connections waiting is more (shouldn't happen) or
      // equal to the number of connections connecting, and the number of
      // connections in the pool, in the process of connecting, and that will be
      // added is less than the maximum number of allowable connections, then
      // we will need to create MORE connections.
      if (this.connectionsBeingCreatedCount <= this.waitingConnectionWork.length &&
          this.poolSize + this.connectionsBeingCreatedCount + this.incrementSize <= this.maxSize)
      {
        this.increasePoolSize(this.incrementSize);
      }

      // If no callback was provided when connect was called, we need to create
      // a promise to return back. We also need to save off the resolve function
      // of that promises callback, so that we can call it to resolve the
      // function we returned
      if (typeof callback == 'undefined') {
        let resolveConnectionPromise;

        const promise = new Promise((resolve, reject) => {
          resolveConnectionPromise = resolve;
        });
        const promiseObj = {
          promise: promise,
          resolveFunction: resolveConnectionPromise
        }
        // push the promise onto the waitingConnectionWork queue, then return
        // it to the user
        this.waitingConnectionWork.push(promiseObj);
        return promise;
      }
      // If a callback was provided, we can just add that to the
      // waitingConnectionWork queue, then return undefined to the user. Their
      // callback will execute when a connection is ready
      else
      {
        this.waitingConnectionWork.push(callback)
        return undefined;
      }
    }
    // Else, there was a free connection available for the user, so either
    // return an immediately resolved promise, or call their callback
    // immediately.
    else
    {
      connection = this.freeConnections.pop();

      // promise...
      if (typeof callback === 'undefined') {
        return Promise.resolve(connection);
      } else {
        // ...or callback
        return callback(null, connection);
      }
    }
  };

  async query(sql, params, cb) {
    // determine the parameters passed
    let callback = cb;
    let parameters = params;

    if (typeof callback === 'undefined') {
      if (typeof parameters === 'function') {
        callback = parameters;
        parameters = null;
      } else if (typeof parameters === 'undefined') {
        parameters = null;
      } // else parameters = params, check type in this.ODBCconnection.query
    }

    if (typeof callback !== 'function') {
      return new Promise((resolve, reject) => {
        this.connect((error, connection) => {
          if (error) {
            reject(error);
          }

          connection.query(sql, parameters, (error, result) => {
            if (error) {
              reject(error);
            } else {
              resolve(result);
            }
            connection.close();
          });
        });
      });
    }

    this.connect((error, connection) => {
      if (error) {
        throw error;
      }

      // ...or callback
      return connection.query(sql, parameters, (error, result) => {
        // after running, close the connection whether error or not
        process.nextTick(() => {
          callback(error, result);
        });
        connection.close();
      });
    });
  }

  // close the pool and all of the connections
  async close(callback = undefined) {
    const connections = [...this.freeConnections];
    this.freeConnections.length = 0;
    this.isOpen = false;

    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        async.each(connections, (connection, cb) => {
          connection.nativeClose((error) => {
            this.poolSize--;
            cb(error);
          });
        }, (error) => {
          if (error) {
            reject(error);
          } else {
            resolve(error);
          }
        });
      });
    }

    async.each(this.freeConnections, (connection, cb) => {
      connection.nativeClose((error) => {
        this.poolSize--;
        cb(error);
      });
    }, error => callback(error));
  }

  async init(callback = undefined) {
    if (!this.isOpen) {
      this.isOpen = true;
      // promise...
      if (typeof callback === 'undefined') {
        return new Promise(async (resolve, reject) => {
          try {
            await this.increasePoolSize(this.initialSize);
            resolve();
          } catch (error) {
            reject(error);
          }
        });
      }

      // ...or callback
      try {
        await this.increasePoolSize(this.initialSize);
      } catch (error) {
        return callback(error);
      }
      return callback(null);
    }
    return undefined;
  }

  generateConnectPromise = function(connectionConfig) {
    return new Promise((resolve, reject) => {
      odbc.connect(connectionConfig, (error, nativeConnection) => {
        if (error) {
          reject(error);
          return;
        }

        this.pool.connectionsBeingCreatedCount--;
        this.pool.poolSize++;
        let connection = new Connection(nativeConnection);
        connection.nativeClose = connection.close;

        if (this.pool.reuseConnections) {
          connection.close = async (closeCallback = undefined) => {
            this.pool.connectionEmitter.emit('connected', connection);

            if (typeof closeCallback === 'undefined') {
              return new Promise((resolve, reject) => {
                resolve();
              })
            }

            return closeCallback(null);
          };
        } else {
          connection.close = async (closeCallback = undefined) => {
            this.pool.increasePoolSize(1);
            if (typeof closeCallback === 'undefined') {
              return new Promise((resolve, reject) => {
                connection.nativeClose((error, result) => {
                  this.pool.poolSize--;
                  if (error) {
                    reject(error);
                  } else {
                    resolve(result);
                  }
                });
              });
            }
      
            connection.nativeClose(closeCallback);
          }
        }

        this.pool.connectionEmitter.emit('connected', connection);
        resolve();
      });
    });
  }

  // odbc.connect runs on an AsyncWorker, so this is truly non-blocking
  async increasePoolSize(count) {
    this.connectionsBeingCreatedCount += count;
    for (let i = 0; i < count; i++)
    {
      ConnectionQueue.enqueue(this, this.generateConnectPromise, this.connectionConfig);
    }
  }
}

module.exports.Pool = Pool;
