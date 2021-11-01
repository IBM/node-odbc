/* eslint-env node, mocha */
const assert     = require('assert');
const odbc       = require('../../lib/odbc');
const { Cursor } = require('../../lib/Cursor');

const TABLE_NAME = "FETCH_TABLE";

// create a queryOptions object to create a Cursor object instead of generating
// results immediately
const queryOptions = {
  cursor: true,
  fetchSize: 3,
};

describe('.close([callback])...', () => {

  // Populate a table that we can fetch from, with a known number of rows
  before(async () => {
    try {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const queries = global.dbmsConfig.generateCreateOrReplaceQueries(`${process.env.DB_SCHEMA}.${TABLE_NAME}`, '(COL1 INT NOT NULL, COL2 CHAR(3), COL3 VARCHAR(16))');
      for(queryString of queries) {
        await connection.query(queryString);
      };
      await connection.query(`DELETE FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(1, 'ABC', 'DEF')`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(2, NULL, NULL)`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(3, 'G', 'HIJKLMN')`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(4, NULL, 'OP')`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(5, 'Q', NULL)`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(6, NULL, 'RST')`);
      result = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${TABLE_NAME} VALUES(7, 'UVW', 'XYZ')`);
    } catch (e) {
      // TODO: catch error
    }
  });

  after(async () => {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.query(`DROP TABLE ${process.env.DB_SCHEMA}.${TABLE_NAME}`);
    await connection.close();
  });


  describe('...with callbacks...', () => {
    it('...should not error when closed before calling .fetch().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions, (error1, cursor) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(cursor, null);
          assert.deepEqual(cursor instanceof Cursor, true);
          assert.deepEqual(cursor.noData, false);
          cursor.close((error2) => {
            assert.deepEqual(error2, null);
            done();
          });
        });
      });
    });

    it('...should not error when closed after calling .fetch().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions, (error1, cursor) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(cursor, null);
          assert.deepEqual(cursor instanceof Cursor, true);
          cursor.fetch((error2, result) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result, null);
            assert.deepEqual(result.length, 3);
            cursor.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });

    it('...should error when fetch is called after close().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions, (error1, cursor) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(cursor, null);
          assert.deepEqual(cursor instanceof Cursor, true);
          cursor.close((error2) => {
            assert.deepEqual(error2, null);
            cursor.fetch((error3, result) => {
              assert.notDeepEqual(error3, null);
              assert.deepEqual(result, null);
              done();
            });
          });
        });
      });
    });

  });

  describe('...with promises...', () => {
    it('...should not error when closed before calling .fetch().', async() => {
      let connection;
      try {
        connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
        const cursor = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions);
        assert.notDeepEqual(cursor, null);
        assert.deepEqual(cursor instanceof Cursor, true);
        assert.deepEqual(cursor.noData, false);
        await cursor.close();
      } catch (e) {
        assert.fail(e);
      } finally {
        await connection.close();
      }
    });

    it('...should not error when closed after calling .fetch().', async() => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const cursor = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions);
      assert.notDeepEqual(cursor, null);
      assert.deepEqual(cursor instanceof Cursor, true);
      const result = await cursor.fetch();
      assert.notDeepEqual(result, null);
      assert.deepEqual(result.length, 3);
      await cursor.close();
      await connection.close();
    });

    it('...should error when fetch is called after close().', async() => {
      assert.rejects(async () => {
        const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
        const cursor = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${TABLE_NAME}`, queryOptions);
        assert.notDeepEqual(cursor, null);
        assert.deepEqual(cursor instanceof Cursor, true);
        await cursor.close();
        const result = await cursor.fetch();
        assert.notDeepEqual(result, null);
        assert.deepEqual(result.length, 3);
        await connection.close();
      });
    });
  });
});
