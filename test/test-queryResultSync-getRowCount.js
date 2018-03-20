var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

db.openSync(common.connectionString);
assert.equal(db.connected, true);

common.dropTables(db, function () {
  common.createTables(db, function (err, data) {
    if (err) {
      console.log(err);

      return finish(2);
    }

    var rs = db.queryResultSync("insert into " + common.tableName + " (colint, coltext) VALUES (100, 'hello world')");
    assert.equal(rs.constructor.name, "ODBCResult");

    assert.equal(rs.getRowCountSync(), 1);

    common.dropTables(db, function () {
      return finish(0);
    });
  });
});

function finish(retValue) {
  console.log("finish exit value: %s", retValue);

  db.closeSync();
  process.exit(retValue || 0);
}
