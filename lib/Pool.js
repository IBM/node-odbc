const EventEmitter = require('events');
const async = require('async');
const binary = require('node-pre-gyp');
const path = require('path');

const { Connection } = require('./Connection');

const bindingPath = binary.find(path.resolve(path.join(__dirname, '../package.json')));

const odbc = require(bindingPath);

const REUSE_CONNECTIONS_DEFAULT = true;
const INITIAL_SIZE_DEFAULT = 10;
const INCREMENT_SIZE_DEFAULT = 10;
const MAX_SIZE_DEFAULT = null;
const SHRINK_DEFAULT = true;
const CONNECTION_TIMEOUT_DEFAULT = 0;
const LOGIN_TIMEOUT_DEFAULT = 0;

class Pool {

  constructor(connectionString) {

    this.connectionEmitter = new EventEmitter();
    this.connectionEmitter.on('connected', (connection) => {
      if (this.connectionQueue.length > 0)
      {
        let connectionWork = this.connectionQueue.pop();
        if (typeof connectionWork == 'function')
        {
          // callback function
          return connectionWork(null, connection);
        } else {
          // Promise (stored resolve function)
          return connectionWork.resolveFunction(connection);
        }
      } else {
        this.freeConnections.push(connection);
      }
    });

    this.isOpen = false;
    this.freeConnections = [];
    this.connectingCount = 0;
    this.poolSize = 0;
    this.connectionQueue = [];

    // connectionString is a...
    if (typeof connectionString === 'string') {
      this.connectionString = connectionString;

      this.reuseConnections = REUSE_CONNECTIONS_DEFAULT;
      this.initialSize = INITIAL_SIZE_DEFAULT;
      this.incrementSize = INCREMENT_SIZE_DEFAULT;
      this.maxSize = MAX_SIZE_DEFAULT;
      this.shrink = SHRINK_DEFAULT;
      this.connectionTimeout = CONNECTION_TIMEOUT_DEFAULT;
      this.loginTimeout = LOGIN_TIMEOUT_DEFAULT;

    } else if (typeof connectionString === 'object') {
      const configObject = connectionString;
      if (!Object.prototype.hasOwnProperty.call(configObject, 'connectionString')) {
        throw new TypeError('Pool configuration object must contain "connectionString" key');
      }
      this.connectionString = configObject.connectionString;

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
      this.connectionTimeout = configObject.connectionTimeout !== undefined ? configObject.connectionTimeout : CONNECTION_TIMEOUT_DEFAULT;

      // loginTimeout
      this.loginTimeout = configObject.loginTimeout !== undefined ? configObject.loginTimeout : LOGIN_TIMEOUT_DEFAULT;
    } else {
      throw TypeError('Pool constructor must passed a connection string or a configuration object');
    }
  }


  // TODO: Documentation
  // TODO: Does this need to be async?
  // returns a open connection, ready to use.
  // should overwrite the 'close' function of the connection, and rename it is 'nativeClose', so
  // that that close can still be called.
  async connect(callback = undefined) {

    let connection;

    if (this.freeConnections.length == 0) {

      // If the number of connections waiting is more (shouldn't happen) or
      // equal to the number of connections connecting, then we will need to
      // create MORE connections.
      if (this.connectingCount <= this.connectionQueue.length &&
          this.poolSize < this.maxSize)
      {
        this.increasePoolSize(this.incrementSize);
      }

      if (typeof callback == 'undefined') {
        let resolveConnectionPromise;

        const promise = new Promise((resolve, reject) => {
          resolveConnectionPromise = resolve;
        });
        const promiseObj = {
          promise: promise,
          resolveFunction: resolveConnectionPromise
        }
        this.connectionQueue.unshift(promiseObj);
        return promise;
      } else {
        this.connectionQueue.unshift(callback)
        return undefined;
      }
    } else {
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

  // odbc.connect runs on an AsyncWorker, so this is truly non-blocking
  async increasePoolSize(count) {
    this.connectingCount += count;
    this.poolSize += count;
    const connectionConfig = {
      connectionString: this.connectionString,
      connectionTimeout: this.connectionTimeout,
      loginTimeout: this.loginTimeout,
    };
    return new Promise(async (resolve, reject) => {
      const connectArray = [];
      for (let i = 0; i < count; i += 1) {
        let promise = new Promise((resolve, reject) => {
          odbc.connect(connectionConfig, (error, nativeConnection) => {
            let connection = undefined;
            if (error) {
              reject(error);
              return;
            }

            connection = new Connection(nativeConnection);
            connection.nativeClose = connection.close;

            if (this.reuseConnections) {
              connection.close = async (closeCallback = undefined) => {
                this.connectionEmitter.emit('connected', connection);

                if (typeof closeCallback === 'undefined') {
                  return new Promise((resolve, reject) => {
                    resolve();
                  })
                }

                return closeCallback(null);
              };
            } else {
              connection.close = async (closeCallback = undefined) => {
                this.increasePoolSize(1);
                if (typeof closeCallback === 'undefined') {
                  return new Promise((resolve, reject) => {
                    connection.nativeClose((error, result) => {
                      this.poolSize--;
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

            this.connectingCount--;
            this.connectionEmitter.emit('connected', connection);
            resolve();
          });
        });
        connectArray.push(promise);
      }
      if (connectArray.length > 0)
      {
        await Promise.race(connectArray);
        resolve();
      } else {
        resolve();
      }
    });
  }
}

module.exports.Pool = Pool;
