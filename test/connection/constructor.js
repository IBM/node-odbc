/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('odbc.connect(connectionString)...', () => {
  it('...should return an open Connection when passed a valid connection string.', async () => {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    assert.notDeepEqual(connection, null);
    // assert.deepEqual(connection instanceof odbc.Connection, true);
    assert.deepEqual(connection.connected, true);
    assert.deepEqual(connection.autocommit, true);
    await connection.close();
  });
  it('...should throw an Error when passed an invalid connection string.', async () => {
    assert.rejects(async () => {
      const connection = await odbc.connect('abc123!@#'); // eslint-disable-line no-unused-vars
    });
  });
});
