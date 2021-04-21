/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../');

describe('.query...', () => {
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
});
