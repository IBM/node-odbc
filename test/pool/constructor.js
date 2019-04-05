/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Pool } = require('../../');

describe('constructor...', () => {
  it('...should return the number of open Connections as was requested.', async () => {
    const pool = new Pool(`${process.env.CONNECTION_STRING}`);
  });
});
