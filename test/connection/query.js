/* eslint-env node, mocha */
const assert         = require('assert');
const odbc           = require('../../');
const { Cursor }     = require('../../lib/Cursor');

describe('.query(sql, [parameters], [options], [callback])...', () => {
  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const QUERY_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to connection.query({string}, {array}[optional], {object}[optional], {function}[optional]).',
    };
    const QUERY_CALLBACK = () => {};
    const QUERY_STRING = `SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`;

    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    assert.throws(() => {
      connection.query();
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, []);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, [], QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, 1);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, 1, QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should correctly identify function signature with .query({string}, {function}).', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {array}, {function})', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {array}, {object}, {function})', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(connection, undefined);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result, undefined);
          assert.deepEqual(result.length, 0);
          assert.deepEqual(result.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE ID = ?`, [1], { cursor: true }, (error3, cursor) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(cursor, undefined);
            assert.ok(cursor instanceof Cursor);
            cursor.close((error4) => {
              assert.deepEqual(error4, null);
              connection.close((error5) => {
                assert.deepEqual(error5, null);
                done();
              });
            })
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {object}, {function})', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(connection, undefined);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result, undefined);
          assert.deepEqual(result.length, 0);
          assert.deepEqual(result.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {cursor: true}, (error3, cursor) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(cursor, undefined);
            assert.ok(cursor instanceof Cursor);
            cursor.close((error4) => {
              assert.deepEqual(error4, null);
              connection.close((error5) => {
                assert.deepEqual(error5, null);
                done();
              });
            })
          });
        });
      });
    });
    describe('...testing query options...', () => {
      describe('...[cursor]...', () => {
        it('...should return an error if cursor is not a boolean or string', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: 1 }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                    assert.deepEqual(cursor5, undefined);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                      assert.deepEqual(cursor6, undefined);
                      connection.close((error7) => {
                        assert.deepEqual(error7, null);
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
      describe('...[fetchSize]...', () => {
        it('...should return an error if fetchSize is not a number', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      connection.close((error7) => {
                        assert.deepEqual(error7, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
        it('...should return an error if fetchSize is less-than or equal to 0', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .fetchSize must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .fetchSize must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.close((error5) => {
                    assert.deepEqual(error5, null);
                    done();
                  });
                });
              });
            });
          });
        });
      });
      describe('...[timeout]...', () => {
        it('...should return an error if timeout is not a number', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      connection.close((error7) => {
                        assert.deepEqual(error7, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
        it('...should return an error if timeout is less-than or equal to 0', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .timeout must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .timeout must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.close((error5) => {
                    assert.deepEqual(error5, null);
                    done();
                  });
                });
              });
            });
          });
        });
      });
      describe('...[initialBufferSize]...', () => {
        it('...should return an error if initialBufferSize is not a number', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      connection.close((error7) => {
                        assert.deepEqual(error7, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
        it('...should return an error if initialBufferSize is less-than or equal to 0', (done) => {
          odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(connection, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .initialBufferSize must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .initialBufferSize must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  connection.close((error5) => {
                    assert.deepEqual(error5, null);
                    done();
                  });
                });
              });
            });
          });
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should correctly identify function signature with .query({string}).', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
    it('...should correctly identify function signature with .query({string}, {array})', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
    it('...should correctly identify function signature with .query({string}, {array}, {object})', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE ID = ?`, [1], {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await connection.close();
    });
    it('...should correctly identify function signature with .query({string}, {object})', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await connection.close();
    });
    describe('...testing query options...', () => {
      describe('...[cursor]...', () => {
        it('...should throw an error if cursor is not a boolean or string', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: 1 }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: null }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await connection.close();
        });
      });
      describe('...[fetchSize]...', () => {
        it('...should throw an error if fetchSize is not a number', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await connection.close();
        });
        it('...should throw an error if fetchSize is less-than or equal to 0', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await connection.close();
        });
      });
      describe('...[timeout]...', () => {
        it('...should throw an error if timeout is not a number', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await connection.close();
        });
        it('...should throw an error if timeout is less-than or equal to 0', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await connection.close();
        });
      });
      describe('...[initialBufferSize]...', () => {
        it('...should throw an error if initialBufferSize is not a number', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await connection.close();
        });
        it('...should throw an error if initialBufferSize is less-than or equal to 0', async () => {
          const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
          const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: 0}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await assert.rejects(
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: -1}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await connection.close();
        });
      });
    });
  }); // ...with promises...
}); // '.query(sql, [parameters], [callback])...'
