/* eslint-env node, mocha */
/* eslint-disable global-require */

const { Connection } = require('../');

describe('odbc', () => {
  before(async () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    await connection.query(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
    await connection.close();
  });

  afterEach(async () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    try {
      await connection.query(`DELETE FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE 1=1`);
    } catch (error) {
      // console.log(error);
    } finally {
      await connection.close();
    }
  });

  after(async () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    await connection.query(`DROP TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
    await connection.close();
  });

  require('./connection/test.js');
  require('./statement/test.js');
  require('./pool/test.js');
  // require('./v1.0/test.js')
});
