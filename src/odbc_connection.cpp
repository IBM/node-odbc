/*
  Copyright (c) 2019, IBM
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith<notwink@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"

// object keys for the result object
const char* NAME = "name\0";
const char* DATA_TYPE = "dataType\0";
const char* STATEMENT = "statement\0";
const char* PARAMETERS = "parameters\0";
const char* RETURN = "return\0";
const char* COUNT = "count\0";
const char* COLUMNS = "columns\0";

// error strings
// const char* ODBC_ERRORS = "odbcErrors\0";
// const char* STATE = "state\0";
// const char* CODE = "code\0";
// const char* MESSAGE = "message\0";

Napi::FunctionReference ODBCConnection::constructor;

Napi::Object ODBCConnection::Init(Napi::Env env, Napi::Object exports) {

  DEBUG_PRINTF("ODBCConnection::Init\n");

  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCConnection", {

    InstanceMethod("close", &ODBCConnection::Close),
    InstanceMethod("createStatement", &ODBCConnection::CreateStatement),
    InstanceMethod("query", &ODBCConnection::Query),
    InstanceMethod("beginTransaction", &ODBCConnection::BeginTransaction),
    InstanceMethod("commit", &ODBCConnection::Commit),
    InstanceMethod("rollback", &ODBCConnection::Rollback),
    InstanceMethod("callProcedure", &ODBCConnection::CallProcedure),
    InstanceMethod("getUsername", &ODBCConnection::GetUsername),
    InstanceMethod("tables", &ODBCConnection::Tables),
    InstanceMethod("columns", &ODBCConnection::Columns),

    InstanceAccessor("connected", &ODBCConnection::ConnectedGetter, nullptr),
    InstanceAccessor("autocommit", &ODBCConnection::AutocommitGetter, nullptr),
    InstanceAccessor("connectionTimeout", &ODBCConnection::ConnectTimeoutGetter, &ODBCConnection::ConnectTimeoutSetter),
    InstanceAccessor("loginTimeout", &ODBCConnection::LoginTimeoutGetter, &ODBCConnection::LoginTimeoutSetter)

  });

  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  return exports;
}

Napi::Value ODBCConnection::AutocommitGetter(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLINTEGER autocommit;

  SQLGetConnectAttr(
    this->hDBC,          // ConnectionHandle
    SQL_ATTR_AUTOCOMMIT, // Attribute
    &autocommit,         // ValuePtr
    0,                   // BufferLength
    NULL                 // StringLengthPtr
  );

  if (autocommit == SQL_AUTOCOMMIT_OFF) {
    return Napi::Boolean::New(env, false);
  } else if (autocommit == SQL_AUTOCOMMIT_ON) {
    return Napi::Boolean::New(env, true);
  }

  return Napi::Boolean::New(env, false);
}

ODBCConnection::ODBCConnection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCConnection>(info) {

  this->hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  this->connectionOptions = *(info[2].As<Napi::External<ConnectionOptions>>().Data());
  this->maxColumnNameLength = *(info[3].As<Napi::External<SQLSMALLINT>>().Data());

  this->connectionTimeout = 0;
  this->loginTimeout = 5;
}

ODBCConnection::~ODBCConnection() {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::~ODBCConnection\n", hENV, hDBC);

  this->Free();
}

SQLRETURN ODBCConnection::Free() {
  DEBUG_PRINTF("ODBCConnection::Free\n");

  SQLRETURN returnCode = SQL_SUCCESS;

  uv_mutex_lock(&ODBC::g_odbcMutex);

    if (this->hDBC) {
      returnCode = SQLDisconnect(this->hDBC);
      if (!SQL_SUCCEEDED(returnCode)) {
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        return returnCode;
      }

      returnCode = SQLFreeHandle(SQL_HANDLE_DBC, this->hDBC);
      if (!SQL_SUCCEEDED(returnCode)) {
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        return returnCode;
      }

      hDBC = NULL;
    }

  uv_mutex_unlock(&ODBC::g_odbcMutex);
  return returnCode;
}

Napi::Value ODBCConnection::ConnectedGetter(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLINTEGER connection = SQL_CD_TRUE;

  SQLGetConnectAttr(
    this->hDBC,               // ConnectionHandle
    SQL_ATTR_CONNECTION_DEAD, // Attribute
    &connection,              // ValuePtr
    0,                        // BufferLength
    NULL                      // StringLengthPtr
  );

  if (connection == SQL_CD_FALSE) { // connection dead is false
    return Napi::Boolean::New(env, true);
  }

  return Napi::Boolean::New(env, false);
}

Napi::Value ODBCConnection::ConnectTimeoutGetter(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Number::New(env, this->connectionTimeout);
}

void ODBCConnection::ConnectTimeoutSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (value.IsNumber()) {
    this->connectionTimeout = value.As<Napi::Number>().Uint32Value();
  }
}

Napi::Value ODBCConnection::LoginTimeoutGetter(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Number::New(env, this->loginTimeout);
}

void ODBCConnection::LoginTimeoutSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (value.IsNumber()) {
    this->loginTimeout = value.As<Napi::Number>().Uint32Value();
  }
}

// All of data has been loaded into data->storedRows. Have to take the data
// stored in there and convert it it into JavaScript to be given to the
// Node.js runtime.
Napi::Array ODBCConnection::ProcessDataForNapi(Napi::Env env, QueryData *data) {

  Column **columns = data->columns;
  SQLSMALLINT columnCount = data->columnCount;

  // The rows array is the data structure that is returned from query results.
  // This array holds the records that were returned from the query as objects,
  // with the column names as the property keys on the object and the table
  // values as the property values.
  // Additionally, there are four properties that are added directly onto the
  // array:
  //   'count'   : The  returned from SQLRowCount, which returns "the
  //               number of rows affected by an UPDATE, INSERT, or DELETE
  //               statement." For SELECT statements and other statements
  //               where data is not available, returns -1.
  //   'columns' : An array containing the columns of the result set as
  //               objects, with two properties:
  //                 'name'    : The name of the column
  //                 'dataType': The integer representation of the SQL dataType
  //                             for that column.
  //   'parameters' : An array containing all the parameter values for the
  //                  query. If calling a statement, then parameter values are
  //                  unchanged from the call. If calling a procedure, in/out
  //                  or out parameters may have their values changed.
  //   'return'     : For some procedures, a return code is returned and stored
  //                  in this property.
  //   'statement'  : The SQL statement that was sent to the server. Parameter
  //                  markers are not altered, but parameters passed can be
  //                  determined from the parameters array on this object
  Napi::Array rows = Napi::Array::New(env);

  // set the 'statement' property
  if (data->sql == NULL) {
    rows.Set(Napi::String::New(env, STATEMENT), env.Null());
  } else {
    rows.Set(Napi::String::New(env, STATEMENT), Napi::String::New(env, (const char*)data->sql));
  }

  // set the 'parameters' property
  Napi::Array params = ODBC::ParametersToArray(env, data);
  rows.Set(Napi::String::New(env, PARAMETERS), params);

  // set the 'return' property
  rows.Set(Napi::String::New(env, RETURN), env.Undefined()); // TODO: This doesn't exist on my DBMS of choice, need to test on MSSQL Server or similar

  // set the 'count' property
  rows.Set(Napi::String::New(env, COUNT), Napi::Number::New(env, (double)data->rowCount));

  // construct the array for the 'columns' property and then set
  Napi::Array napiColumns = Napi::Array::New(env);

  for (SQLSMALLINT h = 0; h < columnCount; h++) {
    Napi::Object column = Napi::Object::New(env);
    column.Set(Napi::String::New(env, NAME), Napi::String::New(env, (const char*)columns[h]->ColumnName));
    column.Set(Napi::String::New(env, DATA_TYPE), Napi::Number::New(env, columns[h]->DataType));
    napiColumns.Set(h, column);
  }
  rows.Set(Napi::String::New(env, COLUMNS), napiColumns);

  // iterate over all of the stored rows,
  for (size_t i = 0; i < data->storedRows.size(); i++) {

    Napi::Object row;

    if (this->connectionOptions.fetchArray == true) {
      row = Napi::Array::New(env);
    } else {
      row = Napi::Object::New(env);
    }

    ColumnData *storedRow = data->storedRows[i];

    // Iterate over each column, putting the data in the row object
    for (SQLSMALLINT j = 0; j < columnCount; j++) {

      Napi::Value value;

      // check for null data
      if (storedRow[j].size == SQL_NULL_DATA) {

        value = env.Null();

      } else {

        switch(columns[j]->DataType) {
          case SQL_REAL:
          case SQL_DECIMAL:
          case SQL_NUMERIC:
            value = Napi::Number::New(env, atof((const char*)storedRow[j].data));
            break;
          // Napi::Number
          case SQL_FLOAT:
          case SQL_DOUBLE:
            value = Napi::Number::New(env, *(double*)storedRow[j].data);
            break;
          case SQL_TINYINT:
          case SQL_SMALLINT:
          case SQL_INTEGER:
            value = Napi::Number::New(env, *(int*)storedRow[j].data);
            break;
          // Napi::BigInt
          case SQL_BIGINT:
            value = Napi::BigInt::New(env, *(int64_t*)storedRow[j].data);
            break;
          // Napi::ArrayBuffer
          case SQL_BINARY :
          case SQL_VARBINARY :
          case SQL_LONGVARBINARY :
          {
            SQLCHAR *binaryData = new SQLCHAR[storedRow[j].size]; // have to save the data on the heap
            memcpy((SQLCHAR *) binaryData, storedRow[j].data, storedRow[j].size);
            value = Napi::ArrayBuffer::New(env, binaryData, storedRow[j].size, [](Napi::Env env, void* finalizeData) {
              delete[] (SQLCHAR*)finalizeData;
            });
            break;
          }
          // Napi::String (char16_t)
          case SQL_WCHAR :
          case SQL_WVARCHAR :
          case SQL_WLONGVARCHAR :
            value = Napi::String::New(env, (const char16_t*)storedRow[j].data, storedRow[j].size/sizeof(SQLWCHAR));
            break;
          // Napi::String (char)
          case SQL_CHAR :
          case SQL_VARCHAR :
          case SQL_LONGVARCHAR :
          default:
            value = Napi::String::New(env, (const char*)storedRow[j].data, storedRow[j].size);
            break;
        }
      }
      if (this->connectionOptions.fetchArray == true) {
        row.Set(j, value);
      } else {
        row.Set(Napi::String::New(env, (const char*)columns[j]->ColumnName), value);
      }
    }
    rows.Set(i, row);
  }

  return rows;
}

/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

// CloseAsyncWorker, used by Close function (see below)
class CloseAsyncWorker : public ODBCAsyncWorker {

  public:
    CloseAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CloseAsyncWorker() {}

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      // When closing, make sure any transactions are closed as well. Because we don't know whether
      // we should commit or rollback, so we default to rollback.
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseStatementAsyncWorker::Execute(): Calling SQLEndTran(HandleType = %d, Handle = %p, CompletionType = %d)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, SQL_ROLLBACK);
      if (odbcConnectionObject->hDBC != NULL) {
        sqlReturnCode = SQLEndTran(
          SQL_HANDLE_DBC,             // HandleType
          odbcConnectionObject->hDBC, // Handle
          SQL_ROLLBACK                // CompletionType
        );
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseStatementAsyncWorker::Execute(): SQLEndTran FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
          SetError("[odbc] Error ending any potential transactions\0");
          return;
        }
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseStatementAsyncWorker::Execute(): SQLEndTran succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);

        sqlReturnCode = odbcConnectionObject->Free();
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p] ODBCConnection::CloseStatementAsyncWorker::Execute(): SQLAllocHandle FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
          SetError("[odbc] Error disconnecting from the Connection\0");
          return;
        }
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p] ODBCConnection::CloseAsyncWorker::OnOK()\n", odbcConnectionObject->hENV);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }
};

/*
 *  ODBCConnection::Close (Async)
 *
 *    Description: Closes the connection asynchronously.
 *
 *    Parameters:
 *
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function.
 *
 *         info[0]: Function: callback function, in the following format:
 *            function(error)
 *              error: An error object if the connection was not closed, or
 *                     null if operation was successful.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined. (The return values are attached to the callback function).
 */
