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

  function iteration() {
    db.query("select ? + ?, ? as test", [Math.floor(Math.random() * 1000), Math.floor(Math.random() * 1000), "This is a string"], cb); 
  }

  iteration()
    
  function cb (err, result) {
    if (err) {
      console.error("query: ", err);
      return finish();
    }
    
    if (++count == iterations) {
      var elapsed = new Date().getTime() - time;
          
      console.log("%d queries issued in %d seconds, %d/sec", count, elapsed/1000, Math.floor(count/(elapsed/1000)));
      return finish();
    } else {
      iteration();
    }
  }
  
  function finish() {
    db.close(function () {});
  }
}
