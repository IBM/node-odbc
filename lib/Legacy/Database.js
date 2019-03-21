/*
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith <notwink@gmail.com>
  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.
  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

const odbc = require('../../build/Release/odbc.node');
const { SimpleQueue } = require('./SimpleQueue');
const { Connection } = require('../Connection');

class Database {
  constructor(options) {
    const config = options || {};

    if (odbc.loadODBCLibrary) {
      if (!options.library && !module.exports.library) {
        throw Error('You must specify a library when complied with dynodbc, otherwise this jams will segfault.');
      }

      if (!odbc.loadODBCLibrary(options.library || module.exports.library)) {
        throw Error('Could not load library. You may need to specify full path.');
      }
    }

    // this.odbc = (config.odbc) ? config.odbc : new odbc.ODBC();
    // this.odbc.domain = process.domain;
    this.queue = new SimpleQueue();
    this.fetchMode = config.fetchMode || null;
    this.connected = false;
    this.connectTimeout = Object.prototype.hasOwnProperty.call(config, 'connectTimeout')
      ? config.connectTimeout : null;
    this.loginTimeout = Object.prototype.hasOwnProperty.call(config, 'loginTimeout')
      ? config.connectTimeout : null;
  }

  open(connectionString, callback) {
    let connString = connectionString;
    if (typeof connString === 'object') {
      const obj = connString;
      connString = '';

      Object.keys(obj).forEach((key) => {
        connString += `${key} = ${obj[key]};`;
      });
    }


    odbc.connect(connString, 1, (connErr, conn) => {
      if (connErr) {
        return callback(connErr);
      }

      [this.conn] = conn;
      this.connected = true;
      this.conn.domain = process.domain;

      if (this.connectTimeout || this.connectTimeout === 0) {
        this.conn.connectTimeout = this.connectTimeout;
      }

      if (this.loginTimeout || this.loginTimeout === 0) {
        this.conn.loginTimeout = this.loginTimeout;
      }

      return callback(connErr, this.conn);
    });
  }

  openSync(connectionString) {
    let connString = connectionString;
    this.conn = odbc.connectSync(connString);

    if (this.connectTimeout || this.connectTimeout === 0) {
      this.conn.connectTimeout = this.connectTimeout;
    }

    if (this.loginTimeout || this.loginTimeout === 0) {
      this.conn.loginTimeout = this.loginTimeout;
    }

    if (typeof (connectionString) === 'object') {
      const obj = connectionString;
      connString = '';

      Object.keys(obj).forEach((key) => {
        connString += `${key}=${obj[key]};`;
      });
    }

    // const result = this.conn.openSync(connString);

    if (this.conn) {
      this.connected = true;
    }

    return this.conn;
  }

  close(cb) {
    this.queue.push((next) => {
      // check to see if conn still exists (it's deleted when closed)
      if (!this.conn) {
        if (cb) cb(null);
        return next();
      }

      this.conn.close((err) => {
        this.connected = false;
        delete this.conn;

        if (cb) cb(err);
        return next();
      });

      return undefined;
    });
  }

  closeSync() {
    const result = this.conn.closeSync();

    this.connected = false;
    delete this.conn;

    return result;
  }

  query(sql, params, cb) {
    let parameters = params;
    let callback = cb;

    if (typeof parameters === 'function') {
      callback = parameters;
      parameters = null;
    }

    if (!this.connected) {
      return callback({ message: 'Connection not open.' }, [], false);
    }

    const moreResultsWrapper = (err, result) => callback(err, result, false);

    if (parameters) {
      this.conn.query(sql, parameters, moreResultsWrapper);
    } else {
      this.conn.query(sql, null, moreResultsWrapper);
    }
    return undefined;
  }

  queryResult(sql, params, cb) {
    this.query(sql, params, cb);
  }

  queryResultSync(sql, params) {
    let result;

    if (!this.connected) {
      throw (Error('[node-odbc]: Connection not open'));
    }

    if (params) {
      result = this.conn.querySync(sql, params);
    } else {
      result = this.conn.querySync(sql);
    }

    return result;
  }

  querySync(sql, params) {
    let result;

    if (!this.connected) {
      throw (Error('[node-odbc]: Connection not open.'));
    }

    if (params) {
      result = this.conn.querySync(sql, params);
    } else {
      result = this.conn.querySync(sql, null);
    }

    return result;
  }

  beginTransaction(cb) {
    this.conn.beginTransaction(cb);
    return this;
  }

  endTransaction(rollback, cb) {
    this.conn.endTransaction(rollback, cb);
    return this;
  }

  commitTransaction(cb) {
    this.conn.endTransaction(false, cb); // don't rollback
    return this;
  }

  rollbackTransaction(cb) {
    this.conn.endTransaction(true, cb); // rollback
    return this;
  }

  beginTransactionSync() {
    this.conn.beginTransactionSync();
    return this;
  }

  endTransactionSync(rollback) {
    this.conn.endTransactionSync(rollback);
    return this;
  }

  commitTransactionSync() {
    this.conn.endTransactionSync(false); // don't rollback
    return this;
  }

  rollbackTransactionSync() {
    this.conn.endTransactionSync(true); // rollback
    return this;
  }

  columns(catalog, schema, table, column, callback) {
    if (!this.queue) this.queue = [];

    this.queue.push((next) => {
      this.conn.columns(catalog, schema, table, column, (err, result) => {
        callback(err, result);
        return next();
      });
      return undefined;
    });
  }

  tables(catalog, schema, table, type, callback) {
    if (!this.queue) this.queue = [];

    this.queue.push((next) => {
      this.conn.tables(catalog, schema, table, type, (err, result) => {
        callback(err, result);
        return next();
      });
      return undefined;
    });
  }

  describe(options, callback) {
    const config = options;

    if (typeof callback !== 'function') {
      throw (TypeError('[node-odbc] Missing Arguments: You must specify a callback function in order for the describe method to work.'));
    }

    if (typeof config !== 'object') {
      callback(TypeError('[node-odbc] Missing Arguments: You must pass an object as argument 0 if you want anything productive to happen in the describe method.'), null);
      return undefined;
    }

    if (!config.database) {
      callback(TypeError('[node-odbc] Missing Arguments: The object you passed did not contain a database property. This is required for the describe method to work.'), null);
      return undefined;
    }

    // set some defaults if they weren't passed
    config.schema = config.schema || '%';
    config.type = config.type || 'table';

    if (config.table && config.column) {
      // get the column details
      this.columns(config.database, config.schema, config.table, config.column, callback);
    } else if (config.table) {
      // get the columns in the table
      this.columns(config.database, config.schema, config.table, '%', callback);
    } else {
      // get the tables in the database
      this.tables(config.database, config.schema, null, config.type || 'table', callback);
    }

    return undefined;
  }

  prepare(sql, cb) {
    this.conn.createStatement((stmtErr, stmt) => {
      if (stmtErr) return cb(stmtErr);
      const statement = stmt;
      statement.queue = new SimpleQueue();

      statement.prepare(sql, (prepareErr) => {
        if (prepareErr) return cb(prepareErr);
        return cb(null, stmt);
      });
      return undefined;
    });
  }

  prepareSync(sql) {
    const stmt = this.conn.createStatementSync();
    stmt.queue = new SimpleQueue();
    stmt.prepareSync(sql);
    return stmt;
  }
}

module.exports.debug = false;
module.exports.Database = Database;