Napi::Value ODBCConnection::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::Close()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ***************************** CREATE STATEMENT *******************************
 *****************************************************************************/

// CreateStatementAsyncWorker, used by CreateStatement function (see below)
class CreateStatementAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
    HSTMT hSTMT;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CreateStatementAsyncWorker:Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      uv_mutex_lock(&ODBC::g_odbcMutex);
      sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,            // HandleType
        odbcConnectionObject->hDBC, // InputHandle
        &hSTMT                      // OutputHandlePtr
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CreateStatementAsyncWorker::Execute(): SQLAllocHandle returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, hSTMT);
        SetError("[odbc] Error allocating a handle to create a new Statement\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CreateStatementAsyncWorker::OnOK()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // arguments for the ODBCStatement constructor
      std::vector<napi_value> statementArguments;
      statementArguments.push_back(Napi::External<ODBCConnection>::New(env, odbcConnectionObject));
      statementArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));

      // create a new ODBCStatement object as a Napi::Value
      Napi::Value statementObject = ODBCStatement::constructor.New(statementArguments);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());      // callbackArguments[0]
      callbackArguments.push_back(statementObject); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

  public:
    CreateStatementAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CreateStatementAsyncWorker() {}
};

/*
 *  ODBCConnection::CreateStatement
 *
 *    Description: Create an ODBCStatement to manually prepare, bind, and
 *                 execute.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransactionSync'.
 *
 *        info[0]: Function: callback function:
 *            function(error, statement)
 *              error: An error object if there was an error creating the
 *                     statement, or null if operation was successful.
 *              statement: The newly created ODBCStatement object
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::CreateStatement(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CreateStatement()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CreateStatementAsyncWorker *worker = new CreateStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ********************************** QUERY *************************************
 *****************************************************************************/

