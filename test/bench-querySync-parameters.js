var common = require("./common")
, odbc = require("../")
, db = new odbc.Database();

db.open(common.connectionString, function(err){ 
  if (err) {
    console.error(err);
    process.exit(1);
  }
  
  issueQuery();
});

function issueQuery() {
  var count = 0
  , iterations = 10000
  , time = new Date().getTime();

  for (var x = 0; x < iterations; x++) {
    db.querySync("select ? + ?, ? as test", [Math.floor(Math.random() * 1000), Math.floor(Math.random() * 1000), "This is a string"]); 
  }

  var elapsed = new Date().getTime() - time;
  console.log("%d queries issued in %d seconds, %d/sec", iterations, elapsed/1000, Math.floor(iterations/(elapsed/1000)));

  db.close(function () {});
}
