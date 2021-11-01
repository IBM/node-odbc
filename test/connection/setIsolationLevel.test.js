/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');

describe.skip('.setIsolationLevel(isolationLevel, callback)...', () => {
  let connection = null;
  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });
  describe('...with callbacks...', () => {
    it('...should not error if no transaction has been started', (done) => {
      connection.setIsolationLevel(odbc.SQL_TXN_READ_UNCOMMITTED, (error) => {
        assert.strictEqual(error, null);
        done();
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should not error if no transaction has been started', async () => {
      await assert.doesNotReject(connection.setIsolationLevel(1));
    });
  }); // ...with promises...
});