// QueryAsyncWorker, used by Query function (see below)
class QueryAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData      *data;

    void Execute() {

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): Running SQL '%s'\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, (char*)data->sql);
      
      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      data->sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,
        odbcConnectionObject->hDBC,
        &(data->hSTMT)
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLAllocHandle returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error allocating a handle to run the SQL statement\0");
        return;
      }

      // querying with parameters, need to prepare, bind, execute
      if (data->bindValueCount > 0) {
        // binds all parameters to the query
        data->sqlReturnCode = SQLPrepare(
          data->hSTMT,
          data->sql,
          SQL_NTS
        );
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLPrepare returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error preparing the SQL statement\0");
          return;
        }

        data->sqlReturnCode = SQLNumParams(
          data->hSTMT,
          &data->parameterCount
        );
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLNumParams returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error getting information about the number of parameter markers in the statment\0");
          return;
        }

        if (data->parameterCount != data->bindValueCount) {
          SetError("[odbc] The number of parameter markers in the statement does not equal the number of bind values passed to the function.");
          return;
        }

        data->sqlReturnCode = ODBC::DescribeParameters(data->hSTMT, data->parameters, data->parameterCount);
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): DescribeParameters returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error getting information about parameters\0");
          return;
        }

        data->sqlReturnCode = ODBC::BindParameters(data->hSTMT, data->parameters, data->parameterCount);
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): BindParameters returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error binding parameters\0");
          return;
        }

        data->sqlReturnCode = SQLExecute(data->hSTMT);
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecute returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error executing the sql statement\0");
          return;
        }
      }
      // querying without parameters, can just use SQLExecDirect
      else {
        data->sqlReturnCode = SQLExecDirect(
          data->hSTMT,
          data->sql,
          SQL_NTS
        );
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecDirect returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error executing the sql statement\0");
          return;
        }
      }

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
          SetError("[odbc] Error retrieving the result set from the statement\0");
          return;
        }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p]ODBCConnection::QueryAsyncWorker::OnOk()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

  public:
    QueryAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~QueryAsyncWorker() {
      uv_mutex_lock(&ODBC::g_odbcMutex);
      this->data->sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, this->data->hSTMT);
      this->data->hSTMT = SQL_NULL_HANDLE;
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      delete data;
    }
};

/*
 *  ODBCConnection::Query
 *
 *    Description: Returns the info requested from the connection.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'query'.
 *
 *        info[0]: String: the SQL string to execute
 *        info[1?]: Array: optional array of parameters to bind to the query
 *        info[1/2]: Function: callback function:
 *            function(error, result)
 *              error: An error object if the connection was not opened, or
 *                     null if operation was successful.
 *              result: A string containing the info requested.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::Query(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::Query()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData();
  std::vector<Napi::Value> values;

  // check if parameters were passed or not
  if (info.Length() == 3 && info[0].IsString() && info[1].IsArray() && info[2].IsFunction()) {
    Napi::Array parameterArray = info[1].As<Napi::Array>();
    data->bindValueCount = (SQLSMALLINT)parameterArray.Length();
    data->parameters = new Parameter*[data->bindValueCount];
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&parameterArray, data->parameters);
  } else if ((info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) || (info.Length() == 3 && info[0].IsString() && info[1].IsNull() && info[2].IsFunction())) {
    data->bindValueCount = 0;
    data->parameters = NULL;
  } else {
    Napi::TypeError::New(env, "[node-odbc]: Wrong function signature in call to Connection.query({string}, {array}[optional], {function}).").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[info.Length() - 1].As<Napi::Function>();

  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

  QueryAsyncWorker *worker;

  worker = new QueryAsyncWorker(this, data, callback);
  worker->Queue();
  return env.Undefined();
}

/******************************************************************************
 ***************************** CALL PROCEDURE *********************************
 *****************************************************************************/

