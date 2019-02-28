/* eslint-env node, mocha */
/* eslint-disable global-require */

const { Connection } = require('../');

const TABLE_EXISTS_STATE = '42S01';

describe('odbc', () => {
  before(async () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    try {
      await connection.query(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
    } catch (error) {
      const errorJSON = JSON.parse(`{${error.message}}`);
      const sqlState = errorJSON.errors[0].SQLState;
      if (sqlState !== TABLE_EXISTS_STATE) {
        throw (error);
      }
    }
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
  // require('./pool/test.js');
  // require('./v1.0/test.js')
});
