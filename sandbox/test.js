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
        
            connobj.query("SELECT * FROM MARK.BOOKS", function (err, resultObj) {
                if (err) {
                    console.log("ERROR IS " + err);
                }

                console.log(util.inspect(resultObj.getColumnNames()));

                resultObj.fetchAll(function(error, results) {
                    console.log("\nlength is " + results.length);

                    for (var i = 0; i < results.length; i++) {
                        console.log("result from JS: " + i + " " + JSON.stringify(results[i]));
                    }
                    console.log(results["rowCount"]);
                    console.log(util.inspect(results["fields"]));

                    connobj.close(function (err) {
                        console.log('done');
                    });
                });

            });
            // connobj.query("DELETE FROM MARK.BOOKS WHERE TITLE = ?", ["Holes"], function (err, resultObj) {
            //     if (err) {
            //         console.log("\nquery ERROR IS " + util.inspect(err));
            //         return;
            //     }
            // });
        });
    });
};

quickExec();