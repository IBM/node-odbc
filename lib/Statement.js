class Statement {
  constructor(connection) {
    this.odbcStatement = connection.odbcConnection.getStatement();
  }

  // PREPARE
  prepare(sql, callback) {
    // promise...
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        this.odbcStatement.prepare(sql, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcStatement.prepare(sql, callback);
    return undefined;
  }

  prepareSync(sql) {
    this.odbcStatement.prepareSync(sql);
  }

  // BIND
  bind(params, callback) {
    // promise...
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        this.odbcStatement.bind(params, (error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcStatement.bind(params, callback);
    return undefined;
  }

  bindSync(params) {
    return this.odbcStatement.bindSync(params);
  }

  // EXECUTE
  execute(callback) {
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        this.odbcStatement.execute((error, result) => {
          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    this.odbcStatement.execute(callback);
    return undefined;
  }

  executeSync(callback) {
    if (typeof callback === 'undefined') {
      return this.odbcStatement.executeSync();
    }

    return undefined;
  }
}

module.exports.Statement = Statement;