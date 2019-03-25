/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const { Connection } = require('../../');

describe.skip('.beginTransaction([callback])...', () => {
  let connection = null;

  beforeEach(() => {
    connection = new Connection(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  describe('...with callbacks...', () => {
    it('...should set .autocommit property to false.', (done) => {
      assert.deepEqual(connection.autocommit, true);
      connection.beginTransaction((error1) => {
        assert.deepEqual(error1, null);
        assert.deepEqual(connection.autocommit, false);
        done();
      });
    });
    it('...should be idempotent if called multiple times before rollback().', (done) => {
      connection.beginTransaction((error1) => {
        assert.deepEqual(error1, null);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          assert.deepEqual(result2.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(result3, null);
            assert.deepEqual(result3.length, 1);
            assert.deepEqual(result3.count, -1);
            assert.deepEqual(result3[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
            connection.beginTransaction((error4) => {
              assert.deepEqual(error4, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result5, null);
                assert.deepEqual(result5.length, 1);
                assert.deepEqual(result5.count, -1);
                assert.deepEqual(result5[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
                connection.rollback((error6) => {
                  assert.deepEqual(error6, null);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error7, result7) => {
                    assert.deepEqual(error7, null);
                    assert.notDeepEqual(result7, null);
                    assert.deepEqual(result7.length, 0);
                    assert.deepEqual(result7.count, -1);
                    done();
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...should be idempotent if called multiple times before commit().', (done) => {
      connection.beginTransaction((error1) => {
        assert.deepEqual(error1, null);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          assert.deepEqual(result2.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(result3, null);
            assert.deepEqual(result3.length, 1);
            assert.deepEqual(result3.count, -1);
            assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
            connection.beginTransaction((error4) => {
              assert.deepEqual(error4, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                assert.deepEqual(error5, null);
                assert.notDeepEqual(result5, null);
                assert.deepEqual(result5.length, 1);
                assert.deepEqual(result5.count, -1);
                assert.deepEqual(result5[0], { ID: 1, NAME: 'committed', AGE: 10 });
                connection.commit((error6) => {
                  assert.deepEqual(error6, null);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error7, result7) => {
                    assert.deepEqual(error7, null);
                    assert.notDeepEqual(result7, null);
                    assert.deepEqual(result7.length, 1);
                    assert.deepEqual(result7.count, -1);
                    assert.deepEqual(result7[0], { ID: 1, NAME: 'committed', AGE: 10 });
                    connection.close((closeError) => {
                      assert.deepEqual(closeError, null);
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
    it('...should make transactional queries visible only to the connection that the transaction was started on until commit() is called (at transactional isolation level \'read committed\').', (done) => {
      const connection1 = new Connection(`${process.env.CONNECTION_STRING};CMT=1`); // set commitment level to 1 (Read committed)
      const connection2 = new Connection(`${process.env.CONNECTION_STRING};CMT=1;CONCURRENTACCESSRESOLUTION=1`); // set commitment level to 1 (Read committed)
      connection1.beginTransaction((error0) => {
        assert.deepEqual(error0, null);
        connection1.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          assert.deepEqual(result2.count, 1);
          connection1.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} FETCH1`, (error3, result3) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(result3, null);
            assert.deepEqual(result3.length, 1);
            assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
            assert.deepEqual(result3.count, -1);
            connection2.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} FETCH2`, (error4, result4) => {
              assert.deepEqual(error4, null);
              assert.notDeepEqual(result4, null);
              assert.deepEqual(result4.length, 0);
              assert.deepEqual(result4.count, -1);
              connection1.commit((error5) => {
                assert.deepEqual(error5, null);
                connection2.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error6, result6) => {
                  assert.deepEqual(error6, null);
                  assert.notDeepEqual(result6, null);
                  assert.deepEqual(result6.length, 1);
                  assert.deepEqual(result6.count, -1);
                  assert.deepEqual(result6[0], { ID: 1, NAME: 'committed', AGE: 10 });
                  connection1.close((error7) => {
                    assert.deepEqual(error7, null);
                    connection2.close((error8) => {
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
    it('...shouldn\'t hide queries that occur on other connections.', (done) => {
      const connection1 = new Connection(`${process.env.CONNECTION_STRING}`);
      const connection2 = new Connection(`${process.env.CONNECTION_STRING}`);
      connection1.beginTransaction((error1) => {
        assert.deepEqual(error1, null);
        connection2.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          assert.deepEqual(result2.count, 1);
          connection1.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
            assert.deepEqual(error3, null);
            assert.notDeepEqual(result3, null);
            assert.deepEqual(result3.length, 1);
            assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
            assert.deepEqual(result3.count, -1);
            connection1.close((error4) => {
              assert.deepEqual(error4, null);
              connection2.close((error5) => {
                assert.deepEqual(error5, null);
                done();
              });
            });
          });
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should set .autocommit property to false.', async () => {
      assert.deepEqual(connection.autocommit, true);
      await connection.beginTransaction();
      assert.deepEqual(connection.autocommit, false);
    });
    it('...should be idempotent if called multiple times before rollback().', async () => {
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
      await connection.beginTransaction();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3.count, -1);
      assert.deepEqual(result3[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
      await connection.rollback();
      const result4 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result4, null);
      assert.deepEqual(result4.length, 0);
      assert.deepEqual(result4.count, -1);
    });
    it('...should be idempotent if called multiple times before commit().', async () => {
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.beginTransaction();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3.count, -1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.commit();
      const result4 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result4, null);
      assert.deepEqual(result4.length, 1);
      assert.deepEqual(result4.count, -1);
      assert.deepEqual(result4[0], { ID: 1, NAME: 'committed', AGE: 10 });
    });
  }); // '...with promises...'
}); // '.beginTransaction([callback])...'
