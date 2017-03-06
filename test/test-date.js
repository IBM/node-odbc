var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

var sqlite = /sqlite/i.test(common.connectionString);

db.open(common.connectionString, function(err) {
  assert.equal(err, null);
  assert.equal(db.connected, true);
  
  var dt = new Date();
  dt.setMilliseconds(0);  // MySQL truncates them.
  var ds = dt.toISOString().replace('Z','');
  var sql = "SELECT cast('" + ds + "' as datetime) as DT1";
  // XXX(bnoordhuis) sqlite3 has no distinct DATETIME or TIMESTAMP type.
  // 'datetime' in this expression is a function, not a type.
  if (sqlite) sql = "SELECT datetime('" + ds + "') as DT1";
  console.log(sql);
  
  db.query(sql, function (err, data) {
    assert.equal(err, null);
    assert.equal(data.length, 1);

    db.close(function () {
      assert.equal(db.connected, false);
      console.log(dt);
      console.log(data);
      
      //test selected data after the connection
      //is closed, in case the assertion fails
      if (sqlite) {
        assert.equal(data[0].DT1.constructor.name, "String", "DT1 is not an instance of a String object");
        assert.equal(data[0].DT1, ds.replace('T', ' ').replace(/\.\d+$/, ''));
      } else {
        assert.equal(data[0].DT1.constructor.name, "Date", "DT1 is not an instance of a Date object");
        // XXX(bnoordhuis) DT1 is in local time but we inserted
        // a UTC date so we need to adjust it before comparing.
        dt = new Date(dt.getTime() + 6e4 * dt.getTimezoneOffset());
        assert.equal(data[0].DT1.toISOString(), dt.toISOString());
      }
    });
  });
});
