const { Cursor } = require('./Cursor');

class Statement {

  STATEMENT_CLOSED_ERROR = 'Statement has already been closed!';

  constructor(odbcStatement) {
    this.odbcStatement = odbcStatement;
  }

  /**
   * Prepare an SQL statement template to which parameters can be bound and the statement then executed.
   * @param {string} sql - The SQL statement template to prepare, with or without unspecified parameters.
   * @param {function} [callback] - The callback function that returns the result. If omitted, uses a Promise.
   * @returns {undefined|Promise}
   */
  prepare(sql, callback = undefined) {
    if (typeof sql !== 'string'
    || (typeof callback !== 'function' && typeof callback !== 'undefined')) {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to statement.prepare({string}, {function}[optional]).');
    }

    // promise...
    if (typeof callback === 'undefined') {
      if (!this.odbcStatement)
      {
        throw new Error(Statement.STATEMENT_CLOSED_ERROR);
      }

      return new Promise((resolve, reject) => {
        this.odbcStatement.prepare(sql, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    if (!this.odbcStatement)
    {
      callback(new Error(Statement.STATEMENT_CLOSED_ERROR));
    } else {
      this.odbcStatement.prepare(sql, callback);
    }
  }

  /**
   * Bind parameters on the previously prepared SQL statement template.
   * @param {*[]} parameters - The parameters to bind to the previously prepared SQL statement.
   * @param {function} [callback] - The callback function that returns the result. If omitted, uses a Promise.
   * @return {undefined|Promise}
   */
  bind(parameters, callback = undefined) {
    if (!Array.isArray(parameters)
    || (typeof callback !== 'function' && typeof callback !== 'undefined')) {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to statement.bind({array}, {function}[optional]).');
    }

    // promise...
    if (typeof callback === 'undefined') {
      if (!this.odbcStatement)
      {
        throw new Error(Statement.STATEMENT_CLOSED_ERROR);
      }

      return new Promise((resolve, reject) => {
        this.odbcStatement.bind(parameters, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    if (!this.odbcStatement)
    {
      callback(new Error(Statement.STATEMENT_CLOSED_ERROR));
    } else {
      this.odbcStatement.bind(parameters, callback);
    }
  }

  /**
   * Executes the prepared SQL statement template with the bound parameters, returning the result.
   * @param {function} [callback] - The callback function that returns the result. If omitted, uses a Promise.
   */
  execute(options, callback = undefined) {

    if (options === undefined)
    {
      options = null;
    }

    if (typeof options === 'function' && callback === undefined) {
      callback = options;
      options = null
    }

    if ((typeof callback !== 'function' && typeof callback !== 'undefined') 
    || typeof options !== 'object' && typeof options !== 'undefined' ) {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to statement.execute({object}[optional], {function}[optional]).');
    }

    // Promise...
    if (typeof callback === 'undefined') {
      if (!this.odbcStatement)
      {
        throw new Error(Statement.STATEMENT_CLOSED_ERROR);
      }

      return new Promise((resolve, reject) => {
        this.odbcStatement.execute(options, (error, result) => {
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

    // ...or callback
    if (!this.odbcStatement)
    {
      callback(new Error(Statement.STATEMENT_CLOSED_ERROR));
    } else {
      process.nextTick(() => {
        if (options &&
          (
            options.hasOwnProperty('fetchSize') ||
            options.hasOwnProperty('cursor')
          )
        )
        {
          this.odbcStatement.execute(options, (error, result) => {
            if (error) {
              return callback(error);
            }
  
            const cursor = new Cursor(result);
            return callback(error, cursor);
          });
        }
        else
        {
          this.odbcStatement.execute(options, callback);
        }
      });
    }
  }

  /**
   * Closes the statement, deleting the prepared statement and freeing the handle, making further
   * calls on the object invalid.
   * @param {function} [callback] - The callback function that returns the result. If omitted, uses a Promise. 
   */
  close(callback = undefined) {
    if (typeof callback !== 'function' && typeof callback !== 'undefined') {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to statement.close({function}[optional]).');
    }

    if (typeof callback === 'undefined') {
      if (!this.odbcStatement) {
        throw new Error(Statement.STATEMENT_CLOSED_ERROR);
      }
      return new Promise((resolve, reject) => {
        this.odbcStatement.close((error) => {
          if (error) {
            reject(error);
          } else {
            this.odbcStatement = null;
            resolve();
          }
        });
      });
    }

    // ...or callback
    return this.odbcStatement.close((error) => {
      if (!error) {
        this.odbcStatement = null;
      }
      callback(error);
    });
  }
}

module.exports.Statement = Statement;
