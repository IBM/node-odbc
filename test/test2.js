// /* eslint-env node, mocha */

// require('dotenv').config();
// const assert = require('assert');
// const odbc = require('../build/Release/odbc.node');
// const { Pool, Connection } = require('../lib/odbc');

// const connectionString = `${process.env.CONNECTION_STRING}`;
// const badConnectionString = 'abc123#$%';

// // const poolOptions = {
// //   connectionString: `${process.env.ConnectionString}`,
// // };

// describe('odbc: odbc bindings for Node.js', () => {
//   before(() => {
//     const connection = new Connection(connectionString);
//     connection.querySync(`CREATE TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}(ID INTEGER, NAME VARCHAR(24), AGE INTEGER)`);
//     connection.closeSync();
//   });

//   after(() => {
//     const connection = new Connection(connectionString);
//     connection.querySync(`DROP TABLE ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
//     connection.closeSync();
//   });

//   describe('Connection', () => {
//     describe('constructor(connectionString)...', () => {
//       it('...should connect with a valid connection string.', () => {
//         const connection = new Connection(connectionString);
//         assert.strictEqual(connection.odbcConnection.isConnected, true);
//       });
//       it('...should not connect with an invalid connection string.', () => {
//         assert.throws(() => {
//           const connection = new Connection(badConnectionString);
//         });
//       });
//     }); // constructor(connectionString)...

//     describe('.tables(catalog, schema, table, type, callback)...', () => {
//       describe('...with callbacks...', () => {
//         it('...should return information about a table', (done) => {
//           const connection = new Connection(connectionString);
//           connection.tables(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error, results) => {
//             assert.strictEqual(error, null);
//             assert.strictEqual(results.length, 1);
//             assert.strictEqual(results.count, 1);
//             assert.deepStrictEqual(results.columns,
//               [
//                 { name: 'TABLE_CAT', dataType: odbc.SQL_VARCHAR },
//                 { name: 'TABLE_SCHEM', dataType: odbc.SQL_VARCHAR },
//                 { name: 'TABLE_NAME', dataType: odbc.SQL_VARCHAR },
//                 { name: 'TABLE_TYPE', dataType: odbc.SQL_VARCHAR },
//                 { name: 'REMARKS', dataType: odbc.SQL_VARCHAR },
//               ]);
//             const result = results[0];
//             // not testing for TABLE_CAT, dependent on the system
//             assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
//             assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
//             assert.strictEqual(result.TABLE_TYPE, 'TABLE');
//             // not testing for REMARKS, dependent on the system
//             done();
//           });
//         });
//       });
//     });
//   }); // Connection

//   describe('Pool', () => {
//     describe('constructor(connectionString)...', () => {
//       it('...should not connect until init() is called.', () => {
//         const pool = new Pool(connectionString);
//         assert.strictEqual(pool.isInitialized, false);
//         assert.strictEqual(pool.freeConnections.length, 0);
//       });
//     }); // constructor(connectionString)...

//     describe('constructor(options)...', () => {
//       it('...should not connect until init() is called.', () => {
//         const pool = new Pool(connectionString);
//         assert.strictEqual(pool.isInitialized, false);
//         assert.strictEqual(pool.freeConnections.length, 0);
//       });
//     }); // constructor(connectionString)...

//     describe('init()...', () => {
//       describe('...with promises...', () => {
//         it('...should connect to the database with a valid connectionString.', async () => {
//           const pool = new Pool(connectionString);
//           await pool.init();
//           assert.strictEqual(pool.isInitialized, true);
//         });
//         it('...should throw an error with an invalid connectionString.', async () => {
//           const pool = new Pool(badConnectionString);
//           assert.rejects(async () => {
//             await pool.init();
//           });
//         });
//       });

//       describe('...with callbacks,.,', () => {
//         it('...should connect to the database with a valid connectionString.', (done) => {
//           const pool = new Pool(connectionString);
//           pool.init((error) => {
//             assert.strictEqual(error, null);
//             assert.strictEqual(pool.isInitialized, true);
//             done();
//           });
//         });
//         it('...should have an error with an invalid connectionString', (done) => {
//           const pool = new Pool(badConnectionString);
//           pool.init((error) => {
//             assert.notEqual(error, null);
//             assert.strictEqual(pool.isInitialized, false);
//             done();
//           });
//         });
//       });
//     });
//   }); // Pool
// });
