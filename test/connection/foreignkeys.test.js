/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');

describe('.foreignKeys(catalog, schema, table, fkCatalog, fkSchema, fkTable, callback)...', () => {
  before(async () => {
    let connection;
    try {
      connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const query1 = `CREATE OR REPLACE TABLE ${process.env.DB_SCHEMA}.PKTABLE (ID INTEGER, NAME VARCHAR(24), AGE INTEGER, PRIMARY KEY(ID))`;
      const query2 = `CREATE OR REPLACE TABLE ${process.env.DB_SCHEMA}.FKTABLE (PKID INTEGER, NAME VARCHAR(24), FOREIGN KEY(PKID) REFERENCES PKTABLE(ID))`;
      await connection.query(query1);
      await connection.query(query2);
    } catch (error) {
      if (error.odbcErrors[0].code !== -601) {
        throw (error);
      }
    } finally {
      await connection.close();
    }
  });

  after(async () => {
    let connection;
    try {
      connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const query1 = `DROP TABLE ${process.env.DB_SCHEMA}.PKTABLE`;
      const query2 = `DROP TABLE ${process.env.DB_SCHEMA}.FKTABLE`;
      await connection.query(query1);
      await connection.query(query2);
    } catch (error) {
      if (error.odbcErrors[0].code !== -601) {
        throw (error);
      }
    } finally {
      await connection.close();
    }
  });

  describe('...with callbacks...', () => {
    it('...should return information about a foreign key.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.foreignKeys(null, `${process.env.DB_SCHEMA}`, 'PKTABLE', null, `${process.env.DB_SCHEMA}`, 'FKTABLE', (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 1);
          assert.deepStrictEqual(results.columns, global.dbmsConfig.sqlForeignKeysColumns);

          const result = results[0];
          // not testing for TABLE_CAT, dependent on the system
          assert.strictEqual(result.PKTABLE_SCHEM, `${process.env.DB_SCHEMA}`);
          assert.strictEqual(result.PKTABLE_NAME, 'PKTABLE');
          assert.strictEqual(result.PKCOLUMN_NAME, 'ID');
          assert.strictEqual(result.FKTABLE_SCHEM, `${process.env.DB_SCHEMA}`);
          assert.strictEqual(result.FKTABLE_NAME, 'FKTABLE');
          assert.strictEqual(result.FKCOLUMN_NAME, 'PKID');
          done();
        });
      });
    });
    it('...should return empty with bad parameters.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.foreignKeys(null, 'bad schema name', 'bad table name', null, 'badschema2', 'badtable2', (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 0);
          assert.deepStrictEqual(results.columns, global.dbmsConfig.sqlForeignKeysColumns);

          done();
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should return information about a primary key.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      
      const results = await connection.foreignKeys(null, `${process.env.DB_SCHEMA}`, 'PKTABLE', null, `${process.env.DB_SCHEMA}`, 'FKTABLE');
      assert.strictEqual(results.length, 1);
      assert.deepStrictEqual(results.columns, global.dbmsConfig.sqlForeignKeysColumns);

      const result = results[0];
      // not testing for TABLE_CAT, dependent on the system
      assert.strictEqual(result.PKTABLE_SCHEM, `${process.env.DB_SCHEMA}`);
      assert.strictEqual(result.PKTABLE_NAME, 'PKTABLE');
      assert.strictEqual(result.PKCOLUMN_NAME, 'ID');
      assert.strictEqual(result.FKTABLE_SCHEM, `${process.env.DB_SCHEMA}`);
      assert.strictEqual(result.FKTABLE_NAME, 'FKTABLE');
      assert.strictEqual(result.FKCOLUMN_NAME, 'PKID');
      await connection.close();
    });
    it('...should return empty with bad parameters.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const results = await connection.foreignKeys(null, 'bad schema name', 'bad table name', null, "badschema2", "badtable2");

      assert.strictEqual(results.length, 0);
      assert.deepStrictEqual(results.columns, global.dbmsConfig.sqlForeignKeysColumns);
      await connection.close();
    });
  }); // ...with promises...
});
