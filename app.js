const odbc = require('.');

async function doit() {
  const connection = await odbc.connect('DSN=OSS73DEV');
  const result = await connection.query('SELECT * FROM MIRISH.TEST');
  console.log(result);
}

doit();
