/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');
const { Cursor } = require('../../lib/Cursor');

describe('.execute([calback])...', () => {
  let connection = null;

  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const statement = await connection.createStatement();

    const EXECUTE_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.execute({object}[optional], {function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};
    const PREPARE_SQL = `INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`;

    assert.throws(() => {
      statement.execute(PREPARE_SQL);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.execute(PREPARE_SQL, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.execute(1);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.execute(1, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
  });
  describe('...with callbacks...', () => {
    it('...should execute if a valid SQL string has been prepared and valid values bound.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.deepEqual(error3, null);
            statement.execute((error4, result4) => {
              assert.deepEqual(error4, null);
              assert.notDeepEqual(result4, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result5, null);
                assert.deepEqual(result5.length, 1);
                assert.deepEqual(result5[0].ID, 1);
                assert.deepEqual(result5[0].NAME, 'bound');
                assert.deepEqual(result5[0].AGE, 10);
                done();
              });
            });
          });
        });
      });
    });
    it('...should execute if bind has not been called and the prepared statement has no parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.execute((error4, result4) => {
            assert.deepEqual(error4, null);
            assert.notDeepEqual(result4, null);
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
              assert.deepEqual(error5, null);
              assert.notDeepEqual(result5, null);
              assert.deepEqual(result5.length, 1);
              assert.deepEqual(result5[0].ID, 1);
              assert.deepEqual(result5[0].NAME, 'bound');
              assert.deepEqual(result5[0].AGE, 10);
              done();
            });
          });
        });
      });
    });
    it('...should not execute if prepare has not been called.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.execute((error2, result2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          assert.deepEqual(result2, null);
          done();
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
    it('...should return a Cursor instead of a result set if {cursor: true} has been passed as an option.', (done) => {
      connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error1) => {
        assert.deepEqual(error1, null);
        connection.createStatement((error2, statement) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(statement, null);
          statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3) => {
            assert.deepEqual(error3, null);
            statement.execute({cursor: true}, (error4, cursor) => {
              assert.deepEqual(error4, null);
              assert.deepEqual(cursor instanceof Cursor, true);
              cursor.fetch((error5, result1) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result1, null);
                assert.deepEqual(result1.length, 1);
                assert.deepEqual(result1[0].ID, 1);
                assert.deepEqual(result1[0].NAME, 'bound');
                assert.deepEqual(result1[0].AGE, 10);
                cursor.fetch((error6, result2) => {
                  assert.deepEqual(error6, null);
                  assert.notDeepEqual(result2, null);
                  assert.deepEqual(result2.length, 0);
                  assert.deepEqual(cursor.noData, true);
                  cursor.close((error7) => {
                    assert.deepEqual(error7, null);
                    statement.close((error8) => {
                      assert.deepEqual(error8, null);
                      done();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...should return a Cursor instead of a result set if {fetchSize: <number>} has been passed as an option.', (done) => {
      connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error1) => {
        assert.deepEqual(error1, null);
        connection.createStatement((error2, statement) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(statement, null);
          statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3) => {
            assert.deepEqual(error3, null);
            statement.execute({fetchSize: 5}, (error4, cursor) => {
              assert.deepEqual(error4, null);
              assert.deepEqual(cursor instanceof Cursor, true);
              cursor.fetch((error5, result1) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result1, null);
                assert.deepEqual(result1.length, 1);
                assert.deepEqual(result1[0].ID, 1);
                assert.deepEqual(result1[0].NAME, 'bound');
                assert.deepEqual(result1[0].AGE, 10);
                cursor.fetch((error6, result2) => {
                  assert.deepEqual(error6, null);
                  assert.notDeepEqual(result2, null);
                  assert.deepEqual(result2.length, 0);
                  assert.deepEqual(cursor.noData, true);
                  cursor.close((error7) => {
                    assert.deepEqual(error7, null);
                    statement.close((error8) => {
                      assert.deepEqual(error8, null);
                      done();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...should leave a valid Statement after a Cursor generated from that Statement is closed.', (done) => {
      connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error1, result1) => {
        assert.deepEqual(error1, null);
        connection.createStatement((error2, statement) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(statement, null);
          statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3) => {
            assert.deepEqual(error3, null);
            statement.execute({cursor: true}, (error4, cursor) => {
              assert.deepEqual(error4, null);
              assert.deepEqual(cursor instanceof Cursor, true);
              cursor.fetch((error5, result1) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result1, null);
                assert.deepEqual(result1.length, 1);
                assert.deepEqual(result1[0].ID, 1);
                assert.deepEqual(result1[0].NAME, 'bound');
                assert.deepEqual(result1[0].AGE, 10);
                cursor.fetch((error6, result2) => {
                  assert.deepEqual(error6, null);
                  assert.notDeepEqual(result2, null);
                  assert.deepEqual(result2.length, 0);
                  assert.deepEqual(cursor.noData, true);
                  cursor.close((error7) => {
                    assert.deepEqual(error7, null);
                    // use the same prepared statement, but this time without a Cursor
                    statement.execute((error8, statementResult) => {
                      assert.deepEqual(error8, null);
                      assert.notDeepEqual(statementResult, null);
                      assert.deepEqual(statementResult.length, 1);
                      assert.deepEqual(statementResult[0].ID, 1);
                      assert.deepEqual(statementResult[0].NAME, 'bound');
                      assert.deepEqual(statementResult[0].AGE, 10);
                      statement.close((error9) => {
                        assert.deepEqual(error9, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...should accept a timeout option without crashing', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2) => {
          assert.deepEqual(error2, null);
          statement.execute({timeout: 10}, (error3, result) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(result, null);
            statement.close((error4) => {
              assert.deepEqual(error4, null);
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
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0].ID, 1);
      assert.deepEqual(result2[0].NAME, 'bound');
      assert.deepEqual(result2[0].AGE, 10);
    });
    it('...should not execute if prepare has not been called.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await assert.rejects(async () => {
        await statement.execute();
      });
    });
    it('...should not execute if bind has not been called and the prepared statement has parameters.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await assert.rejects(async () => {
        await statement.execute();
      });
    });
    it('...should not execute if bind values are incompatible with the fields they are binding to.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind(['ID', 10, 'AGE']);
      await assert.rejects(async () => {
        await statement.execute();
      });
    });
    it('...should return a Cursor instead of a result set if {cursor: true} has been passed as an option.', async () => {
      await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`);
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      const cursor = await statement.execute({cursor: true});
      assert.deepEqual(cursor instanceof Cursor, true);
      let result = await cursor.fetch();
      assert.deepEqual(result.length, 1);
      assert.deepEqual(result[0].ID, 1);
      assert.deepEqual(result[0].NAME, 'bound');
      assert.deepEqual(result[0].AGE, 10);
      result = await cursor.fetch();
      assert.deepEqual(result.length, 0);
      assert.deepEqual(cursor.noData, true);
      await cursor.close();
      await statement.close();
    });
    it('...should return a Cursor instead of a result set if {fetchSize: <number>} has been passed as an option.', async () => {
      await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`);
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      const cursor = await statement.execute({fetchSize: 5});
      assert.deepEqual(cursor instanceof Cursor, true);
      let result = await cursor.fetch();
      assert.deepEqual(result.length, 1);
      assert.deepEqual(result[0].ID, 1);
      assert.deepEqual(result[0].NAME, 'bound');
      assert.deepEqual(result[0].AGE, 10);
      result = await cursor.fetch();
      assert.deepEqual(result.length, 0);
      assert.deepEqual(cursor.noData, true);
      await cursor.close();
      await statement.close();
    });
    it('...should leave a valid Statement after a Cursor generated from that Statement is closed.', async () => {
      await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`);
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      const cursor = await statement.execute({fetchSize: 5});
      assert.deepEqual(cursor instanceof Cursor, true);
      let cursorResult = await cursor.fetch();
      assert.deepEqual(cursorResult.length, 1);
      assert.deepEqual(cursorResult[0].ID, 1);
      assert.deepEqual(cursorResult[0].NAME, 'bound');
      assert.deepEqual(cursorResult[0].AGE, 10);
      cursorResult = await cursor.fetch();
      assert.deepEqual(cursorResult.length, 0);
      assert.deepEqual(cursor.noData, true);
      await cursor.close();
      // use the same prepared statement, but this time without a Cursor
      const statementResult = await statement.execute();
      assert.deepEqual(statementResult.length, 1);
      assert.deepEqual(statementResult[0].ID, 1);
      assert.deepEqual(statementResult[0].NAME, 'bound');
      assert.deepEqual(statementResult[0].AGE, 10);
      await statement.close();
    });
    it('...should accept a timeout option without crashing', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      const result = await statement.execute({timeout: 10});
      assert.notDeepEqual(result, null);
      await statement.close();
    });
  }); // '...with promises...'
});
