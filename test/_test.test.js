/* eslint-env node, mocha */
/* eslint-disable global-require */

const odbc = require('../lib/odbc');

const OBJECTS_EXISTS_STATE = -601;
const DBMS_LIST = [
  'ibmi',
  'mariadb',
  'mssql',
  'mysql',
  'postgresql', 'postgres',
  'sybase',
];
global.dbms = undefined;

before(async () => {
  if (process.env.DBMS)
  {
    if (DBMS_LIST.indexOf(process.env.DBMS) > -1)
    {
      require('dotenv').config({ path: `test/DBMS/${process.env.DBMS}/.env` });
      global.dbms = process.env.DBMS;
      global.dbmsConfig = require(`./DBMS/${process.env.DBMS}/config.js`);
    }
    else
    {
      let supportedDbmsList = '';
      DBMS_LIST.forEach((dbms) => {
        supportedDbmsList = supportedDbmsList.concat(`${dbms}, `);
      });
      supportedDbmsList = supportedDbmsList.slice(0, -2);

      throw new Error(`DBMS is not recognized. Supported DBMS values for running tests include:
      
      ${supportedDbmsList}`);
    }
  }
  else
  {
    throw new Error(`Please set the DBMS environment variable to your target DBMS when calling the test suite:

    DBMS=____ npm test`);
  }
});

describe('odbc', () => {
  before(async () => {
    let connection;
    try {
      connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const queries = global.dbmsConfig.generateCreateOrReplaceQueries(`${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, '(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)');
      for(queryString of queries) {
        await connection.query(queryString);
      };
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

  require('./queries/_test.test.js');
  require('./connection/_test.test.js');
  require('./statement/_test.test.js');
  require('./pool/_test.test.js');
  require('./cursor/_test.test.js');
});
