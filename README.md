# node-odbc

`node-odbc` is an ODBC database interface for Node.js. It allows connecting to any database management system if the system has been correctly configured, including installing of unixODBC and unixODBC-devel packages, installing an ODBC driver for your desired database, and configuring your odbc.ini and odbcinst.ini files. By using an ODBC, and it makes remote development a breeze through the use of ODBC data sources, and switching between DBMS systems is as easy as modifying your queries, as all your code can stay the same.

---

## Installation

Instructions on how to set up your ODBC environment can be found in the SETUP.md. As an overview, three main steps must be done before `node-odbc` can interact with your database:

* **Install unixODBC and unixODBC-devel:** Compilation of `node-odbc` on your system requires these packages to provide the correct headers.
  * **Ubuntu/Debian**: `sudo apt-get install unixodbc unixodbc-dev`
  * **RedHat/CentOS**: `sudo yum install unixODBC unixODBC-devel`
  * **OSX**:
    * **macports.<span></span>org:** `sudo port unixODBC`
    * **using brew:** `brew install unixODBC`
  * **IBM i:** `yum install unixODBC unixODBC-devel` (requires [yum](http://ibm.biz/ibmi-rpms))
* **Install ODBC drivers for target database:** Most database management system providers offer ODBC drivers for their product. See the website of your DBMS for more information.
* **odbc.ini and odbcinst.ini**: These files define your DSNs (data source names) and ODBC drivers, respectively. They must be set up for ODBC functions to correctly interact with your database. 

When all these steps have been completed, install `node-odbc` into your Node.js project by using:

```bash
npm install odbc
```
---

## Important Changes in 2.0

`node-odbc` has recently been upgraded from its initial release. The following list highlights the major improvements and potential code-breaking changes.

* **Promise support:** All asynchronous functions can now be used with native JavaScript Promises. If a callback function is not passed, the ODBC functions will return a native Promise. If a callback _is_ passed to the ODBC functions, then the old callback behavior will be used.

* **Performance improvements:** The underlying ODBC function calls have been reworked to greatly improve performance. For ODBC afficianados, `node-odbc` used to retrieved results using SQLGetData, which works for small amounts of data but is slow for large datasets. `node-odbc` now uses SQLBindCol for binding result sets, which for large queries is orders of magnitude faster.

* **Rewritten with N-API:** `node-odbc` was completely rewritten using node-addon-api, a C++ wrapper for N-API, which created an engine-agnostic and ABI-stable package. This means that if you upgrade your Node.js version, there is no need to recompile the package, it just works!

* **API Changes:** The API has been changed and simplified. See the documentation below for a list of all the changes.

---

## API

#### **Callbacks _or_ Promises**

Every asynchronous function in the Node.js `node-odbc` package can be called with either a callback Function or a Promise. To use Promises, simply do not pass a callback function (in the API docs below, specified with a `callback?`). This will return a Promise object than can then be used with `.then` or the more modern `async/await` workflow. To use callbacks, simply pass a callback function. For each function explained in the documents below, both Callback and Promise examples are given.

_All examples are shown using IBM i Db2 DSNs and queries. Because ODBC is DBMS-agnostic, examples will work as long as the query strings are modified for your particular DBMS._

#### **Result Object**

All functions that return a result set do so in an array, where each row in the result set is an entry in the array. The format of data within the row can either be an array or an object, depending on the configuration option passed to the connection.

The result array also contains several properties:
* `count`: the number of rows affected by the statement or procedure. Returns the result from ODBC function SQLRowCount.
* `columns`: a list of columns in the result set. This is returned in an array. Each column in the array has the following properties:
  * `name`: The name of the column
  * `dataType`: The data type of the column properties
* `statement`: The statement used to return the result set
* `parameters`: The parameters passed to the statement or procedure. For input/output and output parameters, this value will reflect the value updated from a procedure. 
* `return`: The return value from some procedures. For many DBMS, this will always be undefined.

```
[ { CUSNUM: 938472,
    LSTNAM: 'Henning ',
    INIT: 'G K',
    STREET: '4859 Elm Ave ',
    CITY: 'Dallas',
    STATE: 'TX',
    ZIPCOD: 75217,
    CDTLMT: 5000,
    CHGCOD: 3,
    BALDUE: 37,
    CDTDUE: 0 },
  { CUSNUM: 839283,
    LSTNAM: 'Jones   ',
    INIT: 'B D',
    STREET: '21B NW 135 St',
    CITY: 'Clay  ',
    STATE: 'NY',
    ZIPCOD: 13041,
    CDTLMT: 400,
    CHGCOD: 1,
    BALDUE: 100,
    CDTDUE: 0 },
  statement: 'SELECT * FROM QIWS.QCUSTCDT',
  parameters: [],
  return: undefined,
  count: -1,
  columns: [ { name: 'CUSNUM', dataType: 2 },
    { name: 'LSTNAM', dataType: 1 },
    { name: 'INIT', dataType: 1 },
    { name: 'STREET', dataType: 1 },
    { name: 'CITY', dataType: 1 },
    { name: 'STATE', dataType: 1 },
    { name: 'ZIPCOD', dataType: 2 },
    { name: 'CDTLMT', dataType: 2 },
    { name: 'CHGCOD', dataType: 2 },
    { name: 'BALDUE', dataType: 2 },
    { name: 'CDTDUE', dataType: 2 } ] ]

```

In this example, two rows are returned, with eleven columns each. The format of these columns is found on the `columns` property, with their names and dataType (which are integers mapped to SQL data types).

With this result structure, users can iterate over the result set like any old array (in this case, `results.length` would return 2) while also accessing important information from the SQL call and result set.

### **Connection**

connection has the following functions:

#### `constructor (new Connection(connectionString))`

Create a Connection object, which is opened (synchronously!)

```javascript
const { Connection } = require('odbc');
const connection = new Connection(connectionString);
```

#### `.query(sql, parameters?, callback?)`

Run a query on the database. Can be passed an SQL string with parameter markers `?` and an array of parameters to bind to those markers. 

```JavaScript
const { Connection } = require('odbc');
const connection = new Connection(connectionString);
connection.query('SELECT * FROM QIWS.QCUSTCDT', (error, result) => {
    if (error) { console.error(error) }
    console.log(result);
})
```

#### `.callProcedure(catalog, schema, name, parameters?, callback?)`

Call a procedure on the database, getting the results returned in a result object.

```javascript
const { Connection } = require('odbc');
const connection = new Connection(connectionString);
connection.callProcedure(null, null, 'myproc', [null], (error, result) => {
    if (error) { console.error(error) }
    console.log(result);
})
```

#### `.createStatement(callback?)`

Returns a Statement object on from the connection (see below).

#### `.close(callback?)`

Closes an open connection. It cannot be reopened.

#### `.columns(catalog, schema, table, type, callback?)`

Returns information about the columns specified in the parameters.

#### `.tables(catalog, schema, table, type, callback?)`

Returns information about the table specified in the parameters.

#### `.beginTransaction(callback?)`

Begins a transaction on the connection. The transaction can be committed by calling `.commit` or rolled back by calling `.rollback`. **If a connection is closed with an open transaction, it will be rolled back.**

#### `.commit(callback?)`

Commits an open transaction. If called on a connection that doesn't have an open transaction, will no-op.

#### `.rollback(callback?)`

Rolls back an open transaction. If called on a connection that doesn't have an open transaction, will no-op.


### **Pool**

#### `constructor (new Pool(connectionString))`

Creates a instance of the Pool class, storing information but not opening any connections.

```JavaScript
const { Pool } = require('odbc');
const pool = new Pool(connectionString);
```

**PLEASE NOTE:** The pool will not have any open connections until you call pool.init();

#### `.init(callback?)`

Opens all the connections in the Pool asynchronously.

**callbacks**
```JavaScript
const { Pool } = require('odbc');
const pool = new Pool(connectionString);
pool.init((error) => {
    if (error) {
        // handle error
    }
    // pool now has open connection to use
});
```
**promises**
```JavaScript
const { Pool } = require('odbc');

async function run() => {
    const pool = new Pool(connectionString);
    await pool.init();
    // pool now has open connections to use
}
run();
```

#### `.connect(callback?)`

Returns a Connection object for you to use from the Pool. Doesn't actually open a connection, because they are already open in the pool.

**callbacks**
```JavaScript
const { Pool } = require('odbc');
const pool = new Pool(connectionString);
pool.init((error1) => {
    if (error1) {
        // handle error
    }
    pool.connect((error2, connection) => {
        if (error2) {
            // handle error
        }
        // connection can now be used like any Connection object
    });
    // pool now has open connection to use
});
```
**promises**
```JavaScript
const { Pool } = require('odbc');

async function run() => {
    const pool = new Pool(connectionString);
    await pool.init();
    const connection = await pool.connect();
    // connection can now be used like any Connection object
}
run()
```

#### `.close(callback?)`

Closes the entire pool of currently unused connections. Will not close connections that are checked-out.

**promises**
```JavaScript
const { Pool } = require('odbc');

async function run() => {
    const pool = new Pool(connectionString);
    await pool.init();
    await close();
    // pool is now closed (even though we didn't do anything with it!)
}
run()
```

#### `.query(sql, parameters?, callback?)`

Utility function to just fire off query from the pool. Will get a connection, fire of the query, return the results, and return the connection the the pool.

**promises**
```JavaScript
const { Pool } = require('odbc');

async function run() => {
    const pool = new Pool(connectionString);
    await pool.init();
    const result = await pool.query('SELECT * FROM mytable');
}
run()
```

### **Statement**

A statement object is created from a Connection, and cannot be created ad hoc with a constructor.

Statements allow you to prepare a commonly used statement, then bind parameters to it multiple times, executing in between.

#### `.prepare(sql, callback?)`

Prepares an SQL statement

#### `.bind(parameters, callback?)`

Binds parameters to the SQL statement

#### `.execute(callback?)`

Executes the prepared and optionally bound sql statement.

#### `.close(callback?)`

Closes the Statement.

## Future improvements

Development of `node-odbc` is an ongoing endeavor, and there are many planned improvements for the package. If you would like to see something, simply add it to the Issues and we will respond!

## contributors

* Mark Irish (mirish@ibm.com)
* Dan VerWeire (dverweire@gmail.com)
* Lee Smith (notwink@gmail.com)
* Bruno Bigras
* Christian Ensel
* Yorick
* Joachim Kainz
* Oleg Efimov
* paulhendrix

license
-------


Copyright (c) 2013 Dan VerWeire <dverweire@gmail.com>
Copyright (c) 2010 Lee Smith <notwink@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies ofthe Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.