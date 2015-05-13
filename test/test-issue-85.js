var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  , util = require('util')
  , count = 0
  ;
 
var sql = (common.dialect == 'sqlite' || common.dialect =='mysql')
  ? 'select cast(-1 as signed) as test;'
  : 'select cast(-1 as int) as test;'
  ;

db.open(common.connectionString, function(err) {
  console.error(err || "Connected")

  if (!err) {
    db.query(sql, function (err, results, more) {
      assert.equal(err, null);
      assert.equal(results[0].test, -1);

      db.close(function(err) { console.log(err || "Closed") })
    })
  }
});
