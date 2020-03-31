class Statement {
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
    return this.odbcStatement.prepare(sql, callback);
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
    return this.odbcStatement.bind(parameters, callback);
  }

  /**
   * Executes the prepared SQL statement template with the bound parameters, returning the result.
   * @param {function} [callback] - The callback function that returns the result. If omitted, uses a Promise.
   */
  execute(callback = undefined) {
    if (typeof callback !== 'function' && typeof callback !== 'undefined') {
      throw new TypeError('[node-odbc]: Incorrect function signature for call to statement.execute({function}[optional]).');
    }

    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        this.odbcStatement.execute((error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    return this.odbcStatement.execute(callback);
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
      return new Promise((resolve, reject) => {
        this.odbcStatement.close((error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    // ...or callback
    return this.odbcStatement.close(callback);
  }
}

module.exports.Statement = Statement;
