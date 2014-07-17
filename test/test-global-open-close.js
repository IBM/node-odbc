var common = require("./common")
  , odbc = require("../")
  , assert = require("assert");

odbc.open(common.connectionString, function (err, conn) {
  if (err) {
    console.log(err);
  }
  assert.equal(err, null);
  assert.equal(conn.constructor.name, 'Database');

  conn.close();
});

