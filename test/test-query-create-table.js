var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

  console.log("UHH");
db.openSync(common.connectionString);
console.log("sync opened");
common.dropTables(db, function(err, data, morefollowing) {
  common.createTables(db, function (err, data, morefollowing) {
    console.log("in create tables");
    //console.log(arguments);
    db.closeSync();
  });
});

