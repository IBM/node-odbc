const util = require('util');
const odbc = require('bindings')('odbc');
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

console.log(util.inspect(odbc));
console.log(util.inspect(odbc.ODBCStatement));

var myodbc = new odbc.ODBC();

function statementExecuteDirect(sql = 'SELECT * FROM QIWS.QCUSTCDT') {

    myodbc.createConnection((err, connection) => {
        console.log("Connection is Created");
        console.log(`Am I connected? ${connection.connected}`);
        connection.open(cn, function (err) {
            if (err) {
                return console.log(`${err} : occured @ connection.open()`);
            }
            connection.createStatement((err, statement) => {
                if (err) {
                    return console.log(`${err} : occured @ connection.createStatement()`);
                }
                statement.executeDirect(sql, (err, result) =>{
                    console.log('executeDirect Callback()');
                    if (err) {
                        return console.log(`${err} : occured @ statement.execute()`);
                    }
                    console.log(util.inspect(result));
                    var results = result.fetchAllSync();
                    console.log("Finished FetchAll");
                    console.log("\nlength is " + results.length);
                    console.log(JSON.stringify(results));

                    connection.close((err, result) => {
                        if (err) {
                            return console.log(`${err} : occured @ connection.close()`);
                        }
                        console.log('done');
                    });
                });
            });
        });
    });
};
    
    var params =
    [87956, //CUSNUM
    'Slack', //LASTNAME
    'S S', //INITIAL
    '212 Park', //ADDRESS
    'Bill', //CITY
    'CO', //STATE
     54872, //ZIP
     4000, //CREDIT LIMIT
     1, // change
     250, //BAL DUE
     2.00]; //CREDIT DUE

function statementPrepareBindExecuteAsync(sql = 'INSERT INTO QIWS.QCUSTCDT(CUSNUM,LSTNAM,INIT,STREET,CITY,STATE,ZIPCOD,CDTLMT,CHGCOD,BALDUE,CDTDUE) VALUES (?,?,?,?,?,?,?,?,?,?,?) with NONE'){
    myodbc.createConnection((err, connection) => {
        console.log("Connection is Created");
        console.log(`Am I connected? ${connection.connected}`);
        connection.open(cn, function(err) {
            if (err) {
                return console.log(`${err} : occured @ connection.open()`);
            }
            connection.createStatement((err, statement) => {
                if (err) {
                    return console.log(`${err} : occured @ connection.createStatement()`);
                }
                statement.prepare(sql, (err, result) =>{
                    console.log('prepareAsync Callback()');
                    if (err) {
                        return console.log(`${err} : occured @ statement.prepare()`);
                    }
                    console.log(`Passed Prepare: ${result}`);
                    statement.bind(params, (err, result) =>{
                        console.log('bindAsync Callback()');
                        if (err) {
                            return console.log(`${err} : occured @ statement.bind()`);
                        }
                        console.log(`Passed Bind: ${result}`);
                        statement.execute((err, result) =>{
                            if (err) {
                                return console.log(`${err} : occured @ statement.bind()`);
                            }
                            console.log(`Passed execute:`);

                            connection.close((err, result) => {
                                if (err) {
                                    return console.log(`${err} : occured @ connection.close()`);
                                }
                                console.log('done');
                            });
                        });
                    });
                });
            });
        });
    });
}

statementPrepareBindExecuteAsync();

function statementPrepareBindExecuteSync(sql = 'INSERT INTO QIWS.QCUSTCDT(CUSNUM,LSTNAM,INIT,STREET,CITY,STATE,ZIPCOD,CDTLMT,CHGCOD,BALDUE,CDTDUE) VALUES (?,?,?,?,?,?,?,?,?,?,?) with NONE'){
    myodbc.createConnection((err, connection) => {
        console.log("Connection is Created");
        console.log(`Am I connected? ${connection.connected}`);
        connection.open(cn, function(err) {
            if (err) {
                return console.log(`${err} : occured @ connection.open()`);
            }
            connection.createStatement((err, statement) => {
                if (err) {
                    return console.log(`${err} : occured @ connection.createStatement()`);
                }
                statement.prepareSync(sql);
                statement.bindSync(params);
                statement.executeSync();
                connection.close((err, result) => {
                    if (err) {
                        return console.log(`${err} : occured @ connection.close()`);
                    }
                    console.log('done');
                });
            });
        });
    });
}

// statementPrepareBindExecuteSync();