const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=MARKIRISH;PWD=my1pass;CHARSET=UTF8';

odbc.connect(cn, function(error, connection) {
    if (error) { console.error("UH OH BASGHETTIOS"); }
    
    connection.createStatement(function(error, statement) {
        if (error) { console.error("ERROR1"); return; }
        statement.prepare('INSERT INTO MARKIRISH.TEST(NAME, INTEGER) VALUES(?, ?)', function(error) {
            if (error) { console.error("ERROR2"); return; }
            statement.bind(['BINDIT', 34], function(error) {
                if (error) { console.error("ERROR3"); return; }
                statement.execute(function(error, result) {
                    if (error) { console.error("ERROR4"); return; }
                    console.log("ok we executing");
                    console.log(result);
                    // console.log(result);
                    connection.closeSync();
                }) 
            })
        })
    });

    // console.log(connection.columnsSync(null, 'MARKIRISH', 'TEST', null));
});