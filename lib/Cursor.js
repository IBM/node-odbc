class Cursor {

  static CUSROR_CLOSED_ERROR = 'Cursor has already been closed!';

  /**
   * An cursor created following the execution of query that is ready to return a result set 
   * @constructor
   * @param {object} odbcCursor - an odbcCursor object, defined in src/odbc_cursor.h/.cpp
   */
  constructor(odbcCursor) {
    this.odbcCursor = odbcCursor;
  }

  /**
   * Return whether or not a call to SQLFetch has returned SQL_NO_DATA
   * @returns {boolean}
   *
   */
  get noData() {
    if (!this.odbcCursor)
    {
      throw new Error(CUSROR_CLOSED_ERROR);
    }
    return this.odbcCursor.noData;
  }

  /**
   * Calls SQL_FETCH and returns the next result set
   * @param {function} [callback] - The callback function to return an error and a result. If the callback is ommited, a Promise is returned.
   * @returns {undefined|Promise}
   */
  fetch(cb) {

    let callback = cb;

    if (typeof callback !== 'function') {
      if (!this.odbcCursor) {
        throw new Error(Cursor.CUSROR_CLOSED_ERROR);
      }
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

    if (!this.odbcCursor) {
      callback(new Error(Cursor.CUSROR_CLOSED_ERROR));
    } else {
      this.odbcCursor.fetch(callback);
    }
  }

  /**
   * Closes the cursor and the underlying SQLHSTMT, freeing all memory
   * @param {function} [callback] - The callback function to return an error. If the callback is ommited, a Promise is returned.
   * @returns {undefined|Promise}
   */
  close(callback = undefined) {

    // promise...
    if (typeof callback !== 'function') {
      if (!this.odbcCursor) {
        throw new Error(Cursor.CUSROR_CLOSED_ERROR);
      }
      return new Promise((resolve, reject) => {
        this.odbcCursor.close((error) => {
          if (error) {
            reject(error);
          } else {
            this.odbcCursor = null;
            resolve();
          }
        });
      });
    }

    // ...or callback
    this.odbcCursor.close((error) => {
      if (!error) {
        this.odbcCursor = null;
      }
      callback(error);
    });
  }
}

module.exports.Cursor = Cursor;
