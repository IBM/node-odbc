/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../lib/Legacy/legacy.js');

describe.skip('Tests for v1.0 API of \'odbc\' package', () => {
  it('test-global-open-close', (done) => {
    odbc.open(`${process.env.CONNECTION_STRING}`, (err, conn) => {
      if (err) { console.error(err); }
      assert.deepEqual(err, null);
      assert.deepEqual(conn.constructor.name, 'Database');
      assert.deepEqual(conn instanceof odbc.Database, true);
      conn.close((error) => {
        assert.deepEqual(error, null);
        done();
      });
    });
  }); // test-global-open-close

  it('test-bad-connection-string', (done) => {
    const db = new odbc.Database();
    assert.throws(() => {
      db.openSync('this is wrong');
    });

    assert.equal(db.connected, false);

    db.open('this is wrong', (err) => {
      assert.deepEqual(err instanceof Error, true);
      assert.deepEqual(err.message, '"errors": [{ "error": "[node-odbc] Error in ConnectAsyncWorker", "message": "[unixODBC][Driver Manager]Data source name not found, and no default driver specified", "SQLState": "IM002"}]');
      assert.equal(db.connected, false);
      done();
    });
  }); // test-bad-connection-string

  it('test-binding-connection-loginTimeout', () => {
    // TODO: db = odbc.ODBC(), but I don't expose an ODBC anymore
    const db = new odbc.Database();
    db.createConnection((err1, conn) => {
      const connection = conn;
      // loginTimeout should be 5 by default as set in C++
      assert.equal(conn.loginTimeout, 5);

      // test the setter and getter
      connection.loginTimeout = 1234;
      assert.equal(conn.loginTimeout, 1234);

      // set the time out to something small
      connection.loginTimeout = 1;
      assert.equal(conn.loginTimeout, 1);

      conn.open(`${process.env.CONNECTION_STRING}`, (err2) => {
        // TODO: it would be nice if we could somehow
        // force a timeout to occurr, but most testing is
        // done locally and it's hard to get a local server
        // to not accept a connection within one second...

        console.log(err2);
        conn.close(() => {

        });
      });
    });
  });

  it('test-getInfoSync', () => {
    const db = new odbc.Database();
    db.openSync(`${process.env.CONNECTION_STRING}`);
    const username = db.conn.getInfoSync(odbc.SQL_USER_NAME);
    assert.deepEqual(username, `${process.env.DB_USERNAME}`);
  }); // test-getInfoSync

  it('test-open-close', (done) => {
    const db = new odbc.Database();
    assert.deepEqual(db.connected, false);

    db.query(`SELECT * FROM ${process.env.DB_TABLE}`, (err1, rs, moreResultSets) => {
      assert.deepEqual(err1, { message: 'Connection not open.' });
      assert.deepEqual(rs, []);
      assert.deepEqual(moreResultSets, false);
      assert.deepEqual(db.connected, false);

      db.open(`${process.env.CONNECTION_STRING}`, (err2) => {
        assert.deepEqual(err2, null);
        assert.deepEqual(db.connected, true);

        db.close(() => {
          assert.deepEqual(db.connected, false);

          db.query(`SELECT * FROM ${process.env.DB_TABLE}`, (err3, rs2, moreResultSets2) => {
            assert.deepEqual(err3, { message: 'Connection not open.' });
            assert.deepEqual(rs2, []);
            assert.deepEqual(moreResultSets2, false);
            assert.deepEqual(db.connected, false);
            done();
          });
        });
      });
    });
  }); // test-open-close

  it('test-prepareSync', () => {
    const db = new odbc.Database();
    db.openSync(`${process.env.CONNECTION_STRING}`);
    assert.equal(db.connected, true);

    const stmt = db.prepareSync('select ? as col1, ? as col2, ? as col3');
    assert.equal(stmt.constructor.name, 'ODBCStatement');

    stmt.bindSync(['hello world', 1, null]);

    stmt.execute((err, result) => {
      assert.equal(err, null);
      assert.equal(result.constructor.name, 'ODBCResult');

      result.fetchAll((err, data) => {
        assert.equal(err, null);
        console.log(data);

        result.closeSync();

        db.closeSync();
        assert.deepEqual(data, [{ col1: 'hello world', col2 : 1, col3 : null }]);
      });
    });
  })
});