// CallProcedureAsyncWorker, used by CreateProcedure function (see below)
class CallProcedureAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData      *data;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CallProcedureAsyncWorker::Execute()\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC);

      char *combinedProcedureName = new char[255]();
      if (data->catalog != NULL) {
        strcat(combinedProcedureName, (const char*)data->catalog);
        strcat(combinedProcedureName, ".");
      }
      if (data->schema != NULL) {
        strcat(combinedProcedureName, (const char*)data->schema);
        strcat(combinedProcedureName, ".");
      }
      strcat(combinedProcedureName, (const char*)data->procedure);

      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      data->sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,            // HandleType
        odbcConnectionObject->hDBC, // InputHandle
        &data->hSTMT                // OutputHandlePtr
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): SQLAllocHandle returned code %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error allocating a statment handle to get procedure information\0");
        return;
      }

      data->sqlReturnCode = SQLProcedures(
        data->hSTMT,     // StatementHandle
        data->catalog,   // CatalogName
        SQL_NTS,         // NameLengh1
        data->schema,    // SchemaName
        SQL_NTS,         // NameLength2
        data->procedure, // ProcName
        SQL_NTS          // NameLength3
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): SQLProcedures returned %d (catalog: '%s', schema: '%s', procedure: '%s')\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode, data->catalog, data->schema, data->procedure);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving information about the procedures in the database\0");
        return;
      }

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): RetrieveResultSet returned code %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving information about procedures\0");
        return;
      }

      if (data->storedRows.size() == 0) {
        char errorString[255];
        sprintf(errorString, "[odbc] CallProcedureAsyncWorker::Execute: Stored procedure '%s' doesn't exist", combinedProcedureName);
        SetError(errorString);
        return;
      }

      data->deleteColumns(); // delete data in columns for next result set

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): Calling SQLProcedureColumns(StatementHandle = %p, CatalogName = %s, NameLength1 = %d, SchemaName = %s, NameLength2 = %d, ProcName = %s, NameLength3 = %d, ColumnName = %s, NameLength4 = %d)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->catalog, SQL_NTS, data->schema, SQL_NTS, data->procedure, SQL_NTS, NULL, SQL_NTS);
      data->sqlReturnCode = SQLProcedureColumns(
        data->hSTMT,     // StatementHandle
        data->catalog,   // CatalogName
        SQL_NTS,         // NameLengh1
        data->schema,    // SchemaName
        SQL_NTS,         // NameLength2
        data->procedure, // ProcName
        SQL_NTS,         // NameLength3
        NULL,            // ColumnName
        SQL_NTS          // NameLength4
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): SQLProcedureColumns returned %d (catalog: '%s', schema: '%s', procedure: '%s')\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode, data->catalog, data->schema, data->procedure);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving information about the columns in the procedure\0");
        return;
      }

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): RetrieveResultSet FAILED: SQLRETURN = %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving the result set containing information about the columns in the procedure\0");
        return;
      }

      data->parameterCount = data->storedRows.size();
      if (data->bindValueCount != (SQLSMALLINT)data->storedRows.size()) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): ERROR: Wrong number of parameters were passed to the function\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT);
        SetError("[odbc] The number of parameters the procedure expects and and the number of passed parameters is not equal\0");
        return;
      }

      // get stored column parameter data from the result set
      for (int i = 0; i < data->parameterCount; i++) {
        data->parameters[i]->InputOutputType = *(SQLSMALLINT*)data->storedRows[i][4].data;
        data->parameters[i]->ParameterType = *(SQLSMALLINT*)data->storedRows[i][5].data; // DataType -> ParameterType
        data->parameters[i]->ColumnSize = *(SQLSMALLINT*)data->storedRows[i][7].data; // ParameterSize -> ColumnSize
        data->parameters[i]->Nullable = *(SQLSMALLINT*)data->storedRows[i][11].data;
        data->parameters[i]->StrLen_or_IndPtr = 0;

        if (data->parameters[i]->InputOutputType == SQL_PARAM_OUTPUT) {
          SQLSMALLINT bufferSize = 0;
          switch(data->parameters[i]->ParameterType) {
            case SQL_DECIMAL:
            case SQL_NUMERIC:
              bufferSize = (SQLSMALLINT) (data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR);
              data->parameters[i]->ValueType = SQL_C_CHAR;
              data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              data->parameters[i]->DecimalDigits = *(SQLSMALLINT*)data->storedRows[i][9].data;
              break;

            case SQL_DOUBLE:
            case SQL_FLOAT:
              bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + data->parameters[i]->ColumnSize);
              data->parameters[i]->ValueType = SQL_C_DOUBLE;
              data->parameters[i]->ParameterValuePtr = new SQLDOUBLE[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              data->parameters[i]->DecimalDigits = *(SQLSMALLINT*)data->storedRows[i][9].data;
              break;

            case SQL_SMALLINT:
            case SQL_TINYINT:
            case SQL_INTEGER:
            case SQL_BIGINT:
              bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + data->parameters[i]->ColumnSize);
              data->parameters[i]->ValueType = SQL_C_SBIGINT;
              data->parameters[i]->ParameterValuePtr = new SQLBIGINT[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              break;

            case SQL_BINARY:
            case SQL_VARBINARY:
            case SQL_LONGVARBINARY:
              bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR);
              data->parameters[i]->ValueType = SQL_C_CHAR;
              data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              break;

            case SQL_WCHAR:
            case SQL_WVARCHAR:
            case SQL_WLONGVARCHAR:
              bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLWCHAR);
              data->parameters[i]->ValueType = SQL_C_WCHAR;
              data->parameters[i]->ParameterValuePtr = new SQLWCHAR[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              break;

            case SQL_CHAR:
            case SQL_VARCHAR:
            case SQL_LONGVARCHAR:
            default:
              bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR);
              data->parameters[i]->ValueType = SQL_C_CHAR;
              data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              break;
          }
        }
      }

      data->sqlReturnCode = ODBC::BindParameters(data->hSTMT, data->parameters, data->parameterCount);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): BindParameters returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error binding parameters to the procedure\0");
        return;
      }

      // create the statement to call the stored procedure using the ODBC Call escape sequence:
      // need to create the string "?,?,?,?" where the number of '?' is the number of parameters;
      SQLTCHAR *parameterString = new SQLTCHAR[255]();

      for (int i = 0; i < data->parameterCount; i++) {
        if (i == (data->parameterCount - 1)) {
          strcat((char *)parameterString, "?"); // for last parameter, don't add ','
        } else {
          strcat((char *)parameterString, "?,");
        }
      }

      data->deleteColumns(); // delete data in columns for next result set

      data->sql = new SQLTCHAR[255]();
      sprintf((char *)data->sql, "{ CALL %s (%s) }", combinedProcedureName, parameterString);

      delete[] combinedProcedureName;

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): Calling SQLExecDirect(StatementHandle = %p, StatementText = %s, TextLength = %d)\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->sql, SQL_NTS);
      data->sqlReturnCode = SQLExecDirect(
        data->hSTMT, // StatementHandle
        data->sql,   // StatementText
        SQL_NTS      // TextLength
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): SQLExecDirect() FAILED: SQL RETURN = %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error calling the procedure\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): SQLExecDirect() succeeded: SQLRETURN = %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): RetrieveResultSet returned %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving the results from the procedure call\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CallProcedureAsyncWorker::OnOk()\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data);

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

  public:
    CallProcedureAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~CallProcedureAsyncWorker() {
      delete data;
    }
};

