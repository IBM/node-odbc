/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

const connection = new Connection(`${process.env.CONNECTION_STRING}`);

describe.only('.bind(parameters, [calback])...', () => {
  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    // const connection = new Connection(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();

    const BIND_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.bind({array}, {function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};

    assert.throws(() => {
      statement.prepare();
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(1);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(1, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(null);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(null, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(undefined);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(undefined, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare({});
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare({}, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);

    // await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should prepare a valid SQL string', (done) => {
      // const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          done();
        });
      });
    });
    it('...should return an error with an invalid SQL string', (done) => {
      // const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare('abc123!@#', (error2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          done();
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should prepare a valid SQL string', async () => {
      // const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      const statement = await connection.createStatement();
      assert.doesNotReject(async () => {
        await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      });
    });
    it('...should return an error with an invalid SQL string', async () => {
      // const connection = new Connection(`${process.env.CONNECTION_STRING}`);
      const statement = await connection.createStatement();
      assert.rejects(async () => {
        await statement.prepare('abc123!@#');
      });
    });
  }); // '...with promises...'
});
