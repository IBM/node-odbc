/* eslint-env node, mocha */
const assert = require('assert');
const odbc   = require('../../lib/odbc');

describe('.commit([callback])...', () => {
  describe('...with callbacks...', () => {
    it('...should commit all queries from after beginTransaction() was called.', (done) => {
      try {
        odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
          assert.deepEqual(error, null);
          connection.beginTransaction((error1) => {
            assert.deepEqual(error1, null);
            connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`, (error2, result2) => {
              assert.deepEqual(error2, null);
              assert.notDeepEqual(result2, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
                assert.deepEqual(error3, null);
                assert.notDeepEqual(result3, null);
                assert.deepEqual(result3.length, 1);
                assert.deepEqual(result3[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
                connection.commit((error4) => {
                  assert.deepEqual(error4, null);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                    assert.deepEqual(error5, null);
                    assert.notDeepEqual(result5, null);
                    assert.deepEqual(result5.length, 1);
                    assert.deepEqual(result5[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
                    connection.close((error6) => {
                      assert.deepEqual(error6, null);
                      done();
                    });
                  });
                });
              });
            });
          });
        });
      } catch (error) {
        done(error);
      }
    });
    it('...shouldn\'t have adverse effects if called outside beginTransaction().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          connection.commit((error2) => {
            assert.deepEqual(error2, null);
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
              assert.deepEqual(error3, null);
              assert.deepEqual(result3.length, 1);
              assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
              connection.commit((error4) => {
                assert.deepEqual(error4, null);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                  assert.deepEqual(error5, null);
                  assert.deepEqual(result5.length, 1);
                  assert.deepEqual(result5[0], { ID: 1, NAME: 'committed', AGE: 10 });
                  connection.close((error6) => {
                    assert.deepEqual(error6, null);
                    done();
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...shouldn\'t commit if called after a transaction is already ended with rollback().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.beginTransaction((error1) => {
          assert.deepEqual(error1, null);
          connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            connection.rollback((error3) => {
              assert.deepEqual(error3, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error4, result4) => {
                assert.deepEqual(error4, null);
                assert.notDeepEqual(result4, null);
                assert.deepEqual(result4.length, 0);
                connection.commit((error5) => {
                  assert.deepEqual(error5, null);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error6, result6) => {
                    assert.deepEqual(error6, null);
                    assert.notDeepEqual(result6, null);
                    assert.deepEqual(result6.length, 0);
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
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should commit all queries from after beginTransaction() was called.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'comitted', 10)`);
      assert.notDeepEqual(result1, null);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'comitted', AGE: 10 });
      await connection.commit();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'comitted', AGE: 10 });
      await connection.close();
    });
    it('...shouldn\'t have adverse effects if called outside beginTransaction().', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      await connection.commit();
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.commit();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
    it('...shouldn\'t commit if called after a transaction is already ended with rollback().', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      await connection.rollback();
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 0);
      await connection.commit();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.length, 0);
      await connection.close();
    });
  }); // '...with promises...'
}); // '.rollback(callback)...'