/*  TODO: Change
 *  ODBCConnection::CallProcedure
 *
 *    Description: Calls a procedure in the database.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'query'.
 *
 *        info[0]: String: the name of the procedure
 *        info[1?]: Array: optional array of parameters to bind to the procedure call
 *        info[1/2]: Function: callback function:
 *            function(error, result)
 *              error: An error object if the connection was not opened, or
 *                     null if operation was successful.
 *              result: A string containing the info requested.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::CallProcedure(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CallProcedure()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData();
  std::vector<Napi::Value> values;

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::TypeError::New(env, "callProcedure: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::TypeError::New(env, "callProcedure: second argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->procedure = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else {
    Napi::TypeError::New(env, "callProcedure: third argument must be a string").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  // check if parameters were passed or not
  if (info.Length() == 5 && info[3].IsArray() && info[4].IsFunction()) {
    Napi::Array parameterArray = info[3].As<Napi::Array>();
    data->bindValueCount = parameterArray.Length();
    data->parameters = new Parameter*[data->bindValueCount]();
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&parameterArray, data->parameters);
  } else if ((info.Length() == 4 && info[4].IsFunction()) || (info.Length() == 5 && info[3].IsNull() && info[4].IsFunction())) {
    data->parameters = 0;
  } else {
    Napi::TypeError::New(env, "[odbc]: Wrong function signature in call to Connection.callProcedure({string}, {array}[optional], {function}).").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[info.Length() - 1].As<Napi::Function>();

  CallProcedureAsyncWorker *worker = new CallProcedureAsyncWorker(this, data, callback);
  worker->Queue();
  return env.Undefined();
}

/*
 *  ODBCConnection::GetUsername
 *
 *    Description: Returns the username requested from the connection.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'getInfo'.
 *
 *        info[0]: Number: option
 *        info[4]: Function: callback function:
 *            function(error, result)
 *              error: An error object if the connection was not opened, or
 *                     null if operation was successful.
 *              result: A string containing the info requested.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::GetUsername(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::GetUsername()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return this->GetInfo(env, SQL_USER_NAME);
}

Napi::Value ODBCConnection::GetInfo(const Napi::Env env, const SQLUSMALLINT option) {

  SQLTCHAR infoValue[255];
  SQLSMALLINT infoLength;
  SQLRETURN sqlReturnCode = SQLGetInfo(
                              this->hDBC,        // ConnectionHandle
                              SQL_USER_NAME,     // InfoType
                              infoValue,         // InfoValuePtr
                              sizeof(infoValue), // BufferLength
                              &infoLength);      // StringLengthPtr

  if (SQL_SUCCEEDED(sqlReturnCode)) {
    #ifdef UNICODE
      return Napi::String::New(env, (const char16_t *)infoValue, infoLength);
    #else
      return Napi::String::New(env, (const char *) infoValue, infoLength);
    #endif
  }

  // TODO: Fix
  // Napi::Error(env, Napi::String::New(env, ODBC::GetSQLError(SQL_HANDLE_DBC, this->hDBC, (char *) "[node-odbc] Error in ODBCConnection::GetInfo"))).ThrowAsJavaScriptException();
  return env.Null();
}

/******************************************************************************
 ********************************** TABLES ************************************
 *****************************************************************************/

// TablesAsyncWorker, used by Tables function (see below)
class TablesAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData *data;

    void Execute() {

      uv_mutex_lock(&ODBC::g_odbcMutex);
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::TablesAsyncWorker::Execute(): Calling SQLAllocHandle(HandleType = %d, InputHandle = %p, OutputHandlePtr = %p)\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT);
      data->sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,            // HandleType
        odbcConnectionObject->hDBC, // InputHandle
        &data->hSTMT                // OutputHandlePtr
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::TablesProcedureAsyncWorker::Execute(): SQLAllocHandle FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error allocating a statement handle to get table information\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::TablesProcedureAsyncWorker::Execute(): SQLAllocHandle succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::TablesAsyncWorker::Execute(): Calling SQLTables(StatementHandle = %p, CatalogName = %s, NameLength1 = %d, SchemaName = %s, NameLength2 = %d, TableName = %s, NameLength3 = %d, TableType = %s, NameLength4 = %d)\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->catalog, SQL_NTS, data->schema, SQL_NTS, data->table, SQL_NTS, data->type, SQL_NTS);
      data->sqlReturnCode = SQLTables(
        data->hSTMT,   // StatementHandle
        data->catalog, // CatalogName
        SQL_NTS,       // NameLength1
        data->schema,  // SchemaName
        SQL_NTS,       // NameLength2
        data->table,   // TableName
        SQL_NTS,       // NameLength3
        data->type,    // TableType
        SQL_NTS        // NameLength4
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::TablesProcedureAsyncWorker::Execute(): SQLTables returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error getting table information\0");
        return;
      }

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::TablesProcedureAsyncWorker::Execute(): RetrieveResultSet returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving table information results set\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::OnOk()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data);
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

  public:

    TablesAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~TablesAsyncWorker() {
      delete data;
    }
};

