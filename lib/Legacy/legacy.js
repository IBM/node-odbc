const native = require('../../build/Release/odbc.node');
const { Pool } = require('./Pool');
const { Database } = require('./Database');
// Wrappers for legacy calls to the native code to use the new API
const { ODBC } = require('./ODBC');
const { ODBCConnection } = require('./ODBCConnection');
const { ODBCStatement } = require('./ODBCStatement');
const { ODBCResult } = require('./ODBCResult');

module.exports = options => new Database(options);
module.exports = {
  ...module.exports,
  Pool,
  Database,
};

module.exports.open = (connectionString, options, cb) => {
  let config = options;
  let callback = cb;

  if (typeof config === 'function') {
    callback = config;
    config = null;
  }

  const db = new Database(config);

  db.open(connectionString, (err) => {
    callback(err, db);
  });
};

// Expose constants
Object.keys(native).forEach((key) => {
  if (typeof native[key] !== 'function') {
    module.exports[key] = native[key]; // On the exports
  }
});

module.exports.ODBC = ODBC;
module.exports.ODBCConnection = ODBCConnection;
module.exports.ODBCStatement = ODBCStatement;
module.exports.ODBCResult = ODBCResult;
module.exports.loadODBCLibrary = native.loadODBCLibrary;
