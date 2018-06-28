const util = require('util');
const odbc = require('bindings')('odbc');
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

//console.log(util.inspect(odbc));
//console.log(util.inspect(odbc.ODBCConnection));

var myodbc = new odbc.ODBC();

function quickExec(sql = 'SELECT * FROM MARK.BOOKS') {


    myodbc.createConnection(function(err, connobj) {

        console.log(connobj.connected);
        connobj.connected = true;
        console.log(connobj.connected);

        connobj.open(cn, function (err) {
            if (err) {
                return console.log(err + " @ open");
            }
        
            var results = connobj.query(sql, function (err, resultObj) {
                if (err) {
                    console.log("ERROR IS " + err);
                }

                var results = resultObj.fetchAll();
                console.log("\nlength is " + results.length);
        
                connobj.close(function (err) {
                    console.log('done');
                });
            });
        });
    });
};

quickExec();