/*
 *  ODBCConnection::Tables
 *
 *    Description: Returns the list of table, catalog, or schema names, and
 *                 table types, stored in a specific data source.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'tables'.
 *
 *        info[0]: String: catalog
 *        info[1]: String: schema
 *        info[2]: String: table
 *        info[3]: String: type
 *        info[4]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a database issue
 *              result: The ODBCResult
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::Tables(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 5) {
    Napi::TypeError::New(env, "tables() function takes 5 arguments.").ThrowAsJavaScriptException();
  }

  Napi::Function callback;
  QueryData* data = new QueryData();

  // Napi doesn't have LowMemoryNotification like NAN did. Throw standard error.
  if (!data) {
    Napi::TypeError::New(env, "Could not allocate enough memory to run query.").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::TypeError::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::TypeError::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::TypeError::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::TypeError::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[4].IsFunction()) { callback = info[4].As<Napi::Function>(); }
  else {
    Napi::TypeError::New(env, "tables: fifth argument must be a function").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  TablesAsyncWorker *worker = new TablesAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ********************************* COLUMNS ************************************
 *****************************************************************************/

// ColumnsAsyncWorker, used by Columns function (see below)
class ColumnsAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData *data;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ColumnsAsyncWorker::Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      uv_mutex_lock(&ODBC::g_odbcMutex);
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ColumnsAsyncWorker::Execute(): Calling SQLAllocHandle(HandleType = %d, InputHandle = %p, OutputHandle = %p)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT);
      data->sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,            // HandleType
        odbcConnectionObject->hDBC, // InputHandle
        &data->hSTMT                // OutputHandlePtr
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ColumnsAsyncWorker::Execute(): SQLAllocHandle FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error allocating a statement handle to get column information\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ColumnsAsyncWorker::Execute(): SQLAllocHandle passed: SQLRETURN = %d, OutputHandle = %p\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode, data->hSTMT);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::ColumnsAsyncWorker::Execute(): Calling SQLColumns(StatementHandle = %p, CatalogName = %s, NameLength1 = %d, SchemaName = %s, NameLength2 = %d, TableName = %s, NameLength3 = %d, ColumnName = %s, NameLength4 = %d)\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->catalog, SQL_NTS, data->schema, SQL_NTS, data->table, SQL_NTS, data->column, SQL_NTS);
      data->sqlReturnCode = SQLColumns(
        data->hSTMT,   // StatementHandle
        data->catalog, // CatalogName
        SQL_NTS,       // NameLength1
        data->schema,  // SchemaName
        SQL_NTS,       // NameLength2
        data->table,   // TableName
        SQL_NTS,       // NameLength3
        data->column,  // ColumnName
        SQL_NTS        // NameLength4
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::ColumnsProcedureAsyncWorker::Execute(): SQLTables returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error getting column information\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::ColumnsAsyncWorker::Execute(): SQLColumns() succeeded: SQLRETURN = %d\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);

      data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::ColumnsProcedureAsyncWorker::Execute(): RetrieveResultSet returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving column information results set\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::ColumnsAsyncWorker::OnOk()\n", this->odbcConnectionObject->hENV, this->odbcConnectionObject->hDBC, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);
      Callback().Call(callbackArguments);
    }

  public:

    ColumnsAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~ColumnsAsyncWorker() {
      delete data;
    }
};

/*
 *  ODBCConnection::Columns
 *
 *    Description: Returns the list of column names in specified tables.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'columns'.
 *
 *        info[0]: String: catalog
 *        info[1]: String: schema
 *        info[2]: String: table
 *        info[3]: String: column
 *        info[4]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a database error
 *              result: The ODBCResult
 *
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::Columns(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData* data = new QueryData();
  Napi::Function callback;

  // Napi doesn't have LowMemoryNotification like NAN did. Throw standard error.
  if (!data) {
    Napi::Error::New(env, "Could not allocate enough memory to run query.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::Error::New(env, "columns: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::Error::New(env, "columns: second argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::Error::New(env, "columns: third argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::Error::New(env, "columns: fourth argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[4].IsFunction()) { callback = info[4].As<Napi::Function>(); }
  else {
    Napi::Error::New(env, "columns: fifth argument must be a function").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  ColumnsAsyncWorker *worker = new ColumnsAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 **************************** BEGIN TRANSACTION *******************************
 *****************************************************************************/

// BeginTransactionAsyncWorker, used by EndTransaction function (see below)
class BeginTransactionAsyncWorker : public ODBCAsyncWorker {

  public:

    BeginTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~BeginTransactionAsyncWorker() {}

  private:

    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::BeginTransactionAsyncWorker::Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      //set the connection manual commits
      sqlReturnCode = SQLSetConnectAttr(
        odbcConnectionObject->hDBC,      // ConnectionHandle
        SQL_ATTR_AUTOCOMMIT,             // Attribute
        (SQLPOINTER) SQL_AUTOCOMMIT_OFF, // ValuePtr
        SQL_NTS                          // StringLength
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::BeginTransactionAsyncWorker::Execute(): SQLSetConnectAttr returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error turning off autocommit on the connection\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::BeginTransactionAsyncWorker::OnOK()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }
};

