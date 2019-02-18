const odbc = require('../build/Release/odbc.node');
const { Connection } = require('./Connection');
const { Pool } = require('./Pool');
const { Statement } = require('./Statement');
const { Legacy } = require('./Legacy/legacy');

module.exports = {
  ...module.exports,
  ...odbc,
  Pool,
  Connection,
  Statement,
  Legacy,
};
