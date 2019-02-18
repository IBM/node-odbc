const { Connection } = require('../Connection');
const { ODBCResult } = require('./ODBCResult');

class ODBCConnection {
  constructor() {
    this.connection = new Connection();
    this.connected = false;
  }

  open(connectionString, callback) {
    this.connection.connect(connectionString, (error) => {
      if (error) return callback(error);

      this.connected = true;
      return callback(null);
    });
  }

  openSync(connectionString) {
    this.connection.connectSync(connectionString);
    return true;
  }

  close() {

  }

  closeSync() {

  }

  createStatement(callback) {
    this.connection.createStatement((error, statement) => callback(error, statement));
  }

  createStatementSync() {
    return this.connection.createStatementSync();
  }

  // 3 parameters:
  // * (sql, paramarray, callback)
  // 2 parameters:
  // * (sql, callback)
  // * (options, callback)
  query(sql, parameters, cb) {
    let sqlString = sql;
    let params = parameters;
    let callback = cb;
    let noResults = false;

    if (typeof callback === 'undefined') {
      callback = params;
      params = null;
    }

    if (typeof sqlString === 'object') {
      const paramObject = sqlString;
      sqlString = Object.prototype.hasOwnProperty.call(paramObject, 'sql') ? paramObject.sql : '';
      params = Object.prototype.hasOwnProperty.call(paramObject, 'params') ? paramObject.params : null;
      noResults = Object.prototype.hasOwnProperty.call(paramObject, 'noResults') ? paramObject.noResults : false;
    }

    this.connection.query(sqlString, params, (error, result) => {
      if (error) return callback(error, undefined);

      if (noResults === true) {
        return callback(error, true);
      }

      return callback(error, new ODBCResult(result));
    });
  }

  querySync(sql, parameters) {
    let sqlString = sql;
    let params = parameters;
    let noResults = false;

    if (typeof params === 'undefined') {
      params = null;
    }

    if (typeof sqlString === 'object') {
      const paramObject = sqlString;
      sqlString = Object.prototype.hasOwnProperty.call(paramObject, 'sql') ? paramObject.sql : '';
      params = Object.prototype.hasOwnProperty.call(paramObject, 'params') ? paramObject.params : null;
      noResults = Object.prototype.hasOwnProperty.call(paramObject, 'noResults') ? paramObject.noResults : false;
    }

    const result = this.connection.querySync(sqlString, params);
    if (noResults === true) {
      return true;
    }

    return new ODBCResult(result);
  }

  beginTransaction(callback) {
    return this.connection.beginTransaction(callback);
  }

  beginTransactionSync() {
    this.connection.beginTransactionSync();
  }

  endTransaction(callback) {
    return this.connection.endTransaction(callback);
  }

  endTransactionSync() {
    return this.connection.endTransactionSync();
  }

  getInfoSync(option) {
    return this.connection.getInfoSync(option);
  }

  columns(catalog, schema, table, column, callback) {
    return this.connection.columns(catalog, schema, table, column, callback);
  }

  tables(catalog, schema, table, type, callback) {
    return this.connection.tables(catalog, schema, table, type, callback);
  }
}

module.exports.ODBCConnection = ODBCConnection;
