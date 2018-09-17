const util = require('util');
const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

var myodbc = new odbc.ODBC();

myodbc.createConnection((err, connection) => {

    console.log("Connection is Created");

    connection.openSync(cn);
    console.log(connection.connected);

    var rs = connection.querySync("select * from MARK.BOOKS");

    console.log(JSON.stringify(rs.getColumnNamesSync()));

    connection.closeSync();
});