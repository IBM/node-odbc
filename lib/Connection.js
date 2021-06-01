const { Statement } = require('./Statement');
const { Cursor }    = require('./Cursor');

class Connection {
  /**
   * An open connection to the database made through an ODBC driver
   * @constructor
   * @param {string|object} connectionString - The connection string to connect using the DSN
   * defined in odbc.ini, or an odbcConnection object
   */
  constructor(odbcConnection) {
    this.odbcConnection = odbcConnection;
  }

  get connected() {
    return this.odbcConnection.connected;
  }

  get autocommit() {
    return this.odbcConnection.autocommit;
  }

  // TODO: Write the documentation
  /**
   *
   * @param {*} sql
   * @param {*} params
   * @param {*} cb
   */
  query(sql, params, opts, cb) {
    // accepted parameter signatures:
    // sql
    // sql, params
    // sql, opts
    // sql, params, opts
    // sql, cb
    // sql, params, cb
    // sql, opts, cb
    // sql, params, opts, cb

    let callback = cb;
    let parameters = params;
    let options = opts;

    // If callback is undefined, search for a function in another position
    if (typeof callback === 'undefined')
    {
      if (typeof options === 'function')
      {
        callback = options;
        options = undefined;
      }
      else if(typeof parameters === 'function')
      {
        callback = parameters;
        options = undefined;
        parameters = undefined;
      }
    }

    if (typeof options === 'undefined')
    {
      if (typeof parameters === 'object' && parameters !== null && !Array.isArray(parameters))
      {
        options = parameters;
        parameters = null;
      } else {
        options = null;
      }
    }

    // if explicitly passing undefined into parameters, need to change to null
    if (typeof parameters === 'undefined') {
      parameters = null;
    }

    if (
      typeof sql !== 'string' ||
      (parameters !== null && !Array.isArray(parameters)) ||
      (options !== null && typeof options !== 'object') ||
      (typeof callback !== 'function' && typeof callback !== 'undefined')
    ) 
    {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to connection.query({string}, {array}[optional], {object}[optional], {function}[optional]).');
    }

    if (typeof callback !== 'function') {
      return new Promise((resolve, reject) => {
        this.odbcConnection.query(sql, parameters, options, (error, result) => {
          if (error) {
            reject(error);
          } else {
            if (options && 
              (
                options.hasOwnProperty('fetchSize') || 
                options.hasOwnProperty('cursor')
              )
            )
            {
              const cursor = new Cursor(result);
              resolve(cursor);
            }
            else
            {
              resolve(result);
            }
          }
        });
      });
    }

    process.nextTick(() => {
      if (options &&
        (
          options.hasOwnProperty('fetchSize') ||
          options.hasOwnProperty('cursor')
        )
      )
      {
        this.odbcConnection.query(sql, parameters, options, (error, result) => {
          if (error) {
            return callback(error);
          }

          const cursor = new Cursor(result);
          return callback(error, cursor);
        });
      }
      else
      {
        this.odbcConnection.query(sql, parameters, options, callback);
      }
    })
  }

  /**
   *
   * @param {string} name
   * @param {Array} parameters
   * @param {function} [cb]
   */
  callProcedure(catalog, schema, name, params = undefined, cb = undefined) {
    // name
    // name, params
    // name, cb
    // name, params, cb

    let callback = cb;
    let parameters = params;

    if (typeof callback === 'undefined') {
      if (typeof parameters === 'function') {
        callback = parameters;
        parameters = null;
      } else if (typeof parameters === 'undefined') {
        parameters = null;
      }
    }

    // if explicitly passing undefined into parameters, need to change to null
    if (typeof parameters === 'undefined') {
      parameters = null;
    }

    if (typeof name !== 'string'
    || (parameters !== null && !Array.isArray(parameters))
    || (typeof callback !== 'function' && typeof callback !== 'undefined')) {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to connection.query({string}, {array}[optional], {function}[optional]).');
    }

    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.callProcedure(catalog, schema, name, parameters, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    return this.odbcConnection.callProcedure(catalog, schema, name, parameters, callback);
  }

  // TODO: Write the documentation
  /**
   *
   * @param {*} callback
   */
  createStatement(callback = undefined) {
    // type-checking
    if (typeof callback !== 'function' && typeof callback !== 'undefined') {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to connection.createStatement({function}[optional]).');
    }

    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.createStatement((error, odbcStatement) => {
          if (error) {
            reject(error);
          } else {
            const statement = new Statement(odbcStatement);
            resolve(statement);
          }
        });
      });
    }


    // ...or callback
    return this.odbcConnection.createStatement((error, odbcStatement) => {
      if (error) { return callback(error, null); }

      const statement = new Statement(odbcStatement);
      return callback(null, statement);
    });
  }

  /** TODO:
   * Get the value of the passed attribute from the connection. Asynchronous, can be used either
   * with a callback function or a Promise.
   * @param {string} attribute - The title of the book.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  close(callback = undefined) {
    // type-checking
    if (typeof callback !== 'function' && typeof callback !== 'undefined') {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to connection.close({function}[optional]).');
    }

    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.close((error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    // ...or callback
    return this.odbcConnection.close(callback);
  }

  // TODO: Documentation
  columns(catalog, schema, table, type, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.columns(catalog, schema, table, type, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.columns(catalog, schema, table, type, callback);
    return undefined;
  }

  // TODO: Documentation
  tables(catalog, schema, table, type, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.tables(catalog, schema, table, type, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.tables(catalog, schema, table, type, callback);
    return undefined;
  }

  // TODO: Documentation
  setIsolationLevel(isolationLevel, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.setIsolationLevel(isolationLevel, (error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    // ...or callback
    return this.odbcConnection.setIsolationLevel(isolationLevel, callback);
  }

  /**
   * Begins a transaction, turning off auto-commit. Transaction is ended with commit() or
   * rollback().
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  beginTransaction(callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.beginTransaction((error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    return this.odbcConnection.beginTransaction(callback);
  }

  /**
   * Asynchronously ends the transaction with a commit.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  commit(callback = undefined) {
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.commit((error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    return this.odbcConnection.commit(callback);
  }

  /**
   * Asynchronously ends the transaction with a rollback.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  rollback(callback = undefined) {
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.rollback((error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    return this.odbcConnection.rollback(callback);
  }
}

module.exports.Connection = Connection;
