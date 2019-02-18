/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../build/Release/odbc.node');
const { Connection } = require('../../');

describe('.columnsSync(catalog, schema, table, column)...', () => {
  it('...should return information about all columns of a table.', () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    const results = connection.columnsSync(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
    assert.strictEqual(results.length, 3);
    assert.strictEqual(results.count, 3);
    assert.deepStrictEqual(results.columns,
      [
        { name: 'TABLE_CAT', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_SCHEM', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'DATA_TYPE', dataType: odbc.SQL_SMALLINT },
        { name: 'TYPE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_SIZE', dataType: odbc.SQL_INTEGER },
        { name: 'BUFFER_LENGTH', dataType: odbc.SQL_INTEGER },
        { name: 'DECIMAL_DIGITS', dataType: odbc.SQL_SMALLINT },
        { name: 'NUM_PREC_RADIX', dataType: odbc.SQL_SMALLINT },
        { name: 'NULLABLE', dataType: odbc.SQL_SMALLINT },
        { name: 'REMARKS', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_DEF', dataType: odbc.SQL_VARCHAR },
        { name: 'SQL_DATA_TYPE', dataType: odbc.SQL_SMALLINT },
        { name: 'SQL_DATETIME_SUB', dataType: odbc.SQL_SMALLINT },
        { name: 'CHAR_OCTET_LENGTH', dataType: odbc.SQL_INTEGER },
        { name: 'ORDINAL_POSITION', dataType: odbc.SQL_INTEGER },
        { name: 'IS_NULLABLE', dataType: odbc.SQL_VARCHAR },
      ]);
    const idColumn = results[0];
    assert.deepEqual(idColumn.COLUMN_NAME, 'ID');
    assert.deepEqual(idColumn.DATA_TYPE, odbc.SQL_INTEGER);
    assert.deepEqual(idColumn.NULLABLE, odbc.SQL_NULLABLE);

    const nameColumn = results[1];
    assert.deepEqual(nameColumn.COLUMN_NAME, 'NAME');
    assert.deepEqual(nameColumn.DATA_TYPE, odbc.SQL_VARCHAR);
    assert.deepEqual(nameColumn.NULLABLE, odbc.SQL_NULLABLE);

    const ageColumn = results[2];
    assert.deepEqual(ageColumn.COLUMN_NAME, 'AGE');
    assert.deepEqual(ageColumn.DATA_TYPE, odbc.SQL_INTEGER);
    assert.deepEqual(ageColumn.NULLABLE, odbc.SQL_NULLABLE);
  });
  it('...should return empty with bad parameters.', () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    const results = connection.columnsSync(null, 'bad schema name', 'bad table name', null);
    assert.strictEqual(results.length, 0);
    assert.strictEqual(results.count, 0);
    assert.deepStrictEqual(results.columns,
      [
        { name: 'TABLE_CAT', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_SCHEM', dataType: odbc.SQL_VARCHAR },
        { name: 'TABLE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'DATA_TYPE', dataType: odbc.SQL_SMALLINT },
        { name: 'TYPE_NAME', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_SIZE', dataType: odbc.SQL_INTEGER },
        { name: 'BUFFER_LENGTH', dataType: odbc.SQL_INTEGER },
        { name: 'DECIMAL_DIGITS', dataType: odbc.SQL_SMALLINT },
        { name: 'NUM_PREC_RADIX', dataType: odbc.SQL_SMALLINT },
        { name: 'NULLABLE', dataType: odbc.SQL_SMALLINT },
        { name: 'REMARKS', dataType: odbc.SQL_VARCHAR },
        { name: 'COLUMN_DEF', dataType: odbc.SQL_VARCHAR },
        { name: 'SQL_DATA_TYPE', dataType: odbc.SQL_SMALLINT },
        { name: 'SQL_DATETIME_SUB', dataType: odbc.SQL_SMALLINT },
        { name: 'CHAR_OCTET_LENGTH', dataType: odbc.SQL_INTEGER },
        { name: 'ORDINAL_POSITION', dataType: odbc.SQL_INTEGER },
        { name: 'IS_NULLABLE', dataType: odbc.SQL_VARCHAR },
      ]);
  });
});
