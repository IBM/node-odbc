const odbc = require('../../../lib/odbc');

module.exports = {
  generateCreateOrReplaceQueries: function(tableName, fields) {
    const sqlQueries = [];
    sqlQueries.push(`CREATE OR REPLACE TABLE ${tableName} ${fields}`);
    return sqlQueries;
  },
  sqlColumnsColumns: [
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_CAT',
      nullable: true,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_SCHEM',
      nullable: true,
    },
    {
      columnSize: 256,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_NAME',
      nullable: false,
    },
    {
      columnSize: 256,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'COLUMN_NAME',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TYPE_NAME',
      nullable: false,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      dataTypeName: 'SQL_INTEGER',
      decimalDigits: 0,
      name: 'COLUMN_SIZE',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      dataTypeName: 'SQL_INTEGER',
      decimalDigits: 0,
      name: 'BUFFER_LENGTH',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'DECIMAL_DIGITS',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'NUM_PREC_RADIX',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'NULLABLE',
      nullable: false,
    },
    {
      columnSize: 254,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'REMARKS',
      nullable: true,
    },
    {
      columnSize: 2000,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'COLUMN_DEF',
      nullable: true,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'SQL_DATA_TYPE',
      nullable: false,
    },
    {
      columnSize: 5,
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      decimalDigits: 0,
      name: 'SQL_DATETIME_SUB',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      dataTypeName: 'SQL_INTEGER',
      decimalDigits: 0,
      name: 'CHAR_OCTET_LENGTH',
      nullable: true,
    },
    {
      columnSize: 10,
      dataType: odbc.SQL_INTEGER,
      dataTypeName: 'SQL_INTEGER',
      decimalDigits: 0,
      name: 'ORDINAL_POSITION',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'IS_NULLABLE',
      nullable: true,
    },
  ],
  sqlForeignKeysColumns: [
    {
      name: 'PKTABLE_CAT',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'PKTABLE_SCHEM',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'PKTABLE_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'PKCOLUMN_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'FKTABLE_CAT',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'FKTABLE_SCHEM',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'FKTABLE_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'FKCOLUMN_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'KEY_SEQ',
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      columnSize: 5,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'UPDATE_RULE',
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      columnSize: 5,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'DELETE_RULE',
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      columnSize: 5,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'FK_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'PK_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'DEFERRABILITY',
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      columnSize: 5,
      decimalDigits: 0,
      nullable: true
    }
  ],
  sqlPrimaryKeysColumns: [
    {
      name: 'TABLE_CAT',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'TABLE_SCHEM',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    },
    {
      name: 'TABLE_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'COLUMN_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 256,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'KEY_SEQ',
      dataType: odbc.SQL_SMALLINT,
      dataTypeName: 'SQL_SMALLINT',
      columnSize: 5,
      decimalDigits: 0,
      nullable: false
    },
    {
      name: 'PK_NAME',
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      columnSize: 128,
      decimalDigits: 0,
      nullable: true
    }

  ],
  sqlTablesColumns: [
    {
      columnSize: 18,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_CAT',
      nullable: true,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_SCHEM',
      nullable: true,
    },
    {
      columnSize: 256,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_NAME',
      nullable: false,
    },
    {
      columnSize: 128,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'TABLE_TYPE',
      nullable: false,
    },
    {
      columnSize: 254,
      dataType: odbc.SQL_VARCHAR,
      dataTypeName: 'SQL_VARCHAR',
      decimalDigits: 0,
      name: 'REMARKS',
      nullable: false,
    },
  ]
};
