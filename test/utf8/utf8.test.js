/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');

UNICODE_TABLE = 'Tαble'

describe('UTF8:', () => {
  let connection = null;

  before(async () => {

    global.table = `${process.env.DB_SCHEMA}.${UNICODE_TABLE}`;
        
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    
    ddl_statements = [
        `DROP TABLE IF EXISTS ${global.table}`,
        `CREATE TABLE ${global.table} (
        [Col Ω] nvarchar(255),
        [Col α] varchar(255)
        );`,        
        `DROP PROCEDURE IF EXISTS [${process.env.DB_SCHEMA}].[ΩProc]`,
        //`CREATE PROCEDURE [${process.env.DB_SCHEMA}].[ΩProc] @value NVARCHAR(32) AS SELECT N'Ω', @value;`
        `CREATE PROCEDURE [${process.env.DB_SCHEMA}].[ΩProc] @value NVARCHAR(32) AS SET FMTONLY ON;`
    ]
    for (const statement of ddl_statements) {      
        await connection.query(statement);
    }
    console.log('Exit');
    await connection.close();
  
  });

  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  after(async () => {    
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.query(`DROP TABLE IF EXISTS ${global.table}`);
    await connection.query(`DROP PROCEDURE IF EXISTS [${process.env.DB_SCHEMA}].[ΩProc]`);
    await connection.close();    
  });

  describe('...with connection.query()', () => {
    it('- should accept UTF8 value literals.', async () => {       
      statement = `insert into ${global.table} ([Col Ω], [Col α]) values (N'an Ω', 'other')`
      result = await connection.query(statement);
      assert.equal(result.statement, statement);
    });

    it('- should not accept UNICODE literal beyond UCS-2 (e.g., emoji 😀)', async () => {      
      statement = `insert into ${global.table} ([Col Ω], [Col α]) values (N'😀', 'other')`
      assert.rejects(connection.query(statement));   
      
      statement = `insert into ${global.table} ([Col Ω], [Col α]) values (N'\u{1F600}', 'other')`
      assert.rejects(connection.query(statement));

      statement = `insert into ${global.table} ([Col Ω], [Col α]) values (N'\u{1F600}', 'other')`
      assert.rejects(connection.query(statement));

      statement = `select [Col Ω] as [Col \u{1F600}] from ${global.table}`
      assert.rejects(connection.query(statement));
    });

    it('- should return UTF8 query values', async () => {      
      statement = `select [Col Ω],[Col α] from ${global.table}`
      result = await connection.query(statement);
      assert.deepEqual(result[0], { 'Col Ω': 'an Ω', 'Col α': 'other' });

      statement = `select "Col Ω","Col α" from ${global.table}`
      result = await connection.query(statement);
      assert.deepEqual(result[0], { 'Col Ω': 'an Ω', 'Col α': 'other' });

      statement = `select * from ${global.table} where [Col Ω] = N'an Ω'`
      result = await connection.query(statement);
      assert.deepEqual(result[0], { 'Col Ω': 'an Ω', 'Col α': 'other' });
    });

    it('- should accept UTF8 literals when prepared via query parameters', async () => {
      statement = `insert into ${global.table} ([Col Ω], [Col α]) values (?, ?)`
      result = await connection.query(statement, ['Ω prepare','other']);
      assert.equal(result.statement, statement);
      assert.deepEqual(result.parameters, [ 'Ω prepare', 'other' ]);
    });

  });  

  describe('...with connection.columns()', () => {

    it('- should accept UTF8 literals', async () => {      
      result = await connection.columns(
        null,
        process.env.DB_SCHEMA,
        UNICODE_TABLE,
        null);
      assert.deepEqual(result[0]['COLUMN_NAME'], 'Col Ω');
      assert.deepEqual(result[1]['COLUMN_NAME'], 'Col α');
    });

    it('- should accept NULL values', async () => {      
      result = await connection.columns(
        null,
        process.env.DB_SCHEMA,
        UNICODE_TABLE,
        null);
        assert.deepEqual(result[0]['COLUMN_NAME'], 'Col Ω');
        assert.deepEqual(result[1]['COLUMN_NAME'], 'Col α');
    });

    it('- should not accept UNICODE literals beyond UCS-2 (e.g., emoji 😀)', async () => {      
      assert.rejects(connection.columns(
        '😀', 
        '😀',
        '😀',
        null));
    });
  });

  describe('...with connection.tables()', () => {
    it('- should accept UTF8 literals', async () => {
      result = await connection.tables(
        null,
        process.env.DB_SCHEMA,
        UNICODE_TABLE,
        'TABLE');
      assert.deepEqual(result[0]['TABLE_SCHEM'], process.env.DB_SCHEMA);
      assert.deepEqual(result[0]['TABLE_NAME'], UNICODE_TABLE);
      assert.deepEqual(result[0]['TABLE_TYPE'], 'TABLE');
    });

    it('- should accept NULL values', async () => {      
      result = await connection.tables(
        null,
        process.env.DB_SCHEMA,
        UNICODE_TABLE,
        null);
      assert.deepEqual(result[0]['TABLE_SCHEM'], process.env.DB_SCHEMA);
      assert.deepEqual(result[0]['TABLE_NAME'], UNICODE_TABLE);
      assert.deepEqual(result[0]['TABLE_TYPE'], 'TABLE');
    });

    it('- should not accept UNICODE literals beyond UCS-2 (e.g., emoji 😀)', async () => {      
      assert.rejects(connection.tables(
        '😀', 
        '😀',
        '😀',
        null));
    });

  });

  describe('...with statement.prepare()', () => {

    it('- should accept UTF8 literals', async () => {
      statement = await connection.createStatement();
      assert.doesNotReject(statement.prepare(
        `insert into ${global.table} ([Col Ω], [Col α]) values (?, ?)`));
    });

    it('- should not accept UNICODE literal beyond UCS-2 (e.g., emoji 😀)', async () => {      
      statement = await connection.createStatement();
      assert.rejects(statement.prepare(
        `insert into ${global.table} ([Col Ω], [Col 😀]) values (?, ?)`));
    });

  });

  describe.skip('...with statement.callProcedure()', () => {

    // This fails due to another issue with a check on the number of parameters
    // Error: [odbc] The number of parameters the procedure expects and and the number of passed parameters is not equal
    
    it('- should accept UTF8 literals', async () => {
      result = await connection.callProcedure(
        null,
        process.env.DB_SCHEMA,
        'ΩProc', ['α']);
      assert.equal(result.statement, `{ CALL ${process.env.DB_SCHEMA}.ΩProc (?) }`);
      assert.deepEqual(result.parameters, [ 'α' ]);
    });

  });

});
