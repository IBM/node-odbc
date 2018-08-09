const util = require('util');
const odbc = require("../")
const cn = 'DRIVER={SQLite3};DATABASE=data/sqlite-test.db';

var myodbc = new odbc.ODBC();

myodbc.createConnection((err, connection) => {

    console.log("Connection is Created");
    console.log(`Am I connected? ${connection.connected}`);

    connection.openSync(cn);
    assert.equal(odbc.connected, true);

    var rs = myodbc.queryResultSync("select 1 as SomeIntField, 'string' as someStringField");

    assert.deepEqual(rs.getColumnNamesSync(), ['SomeIntField', 'someStringField']);

    myodbc.closeSync();
});