/* eslint-env node, mocha */
/* eslint-disable global-require */

const odbc = require('../');

const OBJECTS_EXISTS_STATE = -601;

describe('odbc', () => {
  before(async () => {
    let connection;
    try {
      connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.query(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
    } catch (error) {
      if (error.odbcErrors[0].code !== OBJECTS_EXISTS_STATE) {
        throw (error);
      }
    } finally {
      await connection.close();
    }
  });

  afterEach(async () => {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    try {
      await connection.query(`DELETE FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
    } catch (error) {
      // There may be errors if deleting from the table when there are no rows in the table
    } finally {
      await connection.close();
    }
  });

  after(async () => {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.query(`DROP TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
    await connection.close();
  });

  require('./queries/test.js');
  require('./connection/test.js');
  require('./statement/test.js');
  require('./pool/test.js');
});
