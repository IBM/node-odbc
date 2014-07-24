var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

db.openSync(common.connectionString);
issueQuery(100001);
issueQuery(3000);
issueQuery(4000);
issueQuery(5000);
issueQuery(8000);
finish(0);

function issueQuery(length) {
  var count = 0
    , time = new Date().getTime()
    , stmt
    , result
    , data
    , str = ''
    ;
  
  var set = 'abcdefghijklmnopqrstuvwxyz';
  
  for (var x = 0; x < length; x++) {
    str += set[x % set.length];
  }
  
  assert.doesNotThrow(function () {
    stmt = db.prepareSync('select ? as longString');
  });
  
  assert.doesNotThrow(function () {
    stmt.bindSync([str]);
  });
  
  assert.doesNotThrow(function () {
    result = stmt.executeSync();
  });
  
  assert.doesNotThrow(function () {
    data = result.fetchAllSync();
  });
  
  console.log('expected length: %s, returned length: %s', str.length, data[0].longString.length);
  
  for (var x = 0; x < str.length; x++) {
    if (str[x] != data[0].longString[x]) {
      console.log(x, str[x], data[0].longString[x]);
      
      assert.equal(str[x], data[0].longString[x]);
    }
  }
  
  assert.equal(data[0].longString, str);
}

function finish(exitCode) {
  db.closeSync();
  
  console.log("connection closed");
  process.exit(exitCode || 0);
}
