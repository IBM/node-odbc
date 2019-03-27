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

`node-odbc` has recently been upgraded from its initial release. The following list highlights the major improvements and potential code-breaking 

* **Performance improvements:** The underlying ODBC function calls have been reworked to greatly improve performance. For ODBC afficianados, `node-odbc` used to retrieved results using SQLGetData, which works for small amounts of data but is slow for large datasets. `node-odbc` now uses SQLBindCol for binding result sets, which for large queries is orders of magnitude faster.

* **Rewritten with N-API:** `node-odbc` was completely rewritten using node-addon-api, a C++ wrapper for N-API, which created an engine-agnostic and ABI-stable package. This means that if you upgrade your Node.js version, there is no need to recompile the package, it just works!

* **Database Class Removed:** The exported `odbc` object now contains the functions for connecting to the database and returning an open connection. There is no longer a `Database` class, and instead calling `odbc.open(...)` or `odbc.openSync(...)` will return an open database connection to the requested source with any passed configuration options.

* **No JavaScript Layer:** All of the logic running the package is now done in the C++ code for improved readability, maintainability, and performance. Previously, the JavaScript did not match the API exported by the package as it was wrapped in a JavaScript implementation of the Database class. With the removal of the JavaScript layer, 

* **Minor API Changes:** In addition to removing the Database class, minor API improvements have been made. This includes the abiility to pass configuration information through a `config` object to the connection, specify the input/output type of bound parameters, and whether results should be returned as objects or arrays. The complete API documentation can be found in this file. RESULT//TODO

---

## API

#### **Asynchronous and Synchronous Functions**


Every function in `node-odbc` can be called both asynchronously (recommended) and synchronously (not recommended). The synchronous version can be called by adding 'Sync' to the end of the function name. This version will not take a callback function, and will return the result directly. **WARNING: The synchronous version will block the Node.js event loop, and will greatly degrade performance. Use at your own risk!**

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

```javascript
const { Connection } = require('odbc');
const connection = new Connection(connectionString);
```

#### `.query(sql, parameters?, callback?)`

```javascript
const { Connection } = require('odbc');
const connection = new Connection(connectionString);
connection.query('SELECT * FROM QIWS.QCUSTCDT', (error, result) => {
    if (error) { console.error(error) }
    console.log(result);
})
```

#### `.callProcedure(catalog, schema, name, parameters?, callback?)`

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

#### `.columns(catalog, schema, table, type, callback?)`

#### `.tables(catalog, schema, table, type, callback?)`

#### `.beginTransaction(callback?)`

#### `.commit(callback?)`

#### `.rollback(callback?)`


### **Pool**

#### `constructor (new Pool(connectionString))`

```javascript
const { Pool } = require('odbc');
const pool = new Pool(connectionString);
```

**PLEASE NOTE:** The pool will not have any open connections until you call pool.init();

#### `.init(callback?)`

#### `.connect(callback?)`

Returns a Connection object for you to use (already open, doesn't actually connect).

#### `.close(callback?)`

Closes the entire pool.

#### `.query(sql, parameters?, callback?)`

Utility function to just fire off query from the pool. Will get a connection, fire of the query, return the results, and return the connection the the pool.


// TODO: Refactor below

### **ODBC**

The `ODBC` object is the gateway into connecting to the database. This is done through the `connect` function.

**`.connect(connectionString, callback(error, connection))`**

Connect to your database by taking a connection string a returning an open `Connection` object.

* _string_ `connectionString`: The ODBC connection string for your database,
* _function_ `callback`: callback (error, connection)
    * `error`: holds an object containing database errors, or null if no error
    * `connection`: the open `Connection` to your database.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.connect(connectionString, function(error, connection) {
    if (error) {
        // handle error
    }
    // connection now holds an open connection object
})
```
Synchronous version: `const connection = connectSync(connectionString)`

#### ODBC Constants

Several constants from the ODBC header files are exported on the ODBC object to be used with functions like statement.bind(), connection.getInfo(), or connection.endTransaction(). The available constants are:

* `SQL_COMMIT`
* `SQL ROLLBACK`
* `SQL_USER_NAME`
* `SQL_PARAM_INPUT`
* `SQL_PARAM_OUTPUT`
* `SQL_PARAM_INPUT_OUTPUT`


### **Connection**

**`.createStatement(callback(error, statement))`**

Creates a `Statement` object, with which queries can be prepare, parameters can be bound (and rebound), and the queries can be executed. See `Statement` class below.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.createStatement(function(error, statement) {
        if (error) { }

        // statement is now ready to prepare/bind/execute
    })
})
```

**`.query(sql, parameters?, callback(error, results)`**

Execute a query on the connection. The query can either be a pre-formatted SQL string, or a parameterized string with wildcard characters ('?') along with an array of parameters to insert. The results are returned in a standard `Result` array.

Without parameters;

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.query('SELECT * FROM SCHEMA.TABLE WHERE AGE > 30', function(error, result) {
        if (error) { }

        console.log(result);
    })
})
```

With parameters:

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    const parameters = [30];
    connection.query('SELECT * FROM SCHEMA.TABLE WHERE AGE > ?', parameters, function(error, result) {
        if (error) { }

        console.log(result);
    })
})
```

