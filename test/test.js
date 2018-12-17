const assert = require('assert');
const odbc = require('../build/Release/odbc');
const cn = `DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8`;

describe('node-odbc', function() {

  before(function() {
    const connection = odbc.connectSync(cn);
    const results = connection.querySync("CREATE TABLE MARK.ODBCTESTS(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)");
    connection.closeSync();
  });

  after(function() {
    const connection = odbc.connectSync(cn);
    const results = connection.querySync("DROP TABLE MARK.ODBCTESTS");
    connection.closeSync();
  });

  describe('ODBC', function() {

    describe('odbc exported constants...', function() {

      it('...should have the correct values', function() {
        assert.equal(odbc.SQL_COMMIT, 0);
        assert.equal(odbc.SQL_ROLLBACK, 1);
        assert.equal(odbc.SQL_USER_NAME, 47);
        assert.equal(odbc.SQL_PARAM_INPUT, 1);
        assert.equal(odbc.SQL_PARAM_INPUT_OUTPUT, 2);
        assert.equal(odbc.SQL_PARAM_OUTPUT, 4);
      });

      it('...should be immutable', function() {
        odbc.SQL_COMMIT = "Commit";
        odbc.SQL_ROLLBACK = null;
        odbc.SQL_USERNAME = undefined;
        odbc.SQL_PARAM_INPUT = {};
        odbc.SQL_PARAM_INPUT_OUTPUT = -5;
        odbc.SQL_PARAM_OUTPUT = false;

        assert.equal(odbc.SQL_COMMIT, 0);
        assert.equal(odbc.SQL_ROLLBACK, 1);
        assert.equal(odbc.SQL_USER_NAME, 47);
        assert.equal(odbc.SQL_PARAM_INPUT, 1);
        assert.equal(odbc.SQL_PARAM_INPUT_OUTPUT, 2);
        assert.equal(odbc.SQL_PARAM_OUTPUT, 4);
      });

    });

    describe('odbc.connect()...', function() {

      it('...requires two parameters.', function() {

        assert.throws( function() { odbc.connect(); },
          {
            name: 'TypeError',
            message: 'connect(connectionString, callback) requires 2 parameters.'
          }
        );

        assert.throws( function() { odbc.connect("connection", function() {}, null); },
          {
            name: 'TypeError',
            message: 'connect(connectionString, callback) requires 2 parameters.'
          }
        );
      })

      it('...requires a connection string as the first parameter.', function() {
        assert.throws( function() { odbc.connect(null, function(err, conn) {}) },
          {
            name: 'TypeError',
            message: 'connect: first parameter must be a string.',
          }
        );
      });

      it('...requires a callback function as the second parameter', function() {
        assert.throws( function() { odbc.connect("connectionString", null) },
          {
            name: 'TypeError',
            message: 'connect: second parameter must be a function.',
          }
        );
      });

      it('...returns an open connection with a good connection string.', function() {
        assert.doesNotThrow(
          function() { odbc.connect(cn, function(error, conn) {
            assert.equal(error, null);
            assert.notEqual(conn, null);
            assert.equal(conn.isConnected, true);
            conn.closeSync();
          });
        });
      });

      it('...returns an error with a bad connection string.', function() {
        assert.doesNotThrow(
          function() { odbc.connect("badconnectionstring", function(error, conn) {
            assert.notEqual(error, null);
            assert.equal(conn, null);
          });
        });
      });

    });

    describe('odbc.connectSync()...', function() {

      it('...requires one parameter.', function() {

        assert.throws( function() { odbc.connectSync(); },
          {
            name: 'TypeError',
            message: 'connectSync(connectionString) requires 1 parameter.'
          }
        );

        assert.throws( function() { odbc.connectSync("connection", null); },
          {
            name: 'TypeError',
            message: 'connectSync(connectionString) requires 1 parameter.'
          }
        );
      })

      it('...requires a connection string as the first parameter', function() {
        assert.throws(
          function() { odbc.connectSync(function(err, conn) { }) },
          {
            name: 'TypeError',
            message: 'connectSync: first parameter must be a string.',
          }
        );
      });

      it('...returns an open connection with a good connection string.', function() {

        this.slow(1000);

        assert.doesNotThrow(function() {
          let conn = odbc.connectSync(cn);
          assert.notEqual(conn, null);
          assert.equal(conn.isConnected, true);
          conn.closeSync();
        });
      });

      it('...throws an error with a bad connection string.', function() {
        assert.throws(function() {
          let conn = odbc.connectSync('badConnectionString');
          assert.equal(conn, null);
        }, Error);

      });

    });
  }); //ODBC

  describe('Connection', function() {

    describe('querySync...', function() {

      it('...should be able to insert, select, and delete data', function(done) {

        this.slow(1000);

        odbc.connect(cn, function(error, connection) {
          assert.equal(error, null);

          assert.doesNotThrow( () => { connection.querySync("INSERT INTO MARK.ODBCTESTS(ID, NAME, AGE) VALUES(1, 'MARK', 28)") });
          let results = connection.querySync("SELECT * FROM MARK.ODBCTESTS WHERE ID = 1");
          let row = results[0];
          assert.equal(row['ID'], 1);
          assert.equal(row['NAME'], 'MARK');
          assert.equal(row['AGE'], 28);
        
          assert.doesNotThrow( () => { connection.querySync("DELETE FROM MARK.ODBCTESTS WHERE ID = 1") });
          results = connection.querySync("SELECT * FROM MARK.ODBCTESTS WHERE ID = 1");
          assert.equal(results.length, 0);
        
          connection.closeSync();
          done();
        });
      });
    });
  }); // Connection

  describe('Statement', function() {

    describe('prepare/bind/execute...', function() {

      it('...should query even if there are no parameters to bind.', function() {
        odbc.connect(cn, function(error, connection) {
          assert.equal(error, null);
          assert.notEqual(connection, null);

          connection.createStatement(function(error, statement) {
            assert.notEqual(statement, null);

            statement.prepare('INSERT INTO MARK.ODBCTESTS(ID, NAME, AGE) VALUES(2, "DINGO", 19)', function(error) {
              assert.equal(error, null);

              let result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2');
              assert.equal(result.length, 0);
              statement.execute(function(error, result) {
                assert.equal(error, null);

                result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2');
                assert.equal(result.length, 1);
                let row = result[0];
                assert.equal(row['ID'], 2);
                assert.equal(row['NAME'], "DINGO");
                assert.equal(row["AGE"], 19);

                statement.closeSync();

                connection.query('DELETE FROM MARK.ODBCTESTS WHERE ID = 2', function(error, result) {
                  assert.equal(error, null);
                  connection.query('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2', function(error, result) {
                    assert.equal(error, null);
                    assert.equal(result.length, 0);
                    connection.closeSync();
                  });
                });
              });
            });
          });
        });
      });

      // it('...should bind with an array of values.', function() {
      //   odbc.connect(cn, function(error, connection) {
      //     assert.equal(error, null);
      //     assert.notEqual(connection, null);

      //     connection.createStatement(function(error, statement) {
      //       assert.notEqual(statement, null);

      //       statement.prepare('INSERT INTO MARK.ODBCTESTS(ID, NAME, AGE) VALUES(2, "DINGO", 19)', function(error) {
      //         assert.equal(error, null);

      //         let result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2');
      //         assert.equal(result.length, 0);
      //         statement.execute(function(error, result) {
      //           assert.equal(error, null);

      //           result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2');
      //           assert.equal(result.length, 1);
      //           let row = result[0];
      //           assert.equal(row['ID'], 2);
      //           assert.equal(row['NAME'], "DINGO");
      //           assert.equal(row["AGE"], 19);

      //           statement.closeSync();

      //           connection.query('DELETE FROM MARK.ODBCTESTS WHERE ID = 2', function(error, result) {
      //             assert.equal(error, null);
      //             connection.query('SELECT * FROM MARK.ODBCTESTS WHERE ID = 2', function(error, result) {
      //               assert.equal(error, null);
      //               assert.equal(result.length, 0);
      //               connection.closeSync();
      //             });
      //           });
      //         });
      //       });
      //     });
      //   });
      // });

      it('...should bind with an array of arrays.', function(done) {

        odbc.connect(cn, function(error, connection) {
          assert.equal(error, null);
          assert.notEqual(connection, null);

          connection.createStatement(function(error, statement) {
            assert.notEqual(statement, null);

            statement.prepare('INSERT INTO MARK.ODBCTESTS(ID, NAME, AGE) VALUES(?, ?, ?)', function(error) {
              assert.equal(error, null);
              console.log("hello2?");
              statement.bind([3, "JAX", 88], function(error) {
                assert.equal(error, null);

                let result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 3');
                assert.equal(result.length, 0);
                statement.execute(function(error, result) {
                  assert.equal(error, null);
                  console.log("hello3?");

                  result = connection.querySync('SELECT * FROM MARK.ODBCTESTS WHERE ID = 3');
                  assert.equal(result.length, 1);
                  let row = result[0];
                  console.log("ELLO");
                  assert.equal(row['ID'], 3);
                  assert.equal(row['NAME'], "JAX");
                  assert.equal(row["AGE"], 88);

                  statement.closeSync();

                  connection.query('DELETE FROM MARK.ODBCTESTS WHERE ID = 3', function(error, result) {
                    assert.equal(error, null);
                    connection.query('SELECT * FROM MARK.ODBCTESTS WHERE ID = 3', function(error, result) {
                      assert.equal(error, null);
                      assert.equal(result.length, 0);
                      connection.closeSync();
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
  }); // Statement
});
