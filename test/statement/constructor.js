/* eslint-env node, mocha */
/* eslint-disable global-require */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

describe('new Connection(connectionString)...', () => {
  it('...should return an open Connection when passed a valid connection string.', async () => {
    const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    assert.notDeepEqual(connection, null);
    assert.deepEqual(connection instanceof Connection, true);
    assert.deepEqual(connection.connected, true);
    assert.deepEqual(connection.autocommit, true);
    await connection.close();
  });
  it('...should throw an Error when passed an invalid connection string.', async () => {
    assert.throws(() => {
      const connection = new Connection('abc123!@#'); // eslint-disable-line no-unused-vars
    });
  });
});
