const util = require('util');
var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert");

db.openSync(common.connectionString);
console.log(common);
var userName = db.conn.getInfoSync(0);
assert.equal(userName, common.user);

