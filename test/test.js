/* eslint-env node, mocha */
const { Connection } = require('../');

describe('odbc', () => {
  before(() => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    connection.querySync(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
    connection.closeSync();
  });

  after(() => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    connection.querySync(`DROP TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
    connection.closeSync();
  });

  require('./connection/test.js');
  require('./pool/test.js');
  require('./v1.0/test.js')
});
