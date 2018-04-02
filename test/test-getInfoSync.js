var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert");

db.openSync(common.connectionString);
console.log(common);
var userName = db.conn.getInfoSync(odbc.SQL_USER_NAME);
assert.equal(userName, common.user);

