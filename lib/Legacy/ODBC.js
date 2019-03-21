const ODBCConnection = require('./ODBCConnection');

class ODBC {
  constructor() {}

  // eslint-disable-next-line class-methods-use-this
  createConnection(callback) {
    return callback(null, new ODBCConnection());
  }

  // eslint-disable-next-line class-methods-use-this
  createConnectionSync() {
    return new ODBCConnection();
  }
}

module.exports.ODBC = ODBC;
