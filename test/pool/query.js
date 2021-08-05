/* eslint-env node, mocha */
const assert     = require('assert');
const odbc       = require('../../');
const { Cursor } = require('../../lib/Cursor');

describe.only('.query...', () => {
  describe('...with callbacks...', () => {
    it('...should be able to insert and retrieve data from the database.', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error, pool) => {
        assert.deepStrictEqual(error, null);
        assert.notDeepEqual(pool, null);
        pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepStrictEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepStrictEqual(result1.count, 1);
          pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepStrictEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepStrictEqual(result2.length, 1);
            done();
          });
        });
      });
    });
  });
  describe('...with promises...', () => {
    it('...should correctly identify function signature with .query({string}).', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {array})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {array}, {object})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE ID = ?`, [1], {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {object})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await pool.close();
    });
    describe('...testing query options...', () => {
      describe('...[cursor]...', () => {
        it('...should throw an error if cursor is not a boolean or string', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: 1 }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: null }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await pool.close();
        });
      });
      describe('...[fetchSize]...', () => {
        it('...should throw an error if fetchSize is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if fetchSize is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
      describe('...[timeout]...', () => {
        it('...should throw an error if timeout is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if timeout is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
      describe('...[initialBufferSize]...', () => {
        it('...should throw an error if initialBufferSize is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if initialBufferSize is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: 0}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: -1}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
    });
  }); // ...with promises...
});
