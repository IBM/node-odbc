/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../');

describe('close()...', () => {
  describe('...with callbacks...', () => {
    it('...should close all connections in the Pool.', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(pool, null);
        setTimeout(() => {
          assert.deepEqual(pool.freeConnections.length, 10);
          pool.close((error2) => {
            assert.deepEqual(error2, null);
            assert.deepEqual(pool.freeConnections.length, 0);
            done();
          });
        }, 5000);
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should close all connections in the Pool.', async () => {
      const pool = await odbc.pool(`${process.env.CONNECTION_STRING}`);
      assert.notDeepEqual(pool, null);
      await new Promise(resolve => setTimeout(resolve, 5000));
      assert.deepEqual(pool.freeConnections.length, 10);
      await pool.close();
      assert.deepEqual(pool.freeConnections.length, 0);
    });
  }); // ...with promises...
});
