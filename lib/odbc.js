const odbc = require("bindings")("odbc_bindings");

module.exports = { ...odbc.ODBCConstants };

module.exports.connectSync = odbc.connectSync;
module.exports.connect = odbc.connect;

Object.freeze(module.exports);