**`.beginTransaction(callback(error))`**

Creates a transaction by turning auto-commit off on the database connection. All queries run will be committed when **`.endTransaction()`** is called.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.beginTransaction(function(error) {
        if (error) { }

        // transaction is now open
    })
})
```

**`.endTransaction(rollback, callback(error))`**

Calls [SQLEndTran](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlendtran-function?view=sql-server-2017), with either SQL_COMMIT or SQL_ROLLBACK. Everything run on the `Connection` since calling **`.beginTransaction()`** will either be committed or rolled back.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }
    connection.beginTransaction(function(error) {
        if (error) { }
        // transaction is now open
        connection.query('INSERT INTO SCHEMA.TABLE(AGE) VALUES(1)', function(error, result) {
            if (error) { }
            connection.endTransaction(odbc.SQL_COMMIT, function(error, result) {
                if (error) { }
                // transaction is now closed, an committed
            })
        })
    })
})
```

**`.getInfo(option, callback(error, results))`**

Returns information about the driver and data source. **Currently only returns the the SQL_USER_NAME information**. Calls the [SQLGetInfo](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetinfo-function?view=sql-server-2017) ODBC function.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.getInfo(odbc.SQL_USER_NAME, function(error, name) {
        if (error) { }

        console.log(name); // The user name used in the database
    })
})
```

**`.columns(catalog, schema, table, type, callback(error, result))`**

Returns the list of column names in specified tables in a standard `Result` array. Calls the [SQLColumns](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-2017) ODBC function.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.columns(null, SCHEMA, TABLE, null, function(error, results) {
        if (error) { }

        console.log(results);
    })
})
```

**`.tables(catalog, schema, table, type, callback(error, result))`**

Returns the list of column names in specified tables in a standard `Result` array. Calls the [SQLTables](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqltables-function?view=sql-server-2017) ODBC function.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.tables(null, SCHEMA, null, null, function(error, results) {
        if (error) { }

        console.log(results);
    })
})
```

**`.close(function(error))`**

Closes the connection, freeing the database connection any statements that are still open.

```JavaScript
const odbc = require(odbc);
const connectionString = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8'

odbc.open(connectionString, function(error, connection) {
    if (error) { }

    connection.close(function(error) {
        if (error) { }
        // connection is closed, memory freed
    })
})
```

**`.connected: Boolean`**

Property for getting whether or not the `Connection` is connected to the database.

**`.connectionTimeout: Number`**

Property for getting and setting the connection timeout length, in seconds.

**`.loginTimeout: Number`**

Property for getting and setting the login timeout length, in seconds.

### **Statement**

A `Statement` can be used to explicitly bind, prepare, and execute a query. The `Statement` is useful if you want to execute the same parameterized query multiple times with different bound parameters. 

**`.prepare(sql, callback(error, success))`**

Prepare a statement to be bound (if there are parameters to bind to the sql string) or executed (if there are no parameters to be bound).

**`.bind(parameters, callback(error, success))`**

Binds an array of parameters onto a prepared statement. The array can be made up of values, arrays in the format `[value, inOutType]`, and/or objects with keys `{value: , inOutType: }`. These can all be mixed an matched on the in the same parameter array.

**`.execute(callback(error, results))`**

Executes a prepared

**`.close(callback(error))`**

Closes the `Statement`, and frees all of the data stored with it.

```JavaScript
const odbc = require("../")
const cn = 'DSN=*LOCAL;UID=USERNAME;PWD=PASSWORD;CHARSET=UTF8';

odbc.connect(cn, function(error, connection) {
    if (error) { console.error("UH OH BASGHETTIOS"); }
    
    connection.createStatement(function(error, statement) {
        if (error) { console.error("ERROR"); return; }
        statement.prepare('INSERT INTO SCHEMA.TABLE(NAME, AGE) VALUES(?, ?)', function(error) {
            if (error) { console.error("ERROR"); return; }
            statement.bind(['Mark', 34], function(error) {
                if (error) { console.error("ERROR"); return; }
                statement.execute(function(error, result) {
                    if (error) { console.error("ERROR"); return; }
                    console.log(result);
                    statement.close(function(error) {
                        if (error) { console.error("Error closing"); return; }
                    })
                    connection.closeSync();
                }) 
            })
        })
    });
});
```

---

## Future improvements

Development of `node-odbc` is an ongoing endeavor, and there are many planned improvements for the package. These include:

* **Promise and async/await Support:** `node-odbc` currently uses node-addon-api's `AsyncWorker` class for its asynchronous functions. The current implementation of this class presupposes the use of callback functions. The node-addon-api development team is currently working on making it more abstract so that it can use Promises and enable async/await support (see [node-addon-api Issue #231](https://github.com/nodejs/node-addon-api/issues/231)). Rather than write and then rewrite this functionality now, the developers of `node-odbc` are waiting for this update before adding Promise and asycn/await support.

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