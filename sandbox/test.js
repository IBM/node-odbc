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
        
            connobj.query("SELECT * FROM MARK.LONG_TABLE", function (err, resultObj) {
                if (err) {
                    console.log("ERROR IS " + err);
                }

                console.log(util.inspect(resultObj.getColumnNames()));

                resultObj.fetchAll(function(error, results) {
                    console.log("\nlength is " + results.length);

                    for (var i = 0; i < results.length; i += 10000) {
                    console.log("result from JS: " + i + " " + JSON.stringify(results[i]));
                    }

                    connobj.close(function (err) {
                        console.log('done');
                    });
                });

            });
        });
    });
};

quickExec();