require('dotenv').config();
const { Connection, Pool } = require('../');
const odbc = require('../build/Release/odbc.node');

// console.log(`ODBC VER IS ${odbc.ODBCVER.toString(16)}`);

const cn = `${process.env.CONNECTION_STRING}`;

// const connection = new Connection(cn);
// connection.getAttribute(odbc.SQL_ATTR_CONNECTION_DEAD, (err, result) => {
//   if (err) console.log('error');
//   console.log(result);
// });
async function promise() {
  const connection = new Connection(cn);
  try {
    const result = await connection.columns(null, 'MARK', 'ODBCTESTS', null);
    console.log(result);
    // await connection.query(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
    // await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'comitted', 20)`);
    // const result = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
    // console.log(result);
    await connection.close();
    // await connection.query(`DROP TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
  } catch (error) {
    console.error(error);
  }
}

// // async function pooltest() {
// //   try {
// //     const pool = new Pool(cn);
// //     await pool.init();
// //     const connection = await pool.connect();
// //     const results = await connection.query('SELECT * FROM MARK.NEW_CUST');
// //     await connection.reclaim();
// //     console.log(pool.freeConnections.length);
// //     setTimeout(() => {
// //       console.log(pool.freeConnections.length);
// //     }, 5000);
// //   } catch (error) {
// //     console.error(error);
// //   }
// // }

// async function test() {
//   promise();
//   // pooltest();
// }

// test();

promise();
