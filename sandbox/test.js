const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

let connection = odbc.createConnectionSync();

connection.openSync(cn);

let result = connection.querySync('SELECT * FROM MARK.NEW_CUST');

console.log(result);

connection.closeSync();