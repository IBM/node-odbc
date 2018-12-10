const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

odbc.connect(cn, function(error, connection) {
    if (error) { return; }

    //let parameters = [["withparam", odbc.SQL_PARAM_INPUT]];
    
    connection.query('SELECT * FROM MARKIRISH.TEST', function(error, result) {
        if (error) { return; }

        for (let i = 0; i < result.length; i++) {
            console.log(result[i]);
        }
    
        // console.log(result);
        connection.closeSync();
    });

    // console.log(connection.columnsSync(null, 'MARKIRISH', 'TEST', null));
});