/*
 *  ODBCConnection::BeginTransaction (Async)
 *
 *    Description: Begin a transaction by turning off SQL_ATTR_AUTOCOMMIT.
 *                 Transaction is commited or rolledback in EndTransaction or
 *                 EndTransactionSync.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'beginTransaction'.
 *
 *        info[0]: Function: callback function:
 *            function(error)
 *              error: An error object if the transaction wasn't started, or
 *                     null if operation was successful.
 *
 *    Return:
 *      Napi::Value:
 *        Boolean, indicates whether the transaction was successfully started
 */
Napi::Value ODBCConnection::BeginTransaction(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::BeginTransaction\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback;

  if (info[0].IsFunction()) { callback = info[0].As<Napi::Function>(); }
  else { Napi::Error::New(env, "beginTransaction: first argument must be a function").ThrowAsJavaScriptException(); }

  BeginTransactionAsyncWorker *worker = new BeginTransactionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ***************************** END TRANSACTION ********************************
 *****************************************************************************/

 // EndTransactionAsyncWorker, used by Commit and Rollback functions (see below)
class EndTransactionAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    SQLSMALLINT completionType;
    SQLRETURN sqlReturnCode;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::Execute()", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      sqlReturnCode = SQLEndTran(
        SQL_HANDLE_DBC,             // HandleType
        odbcConnectionObject->hDBC, // Handle
        completionType              // CompletionType
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::Execute(): SQLEndTran returned %d", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error ending the transaction on the connection\0");
        return;
      }

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::Execute(): Running SQLSetConnectAttr(ConnectionHandle = %p, Attribute = %d, ValuePtr = %p, StringLength = %d)", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
      //Reset the connection back to autocommit
      sqlReturnCode = SQLSetConnectAttr(
        odbcConnectionObject->hDBC,     // ConnectionHandle
        SQL_ATTR_AUTOCOMMIT,            // Attribute
        (SQLPOINTER) SQL_AUTOCOMMIT_ON, // ValuePtr
        SQL_NTS                         // StringLength
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::Execute(): SQLSetConnectAttr returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error turning on autocommit on the connection\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::Execute(): SQLSetConnectAttr succeeded", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::EndTransactionAsyncWorker::OnOK()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  public:

    EndTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, SQLSMALLINT completionType, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      completionType(completionType) {}

    ~EndTransactionAsyncWorker() {}
};


/*
 *  ODBCConnection::Commit
 *
 *    Description: Commit a transaction by calling SQLEndTran on the connection
 *                 in an AsyncWorker with SQL_COMMIT option.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransaction'.
 *
 *        info[0]: Function: callback function:
 *            function(error)
 *              error: An error object if the transaction wasn't ended, or
 *                     null if operation was successful.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined
 */
Napi::Value ODBCConnection::Commit(const Napi::CallbackInfo &info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::Commit()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env, "[odbc]: commit(callback): first argument must be a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  // calls EndTransactionAsyncWorker with SQL_COMMIT option
  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, SQL_COMMIT, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::Rollback
 *
 *    Description: Rollback a transaction by calling SQLEndTran on the connection
 *                 in an AsyncWorker with SQL_ROLLBACK option.
 *
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransaction'.
 *
 *        info[0]: Function: callback function:
 *            function(error)
 *              error: An error object if the transaction wasn't ended, or
 *                     null if operation was successful.
 *
 *    Return:
 *      Napi::Value:
 *        Undefined
 */
Napi::Value ODBCConnection::Rollback(const Napi::CallbackInfo &info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::Rollback\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 1 && !info[0].IsFunction()) {
    Napi::TypeError::New(env, "[odbc]: rollback: first argument must be a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  // calls EndTransactionAsyncWorker with SQL_ROLLBACK option
  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, SQL_ROLLBACK, callback);
  worker->Queue();

  return env.Undefined();
}

// Encapsulates the workflow after a result set is returned (many paths require this workflow).
// Does the following:
//   Calls SQLRowCount, which returns the number of rows affected by the statement.
//   Calls ODBC::BindColumns, which calls:
//     SQLNumResultCols to return the number of columns,
//     SQLDescribeCol to describe those columns, and
//     SQLBindCol to bind the column return data to buffers
//   Calls ODBC::FetchAll, which calls:
//     SQLFetch to fetch all of the result rows
//     SQLCloseCursor to close the cursor on the result set
SQLRETURN ODBCConnection::RetrieveResultSet(QueryData *data) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::RetrieveResultSet()\n", this->hENV, this->hDBC, data->hSTMT);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::RetrieveResultSet(): Running SQLRowCount(StatementHandle = %p, RowCount = %ld)\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT, data->rowCount);
  data->sqlReturnCode = SQLRowCount(
    data->hSTMT,    // StatementHandle
    &data->rowCount // RowCountPtr
  );
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    // if SQLRowCount failed, return early with the returnCode
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::RetrieveResultSet(): SQLRowCount FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::RetrieveResultSet(): SQLRowCount passed: SQLRETURN = %d, RowCount = %ld\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode, data->rowCount);


  data->sqlReturnCode = this->BindColumns(data);
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    // if BindColumns failed, return early with the returnCode
    return data->sqlReturnCode;
  }

  // data->columnCount is set in ODBC::BindColumns above
  if (data->columnCount > 0) {
    data->sqlReturnCode = this->FetchAll(data);
    // if we got to the end of the result set, thats the happy path
    if (data->sqlReturnCode == SQL_NO_DATA) {
      return SQL_SUCCESS;
    }
  }
  return data->sqlReturnCode;
}

/******************************************************************************
 ****************************** BINDING COLUMNS *******************************
 *****************************************************************************/
