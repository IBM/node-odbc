const util = require('util');
const odbc = require('bindings')('odbc');
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass';

console.log(util.inspect(odbc));
console.log(util.inspect(odbc.ODBCConnection));

//var myodbc = new odbc();
var myodbc = new odbc.ODBC();
var odbcConn = new odbc.ODBCConnection();

function quickExec(sql = 'SELECT * FROM MARK.BOOKS') {
    myodbc.createConnection(function(err) {
        // odbcConn.open(cn, function (err) {
        //     // if (err) {
        //     //     return console.log(err + " @ open");
        //     // }
        
        //     // var results = odbc.query(sql, function (err, data) {
        //     //     if (err) {
        //     //         console.log("ERROR IS " + err);
        //     //     }
        
        //     //     console.log("YO");
        //     //     console.log(results.length);
        //     //     console.log(data);
        
        //     //     odbc.close(function () {
        //     //     console.log('done');
        //     //     });
        //     // });
        // });
    });
};

quickExec();