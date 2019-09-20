/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');
// const odbc = require('../../build/Release/odbc.node');

describe('.columns(catalog, schema, table, column, callback)...', () => {
  let connection = null;
  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });
  describe('...with callbacks...', () => {
    it('...should return information about all columns of a table.', (done) => {
      connection.columns(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error, results) => {
        assert.strictEqual(error, null);
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
        done();
      });
    });
    it('...should return empty with bad parameters.', (done) => {
      connection.columns(null, 'bad schema name', 'bad table name', null, (error, results) => {
        assert.strictEqual(error, null);
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
        done();
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should return information about all columns of a table.', async () => {
      const results = await connection.columns(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
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
    it('...should return empty with bad parameters.', async () => {
      const results = await connection.columns(null, 'bad schema name', 'bad table name', null);
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
  }); // ...with promises...
});
