/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');
const { Connection } = require('../../lib/Connection');

describe('constructor...', () => {
  it('...should return the number of open Connections as was requested.', (done) => {
    odbc.pool(`${process.env.CONNECTION_STRING}`, (error, pool) => {
      assert.deepEqual(error, null);
      assert.notDeepEqual(pool, null);
      setTimeout(() => {
        assert.deepEqual(pool.freeConnections.length, 10);
        done();
      }, 5000);
    });
  });
  it('...should have at least one free connection when .connect is called', (done) => {
    odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
      assert.deepEqual(error1, null);
      pool.connect((error2, connection) => {
        assert.deepEqual(error2, null);
        assert.deepEqual(connection instanceof Connection, true);
        done();
      });
    });
  });
});
