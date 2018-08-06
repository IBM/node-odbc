var common = require("../test/common")
  , odbc = require("../")
  , db = new odbc.ODBC()
  , assert = require("assert")
  , exitCode = 0
  ;

db.createConnection(function (err, conn) {
    if (err) { console.log(err); return; }
  conn.openSync(common.connectionString);

  conn.createStatement(function (err, stmt) {
    if (err) { console.log(err); return; }
    var r, result, caughtError;

    //try excuting without preparing or binding.
    try {
      result = stmt.executeSync();
    }
    catch (e) {
      caughtError = e;
    }

    try {
      assert.ok(caughtError);
    }
    catch (e) {
      console.log("e message is " + e.message);
      exitCode = 1;
    }
    
    //try incorrectly binding a string and then executeSync
    try {
      r = stmt.bind("select 1 + 1 as col1");
    }
    catch (e) {
      caughtError = e;
    }
    
    // try {
      assert.equal(caughtError.message, "Argument 1 must be an Array");
      
      r = stmt.prepareSync("SELECT * from MARK.BOOKS WHERE TITLE = ?");
      assert.equal(r, true, "prepareSync did not return true");
      
      r = stmt.bindSync(["Holes"]);
      assert.equal(r, true, "bindSync did not return true");
      
      result = stmt.executeSync();
      //assert.equal(result.constructor.name, "ODBCResult");
      
      r = result.fetchAllSync();
      assert.deepEqual(r, [ { col1: 3 } ]);
      
      r = result.closeSync();
      assert.equal(r, true, "closeSync did not return true");
    
      
      console.log("R is " + r);
    // }
    // catch (e) {
    //   console.log("e.stack is " + e.stack);
      
    //   exitCode = 1;
    // }
    
    conn.closeSync();
    
    if (exitCode) {
      console.log("failed");
    }
    else {
      console.log("success");
    }
    
    process.exit(exitCode);
  });
});