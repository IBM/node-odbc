const odbc = require('../build/Release/odbc.node');
const { Connection } = require('./Connection');

// TODO: Have to add options:
// increase Pool size or no
// max pool size?
// starting pool size
// increment size
class Pool {
  constructor(connectionString, initialSize = 10) {
    this.isInitialized = false;
    this.freeConnections = [];

    if (typeof connectionString === 'object') {
      const configObject = connectionString;
      this.connectionString = configObject.connectionString;
      if (Object.prototype.hasOwnProperty.call(configObject, 'connectionString')) {
        this.connectionString = configObject.connectionString;
      } else {
        throw new Error('Pool configuration object must contain "connectionString" key');
      }

      this.initialSize = configObject.initialSize || 10;
      this.incrementSize = configObject.incrementSize || configObject.initialSize || 10;
      this.connectionTimeout = configObject.connectionTimeout || 1000;
      this.idleTimeout = configObject.idleTimeout || 1000;
      this.maxSize = configObject.maxSize || 64;
      this.shrink = configObject.shrink || true;
    } else {
      this.connectionString = connectionString;
      this.initialSize = initialSize;
      this.incrementSize = initialSize;
      this.maxSize = 100;
      this.shrink = true;
    }
  }

  async init(callback = undefined) {
    if (!this.isInitialized) {
      // promise...
      if (typeof callback === 'undefined') {
        return new Promise(async (resolve, reject) => {
          try {
            await this.increasePoolSize(this.initialSize);
            this.isInitialized = true;
            resolve(null);
          } catch (error) {
            reject(new Error(error));
          }
        });
      }

      // ...or callback
      try {
        await this.increasePoolSize(this.initialSize);
        this.isInitialized = true;
      } catch (error) {
        return callback(error);
      }
      return callback(null);
    }

    console.log('.init() was called, but the Pool was already initialized.');
    return undefined;
  }

  // TODO: Documentation
  // TODO: Does this need to be async?
  // returns a open connection, ready to use.
  // should overwrite the 'close' function of the connection, and rename it is 'nativeClose', so
  // that that close can still be called.
  async connect(callback = undefined) {
    if (this.freeConnections.length < 2) {
      await this.increasePoolSize(this.incrementSize);
    }

    const connection = this.freeConnections.pop();

    connection.nativeClose = connection.close;

    connection.close = async (closeCallback = undefined) => {
      if (typeof closeCallback === 'undefined') {
        return new Promise((resolve, reject) => {
          connection.close((error, result) => {
            if (error) {
              reject(new Error(error));
            } else {
              resolve(result);
            }
          });

          if (this.shrink === false || this.freeConnections.length < this.initialSize) {
            this.increasePoolSize(1);
          }
        });
      }

      connection.nativeClose(closeCallback);

      if (this.shrink === false || this.freeConnections.length < this.initialSize) {
        try {
          this.increasePoolSize(1);
        } catch (error1) {
          console.error(error1);
        }
      }

      return undefined;
    };

    // promise...
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        if (connection == null) {
          reject();
        } else {
          resolve(connection);
        }
      });
    }

    // ...or callback
    return callback(null, connection);
  }

  query(sql, params, cb) {
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

    const connection = this.freeConnections.pop();

    // promise...
    if (typeof callback !== 'function') {
      return new Promise((resolve, reject) => {
        connection.query(sql, parameters, (error, result) => {
          // after running, close the connection whether error or not
          connection.close((closeError) => {
            this.freeConnections.push(new Connection(this.connectionString));
            if (closeError) {
              // TODO:: throw and error
            }
          });

          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    return connection.query(sql, parameters, (error, result) => {
      // after running, close the connection whether error or not
      connection.close((closeError) => {
        this.freeConnections.push(new Connection(this.connectionString));
        if (closeError) {
          // TODO:: throw an error
        }
      });
      callback(error, result);
    });
  }

  // These are PRIVATE (although can't be made private in ES6... So don't use these)!

  // odbc.connect runs on an AsyncWorker, so this is truly non-blocking
  async increasePoolSize(count) {
    return new Promise((resolve, reject) => {
      Pool.generateConnections(this.connectionString, count, (error, connections) => {
        if (error) {
          reject(new Error(error));
        } else {
          this.freeConnections = [...this.freeConnections, ...connections];
          resolve();
        }
      });
    });
  }

  static async generateConnections(connectionString, count, callback) {
    // promise...
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        odbc.connect(connectionString, count, (error, odbcConnections) => {
          if (error) {
            reject(new Error(error));
          } else {
            const connections = Pool.wrapConnections(odbcConnections);
            resolve(connections);
          }
        });
      });
    }

    // or callback
    return odbc.connect(connectionString, count, (error, odbcConnections) => {
      if (error) {
        callback(error, null);
        return undefined;
      }

      const connections = Pool.wrapConnections(odbcConnections);
      callback(null, connections);
      return undefined;
    });
  }

  static wrapConnections(odbcConnections) {
    const connectionsArray = [];
    odbcConnections.forEach((odbcConnection) => {
      connectionsArray.push(new Connection(odbcConnection));
    });
    return connectionsArray;
  }
}

module.exports.Pool = Pool;
