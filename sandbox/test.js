const util = require('util');
const odbc = require('bindings')('odbc');
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

//console.log(util.inspect(odbc));
//console.log(util.inspect(odbc.ODBCConnection));

var myodbc = new odbc.ODBC();

function quickExec() {

    myodbc.createConnection(function(err, connobj) {

        connobj.open(cn, function (err) {
            if (err) {
                return console.log(err + " @ open");
            }
        
            var results = connobj.query("SELECT * FROM MARK.BOOKS", function (err, resultObj) {
                if (err) {
                    console.log("ERROR IS " + err);
                }

                resultObj.fetchAll(function(error, results) {
                    console.log("\nlength is " + results.length);

                    console.log(results[90000]);

                    connobj.close(function (err) {
                        console.log('done');
                    });
                });

            });
        });
    });
};

quickExec();