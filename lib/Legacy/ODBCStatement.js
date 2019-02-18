const { Statement } = require('../Statement');
const { ODBCResult } = require('./ODBCResult');

class ODBCStatement {
  constructor() {
    this.statement = new Statement();
  }

  execute(callback) {
    this.statement.execute((error, result) => {
      if (error) return callback(error, null);

      return callback(error, new ODBCResult(result));
    });
  }

  executeSync() {
    const result = this.statement.executeSync();
    return new ODBCResult(result);
  }

  executeDirect(sql, callback) {
    this.statement.connection.query(sql, (error, result) => {
      if (error) return callback(error, null);

      return callback(null, new ODBCResult(result));
    });
  }

  executeDirectSync(sql) {
    const result = this.statement.execDirectSync(sql);
    return new ODBCResult(result);
  }

  executeNonQuery(callback) {
    this.statement.execute((error, result) => {
      if (error) return callback(error, null);

      return callback(error, result.count);
    });
  }

  executeNonQuerySync() {
    const result = this.statement.executeSync();
    return result.count;
  }

  prepare(sql, callback) {
    this.statement.prepare(error => callback(error));
  }

  prepareSync(sql) {
    this.statement.prepare(sql);
  }

  bind(parameters, callback) {
    this.statement.bind(parameters, callback);
  }

  bindSync(parameters) {
    this.statement.bindSync(parameters);
  }

  closeSync() {
    this.statement.closeSync();
  }
}

module.exports.ODBCStatement = ODBCStatement;
