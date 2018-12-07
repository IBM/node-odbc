const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

odbc.connect(cn, function(error, connection) {
    if (error) { return; }

    let parameters = [["withparam", odbc.SQL_PARAM_INPUT]];
    
    connection.query('INSERT INTO MARKIRISH.TEST(NAME) VALUES(?)', parameters, function(error, result) {
        if (error) { return; }
    
        console.log(result);
        connection.closeSync();
    });
});