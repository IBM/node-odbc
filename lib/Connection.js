const odbc = require('../build/Release/odbc.node');
const { Statement } = require('./Statement');

class Connection {
  /**
   * An open connection to the database made through an ODBC driver
   * @constructor
   * @param {string|object} connectionString - The connection string to connect using the DSN
   * defined in odbc.ini, or an odbcConnection object
   */
  constructor(connectionString, debug = false) {
    if (typeof connectionString === 'string') {
      this.odbcConnection = odbc.connectSync(connectionString);
    } else {
      this.odbcConnection = connectionString;
    }
    this.debug = debug;
  }

  /**
   * Get the value of the passed attribute from the connection. Asynchronous, can be used either
   * with a callback function or a Promise.
   * @param {number} attribute - The attribute identifier.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  async getAttribute(attribute, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.getAttribute(attribute, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.getAttribute(attribute, callback);
    return undefined;
  }

  /**
   * Set the value of the passed attribute on the connection. Asynchronous, can be used either with
   * a callback function or a Promise.
   * @param {string} attribute - The title of the book.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  setAttribute(attribute, value, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.setAttribute(attribute, value, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.setAttribute(attribute, value, callback);
    return undefined;
  }

  async query(sql, params, cb) {
    // accepted parameter signatures:
    // sql
    // sql, params
    // sql, callback
    // sql, params, callback

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
        this.odbcConnection.query(sql, parameters, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    this.odbcConnection.query(sql, parameters, callback);
    return undefined;
  }

  querySync(sql, params) {
    return this.odbcConnection.querySync(sql, params);
  }

  createStatement() {
    return new Statement(this);
  }

  // shortcut method for getting a statement by calling prepare on the connection
  prepare(sql, callback) {
    const statement = new Statement(this);
    statement.prepare(sql, (error, result) => {
      if (error && result === true) {
        return callback(error);
      }

      // return the statment instead of true/false
      return callback(error, statement);
    });
  }

  // shortcut method for getting a statement by calling prepareSync on the connection
  prepareSync(sql) {
    const statement = new Statement(this);
    statement.prepare(sql);
    return statement;
  }

  // // idb-connector
  // // TODO:
  // conn(database, user, password, callback) {
  //   this.odbcConnection;
  // }

  // // idb-connector
  // // TODO:
  // disconn() {
  //   this.odbcConnection;
  // }

  /** TODO:
   * Get the value of the passed attribute from the connection. Asynchronous, can be used either
   * with a callback function or a Promise.
   * @param {string} attribute - The title of the book.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  close(callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.close((error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.close(callback);
    return undefined;
  }

  /** TODO:
   * Get the value of the passed attribute from the connection. Asynchronous, can be used either
   * with a callback function or a Promise.
   * @param {string} attribute - The title of the book.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  closeSync() {
    return this.odbcConnection.closeSync();
  }

  getInfo(option, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.getInfo(option, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcConnection.getInfo(option, callback);
    return undefined;
  }

  getInfoSync(option) {
    return this.odbcConnection.getInfoSync(option);
  }

  columns(catalog, schema, table, type, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.columns(catalog, schema, table, type, (error, result) => {
          if (error) {
            reject(new Error(error));
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

  columnsSync(catalog, schema, table, type) {
    return this.odbcConnection.columnsSync(catalog, schema, table, type);
  }

  tables(catalog, schema, table, type, callback = undefined) {
    // promise...
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.tables(catalog, schema, table, type, (error, result) => {
          if (error) {
            reject(new Error(error));
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
            reject(new Error(error));
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
        this.odbcConnection.endTransaction(odbc.SQL_COMMIT, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    return this.odbcConnection.endTransaction(odbc.SQL_COMMIT, callback);
  }

  /**
   * Asynchronously ends the transaction with a rollback.
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  rollback(callback = undefined) {
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.odbcConnection.endTransaction(odbc.SQL_ROLLBACK, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    return this.odbcConnection.endTransaction(odbc.SQL_ROLLBACK, callback);
  }
}

module.exports.Connection = Connection;
