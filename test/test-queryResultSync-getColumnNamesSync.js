var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

db.openSync(common.connectionString);
assert.equal(db.connected, true);

var rs = db.queryResultSync("select 1 as SomeIntField, 'string' as someStringField");

assert.deepEqual(rs.getColumnNamesSync(), ['SomeIntField', 'someStringField']);

db.closeSync();
