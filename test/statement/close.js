/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

describe('.close([calback])...', () => {
  let connection = null;

  beforeEach(() => {
    connection = new Connection(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const statement = await connection.createStatement();

    const EXECUTE_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.close({function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};
    const SQL_STRING = `INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`;

    assert.throws(() => {
      statement.close(SQL_STRING);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(SQL_STRING, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(1);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(1, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(null);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(null, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close({});
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close({}, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
  });
  describe('...with callbacks...', () => {
    it.only('...should close a newly created statement.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.close((error2) => {
          assert.deepEqual(error2, null);
          done();
        });
      });
    });
    it.only('...should close after a statement has been prepared with parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.close((error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it.only('...should close after a statement has been prepared without parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.close((error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it.only('...should close after a statement has been prepared and values bound.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.deepEqual(error3, null);
            statement.close((error4) => {
              assert.deepEqual(error4, null);
              done();
            });
          });
        });
      });
    });
    it('...should not execute if bind has not been called and the prepared statement has parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.execute((error3, result3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            assert.deepEqual(result3, null);
            done();
          });
        });
      });
    });
    it('...should not execute if bind values are incompatible with the fields they are binding to.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind(['ID', 10, 'AGE'], (error3) => {
            assert.deepEqual(error3, null);
            statement.execute((error4, result4) => {
              assert.notDeepEqual(error4, null);
              assert.deepEqual(error4 instanceof Error, true);
              assert.deepEqual(result4, null);
              done();
            });
          });
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should execute if a valid SQL string has been prepared and valid values bound.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind([1, 'bound', 10]);
      const result1 = await statement.execute();
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0].ID, 1);
      assert.deepEqual(result2[0].NAME, 'bound');
      assert.deepEqual(result2[0].AGE, 10);
    });
    it('...should execute if bind has not been called and the prepared statement has no parameters.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`);
      const result1 = await statement.execute();
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0].ID, 1);
      assert.deepEqual(result2[0].NAME, 'bound');
      assert.deepEqual(result2[0].AGE, 10);
    });
    it('...should not execute if prepare has not been called.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      assert.rejects(async () => {
        await statement.execute();
      });
    });
    it('...should not execute if bind has not been called and the prepared statement has parameters.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      assert.rejects(async () => {
        await statement.execute();
      });
    });
    it('...should not execute if bind values are incompatible with the fields they are binding to.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind(['ID', 10, 'AGE']);
      assert.rejects(async () => {
        await statement.execute();
      });
    });
  }); // '...with promises...'
});
