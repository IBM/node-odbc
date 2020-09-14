/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.tables(catalog, schema, table, type, callback)...', () => {
  describe('...with callbacks...', () => {
    it('...should return information about a table.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.tables(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 1);
          assert.strictEqual(results.count, 1);
          assert.deepStrictEqual(results.columns,
            [
              {
                columnSize: 18,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_CAT',
                nullable: true,
              },
              {
                columnSize: 128,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_SCHEM',
                nullable: true,
              },
              {
                columnSize: 256,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_NAME',
                nullable: false,
              },
              {
                columnSize: 128,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_TYPE',
                nullable: false,
              },
              {
                columnSize: 254,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'REMARKS',
                nullable: false,
              },
            ]);
          const result = results[0];
          // not testing for TABLE_CAT, dependent on the system
          assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
          assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
          assert.strictEqual(result.TABLE_TYPE, 'TABLE');
          // not testing for REMARKS, dependent on the system
          done();
        });
      });
    });
    it('...should return empty with bad parameters.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.tables(null, 'bad schema name', 'bad table name', null, (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 0);
          assert.strictEqual(results.count, 0);
          assert.deepStrictEqual(results.columns,
            [
              {
                columnSize: 18,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_CAT',
                nullable: true,
              },
              {
                columnSize: 128,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_SCHEM',
                nullable: true,
              },
              {
                columnSize: 256,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_NAME',
                nullable: false,
              },
              {
                columnSize: 128,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'TABLE_TYPE',
                nullable: false,
              },
              {
                columnSize: 254,
                dataType: odbc.SQL_VARCHAR,
                decimalDigits: 0,
                name: 'REMARKS',
                nullable: false,
              },
            ]);
          done();
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should return information about a table.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const results = await connection.tables(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
      assert.strictEqual(results.length, 1);
      assert.strictEqual(results.count, 1);
      assert.deepStrictEqual(results.columns,
        [
          {
            columnSize: 18,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_CAT',
            nullable: true,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_SCHEM',
            nullable: true,
          },
          {
            columnSize: 256,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_NAME',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_TYPE',
            nullable: false,
          },
          {
            columnSize: 254,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'REMARKS',
            nullable: false,
          },
        ]);
      const result = results[0];
      // not testing for TABLE_CAT, dependent on the system
      assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
      assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
      assert.strictEqual(result.TABLE_TYPE, 'TABLE');
      // not testing for REMARKS, dependent on the system
    });
    it('...should return empty with bad parameters.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const results = await connection.tables(null, 'bad schema name', 'bad table name', null);
      assert.strictEqual(results.length, 0);
      assert.strictEqual(results.count, 0);
      assert.deepStrictEqual(results.columns,
        [
          {
            columnSize: 18,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_CAT',
            nullable: true,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_SCHEM',
            nullable: true,
          },
          {
            columnSize: 256,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_NAME',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TABLE_TYPE',
            nullable: false,
          },
          {
            columnSize: 254,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'REMARKS',
            nullable: false,
          },
        ]);
    });
  }); // ...with promises...
});
