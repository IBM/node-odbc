# odbc


An asynchronous interface for Node.js to unixODBC and its supported drivers.

---

## Requirements


* unixODBC binaries and development libraries for module compilation
  * on Ubuntu/Debian `sudo apt-get install unixodbc unixodbc-dev`
  * on RedHat/CentOS `sudo yum install unixODBC unixODBC-devel`
  * on OSX
    * using macports.org `sudo port unixODBC`
    * using brew `brew install unixODBC`
  * on FreeBSD from ports `cd /usr/ports/databases/unixODBC; make install`
  * on IBM i `yum install unixODBC unixODBC-devel` (requires [yum](http://ibm.biz/ibmi-rpms))
* ODBC drivers for target database
* properly configured odbc.ini and odbcinst.ini.

---

## Node.js Version Support

This package is a native addon, built with C++ code using `node-addon-api`, a C++ wrapper for [N-API](https://nodejs.org/api/n-api.html). `node-addon-api` only supports versions of Node.js that are in LTS or newer. [A list of supported versions can be found on the Node.js website](https://nodejs.org/en/about/releases/). Current versions supported include:

* Node.js 12
* Node.js 14
* Node.js 15
* Node.js 16

---

## Installation

Three main steps must be done before `node-odbc` can interact with your database:

* **Install unixODBC and unixODBC-devel:** Compilation of `node-odbc` on your system requires these packages to provide the correct headers.
  * **Ubuntu/Debian**: `sudo apt-get install unixodbc unixodbc-dev`
  * **RedHat/CentOS**: `sudo yum install unixODBC unixODBC-devel`
  * **OSX**:
    * **macports.<span></span>org:** `sudo port unixODBC`
    * **using brew:** `brew install unixODBC`
  * **FreeBSD** from ports: `cd /usr/ports/databases/unixODBC; make install`
  * **IBM i:** `yum install unixODBC unixODBC-devel` (requires [yum](http://ibm.biz/ibmi-rpms))

* **Install ODBC drivers for target database:** Most database management system providers offer ODBC drivers for their product. See the website of your DBMS for more information.

* **odbc.ini and odbcinst.ini**: These files define your DSNs (data source names) and ODBC drivers, respectively. They must be set up for ODBC functions to correctly interact with your database. 

When all these steps have been completed, install `node-odbc` into your Node.js project by using:

```bash
npm install odbc
```
---

## Debugging

This package used to contain its own method of tracing ODBC calls, which was enabled by recompiling the package with `DEBUG` defined. Because this information was almost wholly redundant with existing methods of tracing available through ODBC driver managers, it was removed in v2.4.0.

Instead, tracing should be enabled through your driver manager, and that information can be analyzed and included with the description of issues encountered.

* **unixODBC (Linux, MacOS, IBM i):**

    In your `odbcinst.ini` file, add the following entry:
    ```
    [ODBC]
    Trace=yes
    TraceFile=/tmp/odbc.log
    ```
    Debug information will be appended to the trace file.

* **ODBC Data Source Administrator (Windows):**

    Open up ODBC Data Source Administrator and select the "Tracing" tab. Enter the location where you want the log file to go in **"Log File Path"**, then click **"Start Tracing Now"**.
---

## Drivers

---

## Important Changes in 2.0

`node-odbc` has recently been upgraded from its initial release. The following list highlights the major improvements and potential code-breaking changes.

* **Promise support:** All asynchronous functions can now be used with native JavaScript Promises. If a callback function is not passed, the ODBC functions will return a native Promise. If a callback _is_ passed to the ODBC functions, then the old callback behavior will be used.

* **Performance improvements:** The underlying ODBC function calls have been reworked to greatly improve performance. For ODBC afficianados, `node-odbc` used to retrieved results using SQLGetData, which works for small amounts of data but is slow for large datasets. `node-odbc` now uses SQLBindCol for binding result sets, which for large queries is orders of magnitude faster.

* **Rewritten with N-API:** `node-odbc` was completely rewritten using node-addon-api, a C++ wrapper for N-API, which created an engine-agnostic and ABI-stable package. This means that if you upgrade your Node.js version, there is no need to recompile the package, it just works!

* **API Changes:** The API has been changed and simplified. See the documentation below for a list of all the changes.

* **Timestamp and Datetime Changes:** SQL_DATETIME and SQL_TIMESTAMP no longer are automatically converted to UTC from how they were stored in the table. Previously, the assumption was that whatever was stored in the table was in "local time", and then converted to UTC. There is no guarantee that the time stored is in "local time", and many DBMSs store times without timezone data. Now, the driver will determine how to format the timestamps and datetimes that are returned, as it is retrieved simply as a String with no additional manipulation by this package.

---

## API

* [Connection](#Connection)
    * [constructor: odbc.connect()](#constructor-odbcconnectconnectionstring)
    * [.query()](#querysql-parameters-callback)
    * [.callProcedure()](#callprocedurecatalog-schema-name-parameters-callback)
    * [.createStatement()](#createstatementcallback)
    * [.tables()](#tablescatalog-schema-table-type-callback)
    * [.columns()](#columnscatalog-schema-table-column-callback)
    * [.setIsolationLevel()](#setIsolationLevellevel-callback)
    * [.beginTransaction()](#begintransactioncallback)
    * [.commit()](#commitcallback)
    * [.rollback()](#rollbackcallback)
    * [.close()](#closecallback)
* [Pool](#Pool)
    * [constructor: odbc.pool()](#constructor-odbcpoolconnectionstring)
    * [.connect()](#connectcallback)
    * [.query()](#querysql-parameters-callback-1)
    * [.close()](#closecallback-1)
* [Statement](#Statement)
    * [.prepare()](#preparesql-callback)
    * [.bind()](#bindparameters-callback)
    * [.execute()](#executecallback)
    * [.close()](#closecallback-2)
* [Cursor](#Cursor)
    * [.fetch()](#fetchcallback)
    * [.noData()](#nodata)
    * [.close()](#closecallback-3)

### **Callbacks _or_ Promises**

Every asynchronous function in the Node.js `node-odbc` package can be called with either a callback Function or a Promise. To use Promises, simply do not pass a callback function (in the API docs below, specified with a `callback?`). This will return a Promise object than can then be used with `.then` or the more modern `async/await` workflow. To use callbacks, simply pass a callback function. For each function explained in the documents below, both Callback and Promise examples are given.

_All examples are shown using IBM i Db2 DSNs and queries. Because ODBC is DBMS-agnostic, examples will work as long as the query strings are modified for your particular DBMS._

### **Result Array**

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

---
---

## **Connection**

A Connection is your means of connecting to the database through ODBC.

### `constructor: odbc.connect(connectionString)`

In order to get a connection, you must use the `.connect` function exported from the module. This asynchronously creates a Connection and gives it back to you. Like all asynchronous functions, this can be done either with callback functions or Promises.

#### Parameters:
* **connectionString**: The connection string to connect to the database, usually by naming a DSN. Can also be a configuration object with the following properties:
    * `connectionString` **REQUIRED**: The connection string to connect to the database
    * `connectionTimeout`: The number of seconds to wait for a request on the connection to complete before returning to the application
    * `loginTimeout`: The number of seconds to wait for a login request to complete before returning to the application
* **callback?**: The function called when `.connect` has finished connecting. If no callback function is given, `.connect` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * connection: The Connection object if a successful connection was made

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

async function connectToDatabase() {
    const connection1 = await odbc.connect('DSN=MYDSN');
    // connection1 is now an open Connection

    // or using a configuration object
    const connectionConfig = {
        connectionString: 'DSN=MYDSN',
        connectionTimeout: 10,
        loginTimeout: 10,
    }
    const connection2 = await odbc.connect(connectionConfig);
    // connection2 is now an open Connection
}

connectToDatabase();
```

**Callbacks**

```javascript
const odbc = require('odbc');
odbc.connect(connectionString, (error, connection) => {
    // connection is now an open Connection
});
```

Once a Connection has been created with `odbc.connect`, you can use the following functions on the connection:

---

### `.query(sql, parameters?, options?, callback?)`

Run a query on the database. Can be passed an SQL string with parameter markers `?` and an array of parameters to bind to those markers. Returns a [result array](#result-array).

#### Parameters:
* **sql**: The SQL string to execute
* **parameters?**: An array of parameters to be bound the parameter markers (`?`)
* **options?**: An object containing query options that affect query behavior. Valid properties include:
    * `cursor`: A boolean value indicating whether or not to return a cursor instead of results immediately. Can also be a string naming the cursor, which will assume that a cursor will be returned.
    * `fetchSize`: Used with a cursor, sets the number of rows that are returned on a call to `fetch` on the Cursor.
    * `timeout`: The amount of time (in seconds) that the query will attempt to execute before returning to the application.
    * `initialBufferSize`: Sets the initial buffer size (in bytes) for storing data from SQL_LONG* data fields. Useful for avoiding resizes if buffer size is known before the call.
* **callback?**: The function called when `.query` has finished execution. If no callback function is given, `.query` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The result object from execution

```JavaScript
const odbc = require('odbc');
const connection = odbc.connect(connectionString, (error, connection) => {
    connection.query('SELECT * FROM QIWS.QCUSTCDT', (error, result) => {
        if (error) { console.error(error) }
        console.log(result);
    });
});
```

---

### `.callProcedure(catalog, schema, name, parameters?, callback?)`

Calls a database procedure, returning the results in a [result array](#result-array).

#### Parameters:
* **catalog**: The name of the catalog where the procedure exists, or null to use the default catalog
* **schema**: The name of the schema where the procedure exists, or null to use a default schema
* **name**: The name of the procedure in the database
* **parameters?**: An array of parameters to pass to the procedure. For input and input/output parameters, the JavaScript value passed in is expected to be of a type translatable to the SQL type the procedure expects. For output parameters, any JavaScript value can be passed in, and will be overwritten by the function. The number of parameters passed in must match the number of parameters expected by the procedure.
* **callback?**: The function called when `.callProcedure` has finished execution. If no callback function is given, `.callProcedure` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The result object from execution

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function callProcedureExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const result = await connection.callProcedure(null, null, 'MY_PROC', [undefined]);
    // result contains an array of results, and has a `parameters` property to access parameters returned by the procedure.
    console.log(result);
}

callProcedureExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.callProcedure(null, null, 'MY_PROC', [undefined], (error, result) => {
        if (error) { console.error(error) } // handle
        // result contains an array of results, and has a `parameters` property to access parameters returned by the procedure.
        console.log(result);
    });
});
```

---

### `.createStatement(callback?)`

Returns a [Statement](#Statement) object from the connection.

#### Parameters:
* **callback?**: The function called when `.createStatement` has finished execution. If no callback function is given, `.createStatement` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * statement: The newly created Statement object

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function statementExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();
    // now have a statement where sql can be prepared, bound, and executed
}

statementExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

// returns information about all tables in schema MY_SCHEMA
odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.createStatement((error, statement) => {
        if (error) { return; } // handle
        // now have a statement where sql can be prepared, bound, and executed
    });
});
```

---

### `.tables(catalog, schema, table, type, callback?)`

Returns information about the table specified in the parameters by calling the ODBC function [SQLTables](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqltables-function?view=sql-server-2017). Values passed to parameters will narrow the result set, while `null` will include all results of that level.

#### Parameters:
* **catalog**: The name of the catalog, or null if not specified
* **schema**: The name of the schema, or null if not specified
* **table**: The name of the table, or null if not specified
* **type**: The type of table that you want information about, or null if not specified
* **callback?**: The function called when `.tables` has finished execution. If no callback function is given, `.tables` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The result object from execution

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function getTables() {
    // returns information about all tables in schema MY_SCHEMA
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const result = await connection.tables(null, 'MY_SCHEMA', null, null);
    console.log(result);
}

getTables();
```

**Callbacks**

```javascript
const odbc = require('odbc');

// returns information about all tables in schema MY_SCHEMA
odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.columns(null, "MY_SCHEMA", null, null, (error, result) => {
        if (error) { return; } // handle
        console.log(result);
    });
});
```

---

### `.columns(catalog, schema, table, column, callback?)`

Returns information about the columns specified in the parameters by calling the ODBC function [SQLColumns](https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-2017). Values passed to parameters will narrow the result set, while `null` will include all results of that level.

#### Parameters:
* **catalog**: The name of the catalog, or null if not specified
* **schema**: The name of the schema, or null if not specified
* **table**: The name of the table, or null if not specified
* **column**: The name of the column that you want information about, or null if not specified
* **callback?**: The function called when `.columns` has finished execution. If no callback function is given, `.columns` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The result object from execution

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function getColumns() {
    // returns information about all columns in table MY_SCEHMA.MY_TABLE
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const result = await connection.columns(null, 'MY_SCHEMA', 'MY_TABLE', null);
    console.log(result);
}

getColumns();
```

**Callbacks**

```javascript
const odbc = require('odbc');

// returns information about all columns in table MY_SCEHMA.MY_TABLE
odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.columns(null, "MY_SCHEMA", "MY_TABLE", null, (error, result) => {
        if (error) { return; } // handle
        console.log(result);
    });
});
```

---

### `.setIsolationLevel(level, callback?)`

Sets the transaction isolation level for the connection, which determines what degree of uncommitted changes can be seen. More information about ODBC isolation levels can be found on [the official ODBC documentation](https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/transaction-isolation?view=sql-server-2017).

#### Parameters:
* **level**: The isolation level to set on the connection. [There are four isolation levels specified by ODBC](https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/transaction-isolation-levels?view=sql-server-2017), which can be accessed through the base exported package:
    * `odbc.SQL_TXN_READ_UNCOMMITTED`
    * `odbc.SQL_TXN_READ_COMMITTED`
    * `odbc.SQL_TXN_REPEATABLE_READ`
    * `odbc.SQL_TXN_SERIALIZABLE`
* **callback?**: The function called when `.setIsolationLevel` has finished execution. If no callback function is given, `.setIsolationLevel` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function isolationLevel() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.setIsolationLevel(odbc.SQL_TXN_READ_COMMITTED);
    // isolation level is now set
}

isolationLevel();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.setIsolationLevel(odbc.SQL_TXN_READ_COMMITTED, (error) => {
        if (error) { return; } // handle
        // isolation level is now set
    });
});
```

---

### `.beginTransaction(callback?)`

Begins a transaction on the connection. The transaction can be committed by calling `.commit` or rolled back by calling `.rollback`. **If a connection is closed with an open transaction, it will be rolled back.** Connection isolation level will affect the data that other transactions can view mid transaction.

#### Parameters:
* **callback?**: The function called when `.beginTransaction` has finished execution. If no callback function is given, `.beginTransaction` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function transaction() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.beginTransaction();
    // transaction is now open
}

transaction();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.beginTransaction((error) => {
        if (error) { return; } // handle
        // transaction is now open
    });
});
```

---

### `.commit(callback?)`

Commits an open transaction. If called on a connection that doesn't have an open transaction, will no-op.

#### Parameters:
* **callback?**: The function called when `.commit` has finished execution. If no callback function is given, `.commit` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function commitTransaction() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.beginTransaction();
    const insertResult = await connection.query('INSERT INTO MY_TABLE VALUES(1, \'Name\')');
    await connection.commit();
    // INSERT query has now been committed
}

commitTransaction();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.beginTransaction((error1) => {
        if (error1) { return; } // handle
        connection.query('INSERT INTO MY_TABLE VALUES(1, \'Name\')', (error2, result) => {
            if (error2) { return; } // handle
            connection.commit((error3) => {
                // INSERT query has now been committed
            })
        })
    });
});
```

---


### `.rollback(callback?)`

Rolls back an open transaction. If called on a connection that doesn't have an open transaction, will no-op.

#### Parameters:
* **callback?**: The function called when `.rollback` has finished execution. If no callback function is given, `.rollback` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function rollbackTransaction() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    await connection.beginTransaction();
    const insertResult = await connection.query('INSERT INTO MY_TABLE VALUES(1, \'Name\')');
    await connection.rollback();
    // INSERT query has now been rolled back
}

rollbackTransaction();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.beginTransaction((error1) => {
        if (error1) { return; } // handle
        connection.query('INSERT INTO MY_TABLE VALUES(1, \'Name\')', (error2, result) => {
            if (error2) { return; } // handle
            connection.rollback((error3) => {
                // INSERT query has now been rolled back
            })
        })
    });
});
```

---

### `.close(callback?)`

Closes and open connection. Any transactions on the connection that have not been ended will be rolledback.

#### Parameters:
* **callback?**: The function called when `.close` has finished clsoing the connection. If no callback function is given, `.close` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function closeConnection() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    // do something with your connection here
    await connection.close();
}

rollbackTransaction();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
   // do something with your connection here
   connection.close((error) => {
       if (error) { return; } // handle
       // connection is now closed
   })
});
```

---
---


### **Pool**

### `constructor: odbc.pool(connectionString)`

In order to get a Pool, you must use the `.pool` function exported from the module. This asynchronously creates a Pool of a number of Connections and returns it to you. Like all asynchronous functions, this can be done either with callback functions or Promises.

Note that `odbc.pool` will return from callback or Promise as soon as it has created 1 connection. It will continue to spin up Connections and add them to the Pool in the background, but by returning early it will allow you to use the Pool as soon as possible.

#### Parameters:
* **connectionString**: The connection string to connect to the database for all connections in the pool, usually by naming a DSN. Can also be a configuration object with the following properties:
    * `connectionString` **REQUIRED**: The connection string to connect to the database
    * `connectionTimeout`: The number of seconds to wait for a request on the connection to complete before returning to the application
    * `loginTimeout`:The number of seconds to wait for a login request to complete before returning to the application
    * `initialSize`: The initial number of Connections created in the Pool
    * `incrementSize`: How many additional Connections to create when all of the Pool's connections are taken
    * `maxSize`: The maximum number of open Connections the Pool will create
    * `shrink`: Whether or not the number of Connections should shrink to `initialSize` as they free up
* **callback?**: The function called when `.connect` has finished connecting. If no callback function is given, `.connect` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * connection: The Connection object if a successful connection was made

#### Examples:

**Promises**

```JavaScript
const odbc = require('odbc');

// can only use await keyword in an async function
async function createPool() {
    const pool = await odbc.pool(`${process.env.CONNECTION_STRING}`);
    // can now do something with the Pool
}

createPool();
```

**Callbacks**

```JavaScript
const odbc = require('odbc');
const pool = odbc.pool('DSN=MyDSN', (error, pool) => {
    // pool now has open connections
});
```

### `.connect(callback?)`

Returns a [Connection](#connection) object for you to use from the Pool. Doesn't actually open a connection, because they are already open in the pool when `.init` is called.

#### Parameters:
* **callback?**: The function called when `.connect` has finished execution. If no callback function is given, `.connect` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * connection: The [Connection](#connection) retrieved from the Pool.

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function connectExample() {
    const pool = await odbc.pool(`${process.env.CONNECTION_STRING}`);
    const connection = await pool.connect();
    // now have a Connection to do work with
}

connectExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');
odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
    if (error1) { return; } // handle
    pool.connect((error2, connection) => {
        if (error2) { return; } // handle
        // now have a Connection to do work with
    });
});
```

---

### `.query(sql, parameters?, callback?)`

Utility function to execute a query on any open connection in the pool. Will get a connection, fire of the query, return the results, and return the connection the the pool.

#### Parameters:
* **sql**: An SQL string that will be executed. Can optionally be given parameter markers (`?`) and also given an array of values to bind to the parameters.
* **parameters?**: An array of values to bind to the parameter markers, if there are any. The number of values in this array must match the number of parameter markers in the sql statement.
* **options?**: An object containing query options that affect query behavior. Valid properties include:
    * `cursor`: A boolean value indicating whether or not to return a cursor instead of results immediately. Can also be a string naming the cursor, which will assume that a cursor will be returned.
    * `fetchSize`: Used with a cursor, sets the number of rows that are returned on a call to `fetch` on the Cursor.
    * `timeout`: The amount of time (in seconds) that the query will attempt to execute before returning to the application.
    * `initialBufferSize`: Sets the initial buffer size (in bytes) for storing data from SQL_LONG* data fields. Useful for avoiding resizes if buffer size is known before the call.
* **callback?**: The function called when `.query` has finished execution. If no callback function is given, `.query` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The [result array](#result-array) returned from the executed statement

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function queryExample() {
    const pool = await odbc.pool(`${process.env.CONNECTION_STRING}`);
    const result = await pool.query('SELECT * FROM MY_TABLE');
    console.log(result);
}

queryExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');
odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
    if (error1) { return; } // handle
    pool.query('SELECT * FROM MY_TABLE', (error2, result) => {
        if (error2) { return; } // handle
        console.log(result);
    });
});
```

---

### `.close(callback?)`

Closes the entire pool of currently unused connections. Will not close connections that are checked-out, but will discard the connections when they are closed with Connection's `.close` function. After calling close, must create a new Pool sprin up new Connections.

#### Parameters:
* **callback?**: The function called when `.close` has finished execution. If no callback function is given, `.close` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function closeExample() {
    const pool = await odbc.pool(`${process.env.CONNECTION_STRING}`);
    await pool.close();
    // pool is now closed
}

closeExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.pool(`${process.env.CONNECTION_STRING}`, (error1, pool) => {
    if (error1) { return; } // handle
    // do something with your pool here
    pool.close((error2) => {
        if (error2) { return; } // handle
        // pool is now closed
    });
});
```

---
---

## **Statement**

A Statement object is created from a Connection, and cannot be created _ad hoc_ with a constructor.

Statements allow you to prepare a commonly used statement, then bind parameters to it multiple times, executing in between.

---

### `.prepare(sql, callback?)`

Prepares an SQL statement, with or without parameters (?) to bind to.

#### Parameters:
* **sql**: An SQL string that is prepared and can be executed with the .`execute` function.
* **callback?**: The function called when `.prepare` has finished execution. If no callback function is given, `.prepare` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function prepareExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();
    await statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)');
    // statement has been prepared, can bind and execute
}

prepareExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.createStatement((error1, statement) => {
        if (error1) { return; } // handle
        statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)' (error2) => {
            if (error2) { return; } // handle
            // statement has been prepared, can bind and execute
        });
    });
});
```

---

### `.bind(parameters, callback?)`

Binds an array of values to the parameters on the prepared SQL statement. Cannot be called before `.prepare`.

#### Parameters:
* **sql**: An array of values to bind to the sql statement previously prepared. All parameters will be input parameters. The number of values passed in the array must match the number of parameters to bind to in the prepared statement.
* **callback?**: The function called when `.bind` has finished execution. If no callback function is given, `.bind` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function bindExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();
    await statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)');
    // Assuming MY_TABLE has INTEGER and VARCHAR fields.
    await statement.bind([1, 'Name']);
    // statement has been prepared and values bound, can now execute
}

bindExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.createStatement((error1, statement) => {
        if (error1) { return; } // handle
        statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)' (error2) => {
            if (error2) { return; } // handle
            // Assuming MY_TABLE has INTEGER and VARCHAR fields.
            statement.bind([1, 'Name'], (error3) => {
                if (error3) { return; } // handle
                // statement has been prepared and values bound, can now execute
            });
        });
    });
});
```

---

### `.execute(callback?)`

Executes the prepared and optionally bound SQL statement.

#### Parameters:
* **callback?**: The function called when `.execute` has finished execution. If no callback function is given, `.execute` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * result: The [result array](#result-array) returned from the executed statement

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function executeExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();
    await statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)');
    // Assuming MY_TABLE has INTEGER and VARCHAR fields.
    await statement.bind([1, 'Name']);
    const result = await statement.execute();
    console.log(result);
    
}

executeExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.createStatement((error1, statement) => {
        if (error1) { return; } // handle
        statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)' (error2) => {
            if (error2) { return; } // handle
            // Assuming MY_TABLE has INTEGER and VARCHAR fields.
            statement.bind([1, 'Name'], (error3) => {
                if (error3) { return; } // handle
                statement.execute((error4, result) => {
                    if (error4) { return; } // handle
                    console.log(result);
                })
            });
        });
    });
});
```

---

### `.close(callback?)`

Closes the Statement, freeing the statement handle. Running functions on the statement after closing will result in an error.

#### Parameters:
* **callback?**: The function called when `.close` has finished execution. If no callback function is given, `.close` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured in execution, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function executeExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();
    await statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)');
    // Assuming MY_TABLE has INTEGER and VARCHAR fields.
    await statement.bind([1, 'Name']);
    const result = await statement.execute();
    console.log(result);
    await statement.close();
}

executeExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.createStatement((error1, statement) => {
        if (error1) { return; } // handle
        statement.prepare('INSERT INTO MY_TABLE VALUES(?, ?)' (error2) => {
            if (error2) { return; } // handle
            // Assuming MY_TABLE has INTEGER and VARCHAR fields.
            statement.bind([1, 'Name'], (error3) => {
                if (error3) { return; } // handle
                statement.execute((error4, result) => {
                    if (error4) { return; } // handle
                    console.log(result);
                    statement.close((error5) => {
                        if (error5) { return; } // handle
                        // statement closed successfully
                    })
                })
            });
        });
    });
});
```

---
---

## **Cursor**

A Cursor object is created from a Connection when running a query, and cannot be created _ad hoc_ with a constructor.

Cursors allow you to fetch piecemeal instead of retrieving all rows at once. The fetch size is set on the query options, and then a Cursor is returned from the query instead of a result set. `.fetch` is then called to retrieve the result set by the fetch size.

---

### `.fetch(callback?)`

Asynchronously returns the next chunk of rows from the result set and returns them as a Result object.

#### Parameters:
* **callback?**: The function called when `.fetch` has finished retrieving the result rows. If no callback function is given, `.fetch` will return a native JavaScript `Promise` that resolve the result rows. Callback signature is:
    * error: The error that occured in execution, or `null` if no error
    * results: The [result array](#result-array) returned from the executed statement with at most `fetchSize`-number of rows.

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function cursorExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const cursor = await connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 });
    const result = await cursor.fetch();
    // Now have a results array of size 3 (or less) that we can use
    await cursor.close();
}

cursorExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 }, (error1, cursor) => {
        if (error1) { return; } // handle
        cursor.fetch((error2, results) => {
            if (error2) { return; } // handle
            // Now have a results array of size 3 (or less) that we can use
            cursor.close((error3) => {
                if (error3) { return; } // handle
                // cursor now closed, now do more work
            })
        });
    });
});
```

---

### `.noData()`

Returns whether the cursor is has reached the end of the result set. Fetch must be called at least once before noData can return `true`. Used for determining if there are no more results to retrieve from the cursor.

#### Parameters:
None

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function cursorExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const cursor = await connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 });
    // As long as noData() returns false, keep calling fetch
    while (!cursor.noData())
    {
        const result = await cursor.fetch();
        // Now have a results array of size 3 (or less) that we can use
    }
    await cursor.close();
}

cursorExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 }, (error1, cursor) => {
        if (error1) { return; } // handle
        cursor.fetch((error2, results) => {
            if (error2) { return; } // handle
            // Now have a results array of size 3 (or less) that we can use
            if (!cursor.noData()) {
                // Still more data to retrieve!
            } else {
                cursor.close((error3) => {
                    if (error3) { return; } // handle
                    // cursor now closed, now do more work
                });
            }
        });
    });
});
```

---

### `.close(callback?)`

Closes the statement that the cursor was generated from, and by extension the cursor itself. Needs to be called when the cursor is no longer needed.

#### Parameters:
* **callback?**: The function called when `.close` has finished execution. If no callback function is given, `.close` will return a native JavaScript `Promise`. Callback signature is:
    * error: The error that occured while closing the statement, or `null` if no error

#### Examples:

**Promises**

```javascript
const odbc = require('odbc');

// can only use await keyword in an async function
async function cursorExample() {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const cursor = await connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 });
    const result = await cursor.fetch();
    // Now have a results array of size 3 (or less) that we can use
    await cursor.close();
}

cursorExample();
```

**Callbacks**

```javascript
const odbc = require('odbc');

odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
    connection.query('SELECT * FROM MY_TABLE', { cursor: true, fetchSize: 3 }, (error1, cursor) => {
        if (error1) { return; } // handle
        cursor.fetch((error2, results) => {
            if (error2) { return; } // handle
            // Now have a results array of size 3 (or less) that we can use
            cursor.close((error3) => {
                if (error3) { return; } // handle
                // cursor now closed, now do more work
            })
        });
    });
});
```

---
---


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

* Copyright (c) 2019, 2021 IBM
* Copyright (c) 2013 Dan VerWeire <dverweire@gmail.com>
* Copyright (c) 2010 Lee Smith <notwink@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies ofthe Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
