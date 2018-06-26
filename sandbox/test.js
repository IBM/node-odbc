const util = require('util');
const odbc = require('bindings')('odbc');
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

//console.log(util.inspect(odbc));
//console.log(util.inspect(odbc.ODBCConnection));

var myodbc = new odbc.ODBC();

function quickExec(sql = 'SELECT * FROM MARK.BOOKS') {
    myodbc.createConnection(function(err, connobj) {
        connobj.open(cn, function (err) {
            if (err) {
                return console.log(err + " @ open");
            }
        
            var results = odbcConn.query(sql, function (err, data) {
                if (err) {
                    console.log("ERROR IS " + err);
                }
        
                console.log("YO");
                console.log(results.length);
                console.log(data);
        
                odbcConn.close(function () {
                console.log('done');
                });
            });
        });
    });
};

quickExec();