SQLRETURN ODBCConnection::BindColumns(QueryData *data) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns()\n", this->hENV, this->hDBC, data->hSTMT);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLNumResultCols(StatementHandle = %p, ColumnCount = %d\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT, data->columnCount);
  // SQLNumResultCols returns the number of columns in a result set.
  data->sqlReturnCode = SQLNumResultCols(
    data->hSTMT,       // StatementHandle
    &data->columnCount // ColumnCountPtr
  );
  // if there was an error, set columnCount to 0 and return
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    DEBUG_PRINTF("ODBCConnection::BindColumns(): SQLNumResultCols FAILED: SQLRETURN = %d\n", data->sqlReturnCode);
    data->columnCount = 0;
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLNumResultCols passed: ColumnCount = %d\n", this->hENV, this->hDBC, data->hSTMT, data->columnCount);

  // create Columns for the column data to go into
  data->columns = new Column*[data->columnCount]();
  data->boundRow = new SQLCHAR*[data->columnCount]();

  for (int i = 0; i < data->columnCount; i++) {

    Column *column = new Column();

    column->ColumnName = new SQLTCHAR[this->maxColumnNameLength]();

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLDescribeCol(StatementHandle = %p, ColumnNumber = %d, ColumnName = %s, BufferLength = %d, NameLength = %d, DataType = %d, ColumnSize = %lu, DecimalDigits = %d, Nullable = %d)\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT, i + 1, column->ColumnName, this->maxColumnNameLength, column->NameLength, column->DataType, column->ColumnSize, column->DecimalDigits, column->Nullable);
    data->sqlReturnCode = SQLDescribeCol(
      data->hSTMT,               // StatementHandle
      i + 1,                     // ColumnNumber
      column->ColumnName,        // ColumnName
      this->maxColumnNameLength, // BufferLength
      &column->NameLength,       // NameLengthPtr
      &column->DataType,         // DataTypePtr
      &column->ColumnSize,       // ColumnSizePtr
      &column->DecimalDigits,    // DecimalDigitsPtr
      &column->Nullable          // NullablePtr
    );
    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLDescribeCol FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
      return data->sqlReturnCode;
    }
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLDescribeCol passed: ColumnName = %s, NameLength = %d, DataType = %d, ColumnSize = %lu, DecimalDigits = %d, Nullable = %d\n", this->hENV, this->hDBC, data->hSTMT, column->ColumnName, column->NameLength, column->DataType, column->ColumnSize, column->DecimalDigits, column->Nullable);

    data->columns[i] = column;

    SQLLEN maxColumnLength;
    SQLSMALLINT targetType;

    // bind depending on the column
    switch(column->DataType) {

      case SQL_REAL:
      case SQL_DECIMAL:
      case SQL_NUMERIC:
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLCHAR);
        targetType = SQL_C_CHAR;
        break;

      case SQL_FLOAT:
      case SQL_DOUBLE:
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_DOUBLE;
        break;
      case SQL_TINYINT:
      case SQL_SMALLINT:
      case SQL_INTEGER:
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_SLONG;
        break;

      case SQL_BIGINT :
       maxColumnLength = column->ColumnSize;
       targetType = SQL_C_SBIGINT;
       break;

      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_BINARY;
        break;

      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLWCHAR);
        targetType = SQL_C_WCHAR;
        break;

      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      default:
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLCHAR);
        targetType = SQL_C_CHAR;
        break;
    }

    data->boundRow[i] = new SQLCHAR[maxColumnLength]();

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLBindCol(StatementHandle = %p, ColumnNumber = %d, TargetType = %d, TargetValuePtr = %p, BufferLength = %ld, StrLen_or_Ind = %ld\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT, i + 1, targetType, data->boundRow[i], maxColumnLength, column->StrLen_or_IndPtr);
    // SQLBindCol binds application data buffers to columns in the result set.
    data->sqlReturnCode = SQLBindCol(
      data->hSTMT,              // StatementHandle
      i + 1,                    // ColumnNumber
      targetType,               // TargetType
      data->boundRow[i],        // TargetValuePtr
      maxColumnLength,          // BufferLength
      &column->StrLen_or_IndPtr // StrLen_or_Ind
    );

    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLBindCol FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
      return data->sqlReturnCode;
    }
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLBindCol succeeded: StrLeng_or_IndPtr = %ld\n", this->hENV, this->hDBC, data->hSTMT, column->StrLen_or_IndPtr);
  }
  return data->sqlReturnCode;
}

SQLRETURN ODBCConnection::FetchAll(QueryData *data) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll()\n", this->hENV, this->hDBC, data->hSTMT);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): Running SQLFetch(StatementHandle = %p) (Running multiple times)\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT);
  // continue call SQLFetch, with results going in the boundRow array
  while(SQL_SUCCEEDED(data->sqlReturnCode = SQLFetch(data->hSTMT))) {

    ColumnData *row = new ColumnData[data->columnCount]();

    // Iterate over each column, putting the data in the row object
    for (int i = 0; i < data->columnCount; i++) {

      row[i].size = data->columns[i]->StrLen_or_IndPtr;
      if (row[i].size == SQL_NULL_DATA) {
        row[i].data = NULL;
      } else if (row[i].size == SQL_NO_TOTAL) {
        row[i].size = strlen((char*) data->boundRow[i]);
        row[i].data = new SQLCHAR[row[i].size + 1]();
        memcpy(row[i].data, data->boundRow[i], row[i].size);
      } else if (row[i].size < 0) {
        row[i].size = SQL_NULL_DATA;
        row[i].data = NULL;
      } else {
        row[i].data = new SQLCHAR[row[i].size + 1]();
        memcpy(row[i].data, data->boundRow[i], row[i].size);
      }
    }
    data->storedRows.push_back(row);
  }

  // If SQL_SUCCEEDED failed and return code isn't SQL_NO_DATA, there is an error
  if(data->sqlReturnCode != SQL_NO_DATA) {
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): SQLFetch FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): SQLFetch succeeded: Stored %lu rows of data, each with %d columns\n", this->hENV, this->hDBC, data->hSTMT, data->storedRows.size(), data->columnCount);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): Running SQLCloseCursor(StatementHandle = %p) (Running multiple times)\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT);
  // will return either SQL_ERROR or SQL_NO_DATA
  data->sqlReturnCode = SQLCloseCursor(data->hSTMT);
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): SQLCloseCursor FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::FetchAll(): SQLCloseCursor succeeded\n", this->hENV, this->hDBC, data->hSTMT);
  return data->sqlReturnCode;
}

