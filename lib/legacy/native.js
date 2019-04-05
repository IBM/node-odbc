const odbc = require('../../build/Release/odbc.node');
const SimpleQueue = require('./SimpleQueue');

// Proxy all of the asynchronous functions so that they are queued
odbc.ODBCStatement.prototype.nativeExecute = odbc.ODBCStatement.prototype.execute;
odbc.ODBCStatement.prototype.nativeExecuteDirect = odbc.ODBCStatement.prototype.executeDirect;
odbc.ODBCStatement.prototype.nativeExecuteNonQuery = odbc.ODBCStatement.prototype.executeNonQuery;
odbc.ODBCStatement.prototype.nativePrepare = odbc.ODBCStatement.prototype.prepare;
odbc.ODBCStatement.prototype.nativeBind = odbc.ODBCStatement.prototype.bind;

odbc.ODBCStatement.prototype.execute = (params, cb) => {
  let parameters = params;
  let callback = cb;

  this.queue = this.queue || new SimpleQueue();

  if (!cb) {
    callback = parameters;
    parameters = null;
  }

  this.queue.push((next) => {
    // If params were passed to this function, then bind them and
    // then execute.
    if (params) {
      this.nativeBind(params, (bindErr) => {
        if (bindErr) {
          return callback(bindErr);
        }

        this.nativeExecute((executeErr, result) => {
          callback(executeErr, result);

          return next();
        });
        return undefined;
      });
    } else { // Otherwise execute and pop the next bind call
      this.nativeExecute((err, result) => {
        callback(err, result);

        // NOTE: We only execute the next queued bind call after
        // we have called execute() or executeNonQuery(). This ensures
        // that we don't call a bind() a bunch of times without ever
        // actually executing that bind. Not
        if (this.bindQueue) this.bindQueue.next();
        return next();
      });
    }
  });
};

odbc.ODBCStatement.prototype.executeDirect = (sql, cb) => {
  this.queue = this.queue || new SimpleQueue();

  this.queue.push((next) => {
    this.nativeExecuteDirect(sql, (err, result) => {
      cb(err, result);

      return next();
    });
  });
};

odbc.ODBCStatement.prototype.executeNonQuery = (params, cb) => {
  let parameters = params;
  let callback = cb;

  this.queue = this.queue || new SimpleQueue();

  if (!callback) {
    callback = parameters;
    parameters = null;
  }

  this.queue.push((next) => {
    // If params were passed to this function, then bind them and
    // then executeNonQuery.
    if (params) {
      this.nativeBind(params, (bindErr) => {
        if (bindErr) {
          return callback(bindErr);
        }

        this.nativeExecuteNonQuery((err, result) => {
          callback(err, result);
          return next();
        });

        return undefined;
      });
    } else { // Otherwise executeNonQuery and pop the next bind call
      this.nativeExecuteNonQuery((err, result) => {
        callback(err, result);

        // NOTE: We only execute the next queued bind call after
        // we have called execute() or executeNonQuery(). This ensures
        // that we don't call a bind() a bunch of times without ever
        // actually executing that bind. Not
        if (this.bindQueue) this.bindQueue.next();

        return next();
      });
    }
  });
};

odbc.ODBCStatement.prototype.prepare = (sql, cb) => {
  this.queue = this.queue || new SimpleQueue();

  this.queue.push((next) => {
    this.nativePrepare(sql, (err) => {
      cb(err);

      return next();
    });
  });
};

odbc.ODBCStatement.prototype.bind = (ary, cb) => {
  this.bindQueue = this.bindQueue || new SimpleQueue();

  this.bindQueue.push(() => {
    this.nativeBind(ary, (err) => {
      cb(err);

      // NOTE: we do not call next() here because
      // we want to pop the next bind call only
      // after the next execute call
    });
  });
};


// proxy the ODBCResult fetch function so that it is queued
odbc.ODBCResult.prototype.nativeFetch = odbc.ODBCResult.prototype.fetch;

odbc.ODBCResult.prototype.fetch = (cb) => {
  this.queue = this.queue || new SimpleQueue();

  this.queue.push((next) => {
    this.nativeFetch((err, data) => {
      if (cb) cb(err, data);

      return next();
    });
  });
};
