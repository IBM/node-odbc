var common = require("../test/common")
  , odbc = require("../")
  , db = new odbc.ODBC()
  , assert = require("assert")
  , exitCode = 0
  ;

db.createConnection(function (err, conn) {

  db.openSync(common.connectionString);
assert.equal(db.connected, true);

var rs = db.queryResultSync("select 1 as SomeIntField, 'string' as someStringField");

assert.deepEqual(rs.getColumnNamesSync(), ['SomeIntField', 'someStringField']);

db.closeSync();
  });

