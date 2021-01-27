class Cursor {
  /**
   * An open connection to the database made through an ODBC driver
   * @constructor
   * @param {string|object} connectionString - The connection string to connect using the DSN
   * defined in odbc.ini, or an odbcConnection object
   */
  constructor(odbcCursor) {
    this.odbcCursor = odbcCursor;
  }

  get noData() {
    return this.odbcCursor.noData;
  }

  // TODO: Write the documentation
  /**
   *
   * @param {*} cb
   */
  fetch(cb) {
    // accepted parameter signatures:
    // [empty]
    // cb

    let callback = cb;

    if (typeof callback !== 'function') {
      return new Promise((resolve, reject) => {
        this.odbcCursor.fetch((error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    }

    process.nextTick(() => {
      this.odbcCursor.fetch(callback);
    })
  }

  /** TODO:
   * @param {function} [callback] - Callback function. If not passed, a Promise will be returned.
   */
  close(callback = undefined) {

    // promise...
    if (typeof callback === 'function') {
      return new Promise((resolve, reject) => {
        this.odbcCursor.close((error) => {
          if (error) {
            reject(error);
          } else {
            resolve();
          }
        });
      });
    }

    // ...or callback
    return this.odbcCursor.close(callback);
  }
}

module.exports.Cursor = Cursor;
