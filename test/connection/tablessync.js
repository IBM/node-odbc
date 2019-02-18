/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../build/Release/odbc.node');
const { Connection } = require('../../');

describe('.tablesSync(catalog, schema, table, type)...', () => {
  it('...should return information about a table.', () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    const results = connection.tablesSync(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
    assert.strictEqual(results.length, 1);
    assert.strictEqual(results.count, 1);
    assert.deepStrictEqual(results.columns,
      [
        { name: 'TABLE_CAT', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_SCHEM', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_TYPE', dataType: odbc.SQL_VARCHAR },
        { name: 'REMARKS', dataType: odbc.SQL_VARCHAR },
      ]);
    const result = results[0];
    // not testing for TABLE_CAT, dependent on the system
    assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
    assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
    assert.strictEqual(result.TABLE_TYPE, 'TABLE');
    // not testing for REMARKS, dependent on the system
  });
  it('...should return empty with bad parameters.', () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    const results = connection.tablesSync(null, 'bad schema name', 'bad table name', null);
    assert.strictEqual(results.length, 0);
    assert.strictEqual(results.count, 0);
    assert.deepStrictEqual(results.columns,
      [
        { name: 'TABLE_CAT', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_SCHEM', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_TYPE', dataType: odbc.SQL_VARCHAR },
        { name: 'REMARKS', dataType: odbc.SQL_VARCHAR },
      ]);
  });
});
