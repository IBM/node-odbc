/* eslint-env node, mocha */
const assert     = require('assert');
const odbc       = require('../../');
const { Cursor } = require('../../lib/Cursor');

describe('.query...', () => {
  describe('...with callbacks...', () => {
    it('...should correctly identify function signature with .query({string}, {function}).', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error, pool) => {
        assert.deepEqual(error, null);
        pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            pool.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {array}, {function})', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error, pool) => {
        pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            pool.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {array}, {object}, {function})', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(pool, undefined);
        pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result, undefined);
          assert.deepEqual(result.length, 0);
          assert.deepEqual(result.count, 1);
          pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE ID = ?`, [1], { cursor: true }, (error3, cursor) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(cursor, undefined);
            assert.ok(cursor instanceof Cursor);
            cursor.close((error4) => {
              assert.deepEqual(error4, null);
              pool.close((error5) => {
                assert.deepEqual(error5, null);
                done();
              });
            })
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {object}, {function})', (done) => {
      odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(pool, undefined);
        pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result, undefined);
          assert.deepEqual(result.length, 0);
          assert.deepEqual(result.count, 1);
          pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {cursor: true}, (error3, cursor) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(cursor, undefined);
            assert.ok(cursor instanceof Cursor);
            cursor.close((error4) => {
              assert.deepEqual(error4, null);
              pool.close((error5) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: 1 }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                    assert.deepEqual(cursor5, undefined);
                    pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .cursor must be a STRING or BOOLEAN value.'));
                      assert.deepEqual(cursor6, undefined);
                      pool.close((error7) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .fetchSize must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      pool.close((error7) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .fetchSize must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .fetchSize must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.close((error5) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .timeout must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      pool.close((error7) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .timeout must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .timeout must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.close((error5) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: true }, (error3, cursor3) => {
                assert.deepEqual(error3, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: {} }, (error4, cursor4) => {
                  assert.deepEqual(error4, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: null }, (error5, cursor5) => {
                    assert.deepEqual(error5, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                    assert.deepEqual(cursor5, undefined);
                    pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: () => {} }, (error6, cursor6) => {
                      assert.deepEqual(error6, TypeError('Connection.query options: .initialBufferSize must be a NUMBER value.'));
                      assert.deepEqual(cursor6, undefined);
                      pool.close((error7) => {
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
          odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
            assert.deepEqual(error1, null);
            assert.notDeepEqual(pool, null);
            pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              assert.deepEqual(result2.length, 0);
              pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: 0 }, (error3, cursor3) => {
                assert.deepEqual(error3, RangeError('Connection.query options: .initialBufferSize must be greater than 0.'));
                assert.deepEqual(cursor3, undefined);
                pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: -1 }, (error4, cursor4) => {
                  assert.deepEqual(error4, RangeError('Connection.query options: .initialBufferSize must be greater than 0.'));
                  assert.deepEqual(cursor4, undefined);
                  pool.close((error5) => {
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
  });
  describe('...with promises...', () => {
    it('...should correctly identify function signature with .query({string}).', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {array})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {array}, {object})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} WHERE ID = ?`, [1], {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await pool.close();
    });
    it('...should correctly identify function signature with .query({string}, {object})', async () => {
      const pool = await odbc.pool({
        connectionString: `${process.env.CONNECTION_STRING}`,
        initialSize: 1
      });
      const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const cursor = await pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {cursor: true});
      assert.notDeepEqual(cursor, null);
      assert.ok(cursor instanceof Cursor);
      await cursor.close();
      await pool.close();
    });
    describe('...testing query options...', () => {
      describe('...[cursor]...', () => {
        it('...should throw an error if cursor is not a boolean or string', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: 1 }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: null }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { cursor: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .cursor must be a STRING or BOOLEAN value.'
            }
          );
          await pool.close();
        });
      });
      describe('...[fetchSize]...', () => {
        it('...should throw an error if fetchSize is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .fetchSize must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if fetchSize is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { fetchSize: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .fetchSize must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
      describe('...[timeout]...', () => {
        it('...should throw an error if timeout is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .timeout must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if timeout is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: 0 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { timeout: -1 }),
            {
              name: 'RangeError',
              message: 'Connection.query options: .timeout must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
      describe('...[initialBufferSize]...', () => {
        it('...should throw an error if initialBufferSize is not a number', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: true }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: '1' }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, { initialBufferSize: () => {} }),
            {
              name: 'TypeError',
              message: 'Connection.query options: .initialBufferSize must be a NUMBER value.'
            }
          );
          await pool.close();
        });
        it('...should throw an error if initialBufferSize is less-than or equal to 0', async () => {
          const pool = await odbc.pool({
            connectionString: `${process.env.CONNECTION_STRING}`,
            initialSize: 1
          });
          const result1 = await pool.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: 0}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await assert.rejects(
            pool.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, {initialBufferSize: -1}),
            {
              name: 'RangeError',
              message: 'Connection.query options: .initialBufferSize must be greater than 0.'
            }
          );
          await pool.close();
        });
      });
    });
  }); // ...with promises...
});
