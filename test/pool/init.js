/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Pool } = require('../../');

describe('.init([callback])...', () => {
  it('...with callbacks...', () => {
    it('...should create the number of connections passed to the constructor.', () => {

    });
    it('...should create open connections.', () => {

    });
    it('...should not create connections with a bad connection string.', () => {

    });
    it('...should not create connections if a bad number of initial connections is created.', () => {

    });
  });
  it('...should t', async () => {
    const pool = new Pool(`${process.env.CONNECTION_STRING}`);
    pool.init((error1) => {
      assert.deepEqual(error1, null);
    });
  });
});
