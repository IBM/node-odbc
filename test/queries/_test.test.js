/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');

describe('Queries...', () => {
  let connection = null;

  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  it('...should pass w1nk/node-odbc issue #54', async () => {
    const result = await connection.query(`select cast(-1 as INTEGER) as TEST1, cast(-2147483648 as INTEGER) as TEST2, cast(2147483647 as INTEGER) as TEST3 ${global.dbms === 'ibmi' ? 'from sysibm.sysdummy1' : ''}`);
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].TEST1, -1);
    assert.deepEqual(result[0].TEST2, -2147483648);
    assert.deepEqual(result[0].TEST3, 2147483647);
  });
  it('...should pass w1nk/node-odbc issue #85', async () => {
    const result = await connection.query(`select cast(-1 as INTEGER) as TEST1, cast(-2147483648 as INTEGER) as TEST2, cast(2147483647 as INTEGER) as TEST3 ${global.dbms === 'ibmi' ? 'from sysibm.sysdummy1' : ''}`);
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].TEST1, -1);
    assert.deepEqual(result[0].TEST2, -2147483648);
    assert.deepEqual(result[0].TEST3, 2147483647);
  });
  it('...should retrieve data from an SQL_(W)LONG data types', async () => {
    const alphabet = 'abcdefghijklmnopqrstuvwxyz'
    const result = await connection.query(`select cast ('${alphabet}' as CLOB) as SQL_LONGVARCHAR_FIELD, cast ('${alphabet}' as DBCLOB CCSID 1200) as SQL_WLONGVARCHAR_FIELD, cast (cast('${alphabet}' as CLOB CCSID 1208) as BLOB) as SQL_LONGVARBINARY_FIELD from sysibm.sysdummy1`);
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].SQL_LONGVARCHAR_FIELD, alphabet);
    assert.deepEqual(result[0].SQL_WLONGVARCHAR_FIELD, alphabet);
    let buffer = new ArrayBuffer(alphabet.length);
    let uint8view = new Uint8Array(buffer);
    for (let i = 0; i < alphabet.length; i++)
    {
      uint8view[i] = alphabet.charCodeAt(i);
    }
    assert.deepEqual(result[0].SQL_LONGVARBINARY_FIELD, buffer);
    assert.deepEqual(result.columns[0], {
      name: 'SQL_LONGVARCHAR_FIELD',
      dataType: -1,
      dataTypeName: "SQL_LONGVARCHAR",
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
    assert.deepEqual(result.columns[1], {
      name: 'SQL_WLONGVARCHAR_FIELD',
      dataType: -10,
      dataTypeName: "SQL_WLONGVARCHAR",
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
    assert.deepEqual(result.columns[2], {
      name: 'SQL_LONGVARBINARY_FIELD',
      dataType: -4,
      dataTypeName: "SQL_LONGVARBINARY",
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
  });
  it('...should retrieve data from an SQL_(W)LONG data types with a small initial buffer', async () => {
    const alphabet = 'abcdefghijklmnopqrstuvwxyz'
    const result = await connection.query(`select cast ('${alphabet}' as CLOB) as SQL_LONGVARCHAR_FIELD, cast ('${alphabet}' as DBCLOB CCSID 1200) as SQL_WLONGVARCHAR_FIELD, cast (cast('${alphabet}' as CLOB CCSID 1208) as BLOB) as SQL_LONGVARBINARY_FIELD from sysibm.sysdummy1`, { initialBufferSize: 4 });
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].SQL_LONGVARCHAR_FIELD, alphabet);
    assert.deepEqual(result[0].SQL_WLONGVARCHAR_FIELD, alphabet);
    let buffer = new ArrayBuffer(alphabet.length);
    let uint8view = new Uint8Array(buffer);
    for (let i = 0; i < alphabet.length; i++)
    {
      uint8view[i] = alphabet.charCodeAt(i);
    }
    assert.deepEqual(result[0].SQL_LONGVARBINARY_FIELD, buffer);
    assert.deepEqual(result.columns[0], {
      name: 'SQL_LONGVARCHAR_FIELD',
      dataType: -1,
      dataTypeName: "SQL_LONGVARCHAR",
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
    assert.deepEqual(result.columns[1], {
      name: 'SQL_WLONGVARCHAR_FIELD',
      dataType: -10,
      dataTypeName: "SQL_WLONGVARCHAR",
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
    assert.deepEqual(result.columns[2], {
      name: 'SQL_LONGVARBINARY_FIELD',
      dataTypeName: "SQL_LONGVARBINARY",
      dataType: -4,
      columnSize: 1048576,
      decimalDigits: 0,
      nullable: false
    });
  });
});
