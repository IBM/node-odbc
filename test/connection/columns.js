/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

const dbmsColumnDefinitions = {
  ibmi: [
    {
      columnSize: 128,
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
      columnSize: 256,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'COLUMN_NAME',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'TYPE_NAME',
      nullable: false,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'COLUMN_SIZE',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'BUFFER_LENGTH',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'DECIMAL_DIGITS',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'NUM_PREC_RADIX',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'NULLABLE',
      nullable: false,
    },
    {
      columnSize: 254,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'REMARKS',
      nullable: true,
    },
    {
      columnSize: 2000,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'COLUMN_DEF',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'SQL_DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'SQL_DATETIME_SUB',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'CHAR_OCTET_LENGTH',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'ORDINAL_POSITION',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'IS_NULLABLE',
      nullable: true,
    },
  ],
  mssql: [
    {
      columnSize: 128,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'TABLE_CAT',
      nullable: true,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'TABLE_SCHEM',
      nullable: true,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'TABLE_NAME',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'COLUMN_NAME',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'TYPE_NAME',
      nullable: false,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'COLUMN_SIZE',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'BUFFER_LENGTH',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'DECIMAL_DIGITS',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'NUM_PREC_RADIX',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'NULLABLE',
      nullable: false,
    },
    {
      columnSize: 254,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'REMARKS',
      nullable: true,
    },
    {
      columnSize: 4000,
      dataType: odbc.SQL_WVARCHAR,
      decimalDigits: 0,
      name: 'COLUMN_DEF',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'SQL_DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      decimalDigits: 0,
      name: 'SQL_DATETIME_SUB',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'CHAR_OCTET_LENGTH',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      decimalDigits: 0,
      name: 'ORDINAL_POSITION',
      nullable: false,
    },
    {
      columnSize: 254,
      dataType: odbc.SQL_VARCHAR,
      decimalDigits: 0,
      name: 'IS_NULLABLE',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: 5,
      decimalDigits: 0,
      name: "SS_IS_SPARSE",
      nullable: true
    },
    {
      columnSize: 5,
      dataType: 5,
      decimalDigits: 0,
      name: "SS_IS_COLUMN_SET",
      nullable: true
    },
    {
      columnSize: 5,
      dataType: 5,
      decimalDigits: 0,
      name: "SS_IS_COMPUTED",
      nullable: true
    },
    {
      columnSize: 5,
      dataType: 5,
      decimalDigits: 0,
      name: "SS_IS_IDENTITY",
      nullable: true
    },
    {
      columnSize: 128,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_UDT_CATALOG_NAME",
      nullable: true
    },
    {
      columnSize: 128,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_UDT_SCHEMA_NAME",
      nullable: true
    },
    {
      columnSize: 4000,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_UDT_ASSEMBLY_TYPE_NAME",
      nullable: true
    },
    {
      columnSize: 128,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_XML_SCHEMACOLLECTION_CATALOG_NAME",
      nullable: true
    },
    {
      columnSize: 128,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_XML_SCHEMACOLLECTION_SCHEMA_NAME",
      nullable: true
    },
    {
      columnSize: 128,
      dataType: -9,
      decimalDigits: 0,
      name: "SS_XML_SCHEMACOLLECTION_NAME",
      nullable: true
    },
    {
      columnSize: 3,
      dataType: -6,
      decimalDigits: 0,
      name: "SS_DATA_TYPE",
      nullable: true
    }
  ]
}; 

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
    it.only('...should return information about all columns of a table.', (done) => {
      connection.columns(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error, results) => {
        assert.strictEqual(error, null);
        assert.strictEqual(results.length, 3);
        assert.strictEqual(results.count, -1);
        assert.deepStrictEqual(results.columns, dbmsColumnDefinitions[global.dbms]);

        console.log(results);

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
            {
              columnSize: 128,
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
              columnSize: 256,
              dataType: odbc.SQL_VARCHAR,
              decimalDigits: 0,
              name: 'COLUMN_NAME',
              nullable: false,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'DATA_TYPE',
              nullable: false,
            },
            {
              columnSize: 128,
              dataType: odbc.SQL_VARCHAR,
              decimalDigits: 0,
              name: 'TYPE_NAME',
              nullable: false,
            },
            {
              columnSize: 10,
              dataType: odbc.SQL_INTEGER,
              decimalDigits: 0,
              name: 'COLUMN_SIZE',
              nullable: true,
            },
            {
              columnSize: 10,
              dataType: odbc.SQL_INTEGER,
              decimalDigits: 0,
              name: 'BUFFER_LENGTH',
              nullable: true,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'DECIMAL_DIGITS',
              nullable: true,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'NUM_PREC_RADIX',
              nullable: true,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'NULLABLE',
              nullable: false,
            },
            {
              columnSize: 254,
              dataType: odbc.SQL_VARCHAR,
              decimalDigits: 0,
              name: 'REMARKS',
              nullable: true,
            },
            {
              columnSize: 2000,
              dataType: odbc.SQL_VARCHAR,
              decimalDigits: 0,
              name: 'COLUMN_DEF',
              nullable: true,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'SQL_DATA_TYPE',
              nullable: false,
            },
            {
              columnSize: 5,
              dataType: odbc.SQL_SMALLINT,
              decimalDigits: 0,
              name: 'SQL_DATETIME_SUB',
              nullable: true,
            },
            {
              columnSize: 10,
              dataType: odbc.SQL_INTEGER,
              decimalDigits: 0,
              name: 'CHAR_OCTET_LENGTH',
              nullable: true,
            },
            {
              columnSize: 10,
              dataType: odbc.SQL_INTEGER,
              decimalDigits: 0,
              name: 'ORDINAL_POSITION',
              nullable: false,
            },
            {
              columnSize: 128,
              dataType: odbc.SQL_VARCHAR,
              decimalDigits: 0,
              name: 'IS_NULLABLE',
              nullable: true,
            },
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
          {
            columnSize: 128,
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
            columnSize: 256,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'COLUMN_NAME',
            nullable: false,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'DATA_TYPE',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TYPE_NAME',
            nullable: false,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'COLUMN_SIZE',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'BUFFER_LENGTH',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'DECIMAL_DIGITS',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'NUM_PREC_RADIX',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'NULLABLE',
            nullable: false,
          },
          {
            columnSize: 254,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'REMARKS',
            nullable: true,
          },
          {
            columnSize: 2000,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'COLUMN_DEF',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'SQL_DATA_TYPE',
            nullable: false,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'SQL_DATETIME_SUB',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'CHAR_OCTET_LENGTH',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'ORDINAL_POSITION',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'IS_NULLABLE',
            nullable: true,
          },
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
          {
            columnSize: 128,
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
            columnSize: 256,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'COLUMN_NAME',
            nullable: false,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'DATA_TYPE',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'TYPE_NAME',
            nullable: false,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'COLUMN_SIZE',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'BUFFER_LENGTH',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'DECIMAL_DIGITS',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'NUM_PREC_RADIX',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'NULLABLE',
            nullable: false,
          },
          {
            columnSize: 254,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'REMARKS',
            nullable: true,
          },
          {
            columnSize: 2000,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'COLUMN_DEF',
            nullable: true,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'SQL_DATA_TYPE',
            nullable: false,
          },
          {
            columnSize: 5,
            dataType: odbc.SQL_SMALLINT,
            decimalDigits: 0,
            name: 'SQL_DATETIME_SUB',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'CHAR_OCTET_LENGTH',
            nullable: true,
          },
          {
            columnSize: 10,
            dataType: odbc.SQL_INTEGER,
            decimalDigits: 0,
            name: 'ORDINAL_POSITION',
            nullable: false,
          },
          {
            columnSize: 128,
            dataType: odbc.SQL_VARCHAR,
            decimalDigits: 0,
            name: 'IS_NULLABLE',
            nullable: true,
          },
        ]);
    });
  }); // ...with promises...
});
