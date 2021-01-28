/*
  Copyright (c) 2019, 2021 IBM
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

#define MAX_UTF8_BYTES 4

// object keys for the result object
const char* NAME = "name\0";
const char* DATA_TYPE = "dataType\0";
const char* COLUMN_SIZE = "columnSize\0";
const char* DECIMAL_DIGITS = "decimalDigits\0";
const char* NULLABLE = "nullable\0";
const char* STATEMENT = "statement\0";
const char* PARAMETERS = "parameters\0";
const char* RETURN = "return\0";
const char* COUNT = "count\0";
const char* COLUMNS = "columns\0";

size_t strlen16(const char16_t* string)
{
   const char16_t* str = string;
   while(*str) str++;
   return str - string;
}

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
    InstanceMethod("setIsolationLevel", &ODBCConnection::SetIsolationLevel),

    InstanceAccessor("connected", &ODBCConnection::ConnectedGetter, nullptr),
    InstanceAccessor("autocommit", &ODBCConnection::AutocommitGetter, nullptr),
    InstanceAccessor("connectionTimeout", &ODBCConnection::ConnectTimeoutGetter, &ODBCConnection::ConnectTimeoutSetter),
    InstanceAccessor("loginTimeout", &ODBCConnection::LoginTimeoutGetter, &ODBCConnection::LoginTimeoutSetter)

  });

  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  return exports;
}

/******************************************************************************
 **************************** SET ISOLATION LEVEL *****************************
 *****************************************************************************/

// CloseAsyncWorker, used by Close function (see below)
class SetIsolationLevelAsyncWorker : public ODBCAsyncWorker {

  public:
    SetIsolationLevelAsyncWorker(ODBCConnection *odbcConnectionObject, SQLUINTEGER isolationLevel, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      isolationLevel(isolationLevel){}

    ~SetIsolationLevelAsyncWorker() {}

  private:
    ODBCConnection *odbcConnectionObject;
    SQLUINTEGER isolationLevel;
    SQLRETURN sqlReturnCode;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevelAsyncWorker::Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevelAsyncWorker::Execute(): Calling SQLSetConnectAttr(ConnectionHandle = %p, Attribute = %d, ValuePtr = %p, StringLength = %d)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER) isolationLevel, SQL_NTS);
      sqlReturnCode = SQLSetConnectAttr(
        odbcConnectionObject->hDBC,  // ConnectionHandle
        SQL_ATTR_TXN_ISOLATION,      // Attribute
        (SQLPOINTER) (uintptr_t) isolationLevel, // ValuePtr
        SQL_NTS                      // StringLength
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevelAsyncWorker::Execute(): SQLSetConnectAttr FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
        SetError("[odbc] Error setting isolation level\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevelAsyncWorker::Execute(): SQLSetConnectAttr succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, sqlReturnCode);
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevelAsyncWorker::OnOK()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCConnection::SetIsolationLevel(const Napi::CallbackInfo &info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::SetIsolationLevel()\n", this->hENV, this->hDBC);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLUINTEGER isolationLevel = info[0].As<Napi::Number>().Uint32Value();
  Napi::Function callback = info[1].As<Napi::Function>();

  if (!(isolationLevel & this->availableIsolationLevels)) {
    std::vector<napi_value> callbackArguments;
    callbackArguments.push_back(Napi::Error::New(env, "Isolation level passed to setIsolationLevel is not valid for the connection!").Value());
    callback.Call(callbackArguments);
  }

  SetIsolationLevelAsyncWorker *worker = new SetIsolationLevelAsyncWorker(this, isolationLevel, callback);
  worker->Queue();

  return env.Undefined();
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
  this->availableIsolationLevels = *(info[4].As<Napi::External<SQLUINTEGER>>().Data());

  this->connectionTimeout = 0;
  this->loginTimeout = 5;
}

ODBCConnection::~ODBCConnection() {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::~ODBCConnection\n", hENV, hDBC);

  // this->Free();
  return;
}

SQLRETURN ODBCConnection::Free() {
  DEBUG_PRINTF("ODBCConnection::Free()\n");

  SQLRETURN returnCode = SQL_SUCCESS;

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
Napi::Array ODBCConnection::ProcessDataForNapi(Napi::Env env, QueryData *data, Napi::Reference<Napi::Array> *napiParameters) {

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
    #ifdef UNICODE
    rows.Set(Napi::String::New(env, STATEMENT), Napi::String::New(env, (const char16_t*)data->sql));
    #else
    rows.Set(Napi::String::New(env, STATEMENT), Napi::String::New(env, (const char*)data->sql));
    #endif
  }

  // set the 'parameters' property
  if (napiParameters->IsEmpty()) {
    rows.Set(Napi::String::New(env, PARAMETERS), env.Undefined());
  } else {
    rows.Set(Napi::String::New(env, PARAMETERS), napiParameters->Value());
  }

  // set the 'return' property
  rows.Set(Napi::String::New(env, RETURN), env.Undefined()); // TODO: This doesn't exist on my DBMS of choice, need to test on MSSQL Server or similar

  // set the 'count' property
  rows.Set(Napi::String::New(env, COUNT), Napi::Number::New(env, (double)data->rowCount));

  // construct the array for the 'columns' property and then set
  Napi::Array napiColumns = Napi::Array::New(env);

  for (SQLSMALLINT h = 0; h < columnCount; h++) {
    Napi::Object column = Napi::Object::New(env);
    #ifdef UNICODE
    column.Set(Napi::String::New(env, NAME), Napi::String::New(env, (const char16_t*)columns[h]->ColumnName));
    #else
    column.Set(Napi::String::New(env, NAME), Napi::String::New(env, (const char*)columns[h]->ColumnName));
    #endif
    column.Set(Napi::String::New(env, DATA_TYPE), Napi::Number::New(env, columns[h]->DataType));
    column.Set(Napi::String::New(env, COLUMN_SIZE), Napi::Number::New(env, columns[h]->ColumnSize));
    column.Set(Napi::String::New(env, DECIMAL_DIGITS), Napi::Number::New(env, columns[h]->DecimalDigits));
    column.Set(Napi::String::New(env, NULLABLE), Napi::Boolean::New(env, columns[h]->Nullable));
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
            switch(columns[j]->bind_type) {
              case SQL_C_DOUBLE:
                value = Napi::Number::New(env, storedRow[j].double_data);
                break;
              default:
                value = Napi::Number::New(env, atof((const char*)storedRow[j].char_data));
                break;
            }
            break;
          // Napi::Number
          case SQL_FLOAT:
          case SQL_DOUBLE:
            switch(columns[j]->bind_type) {
              case SQL_C_DOUBLE:
                value = Napi::Number::New(env, storedRow[j].double_data);
                break;
              default:
                value = Napi::Number::New(env, atof((const char*)storedRow[j].char_data));
                break;
              }
            break;
          case SQL_TINYINT:
          case SQL_SMALLINT:
          case SQL_INTEGER:
            switch(columns[j]->bind_type) {
              case SQL_C_UTINYINT:
                value  = Napi::Number::New(env, storedRow[j].tinyint_data);
                break;
              case SQL_C_USHORT:
                value  = Napi::Number::New(env, storedRow[j].smallint_data);
                break;
              case SQL_C_SLONG:
                value  = Napi::Number::New(env, storedRow[j].integer_data);
                break;
              default:
                value = Napi::Number::New(env, *(int*)storedRow[j].char_data);
                break;
              }
            break;
          // Napi::BigInt
          case SQL_BIGINT:
            switch(columns[j]->bind_type) {
              case SQL_C_UBIGINT:
                value = Napi::BigInt::New(env, (int64_t)storedRow[j].ubigint_data);
                break;
              default:
                value = Napi::BigInt::New(env, *(int64_t*)storedRow[j].char_data);
                break;
              }
            break;
          // Napi::ArrayBuffer
          case SQL_BINARY :
          case SQL_VARBINARY :
          case SQL_LONGVARBINARY : {
            SQLCHAR *binaryData = new SQLCHAR[storedRow[j].size]; // have to save the data on the heap
            memcpy((SQLCHAR *) binaryData, storedRow[j].char_data, storedRow[j].size);
            value = Napi::ArrayBuffer::New(env, binaryData, storedRow[j].size, [](Napi::Env env, void* finalizeData) {
              delete[] (SQLCHAR*)finalizeData;
            });
            break;
          }
          // Napi::String (char16_t)
          case SQL_WCHAR :
          case SQL_WVARCHAR :
          case SQL_WLONGVARCHAR :
            value = Napi::String::New(env, (const char16_t*)storedRow[j].wchar_data, storedRow[j].size);
            break;
          // Napi::String (char)
          case SQL_CHAR :
          case SQL_VARCHAR :
          case SQL_LONGVARCHAR :
          default:
            value = Napi::String::New(env, (const char*)storedRow[j].char_data, storedRow[j].size);
            break;
        }
      }
      if (this->connectionOptions.fetchArray == true) {
        row.Set(j, value);
      } else {
        #ifdef UNICODE
        row.Set(Napi::String::New(env, (const char16_t*)columns[j]->ColumnName), value);
        #else
        row.Set(Napi::String::New(env, (const char*)columns[j]->ColumnName), value);
        #endif
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
    SQLRETURN       returnCode;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      uv_mutex_lock(&ODBC::g_odbcMutex);
      // When closing, make sure any transactions are closed as well. Because we don't know whether
      // we should commit or rollback, so we default to rollback.
      if (odbcConnectionObject->hDBC != SQL_NULL_HANDLE) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): Calling SQLEndTran(HandleType = SQL_HANDLE_DBC, ConnectionHandle = %p, CompletionType = SQL_ROLLBACK)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC);
        returnCode = SQLEndTran(
          SQL_HANDLE_DBC,             // HandleType
          odbcConnectionObject->hDBC, // Handle
          SQL_ROLLBACK                // CompletionType
        );
        if (!SQL_SUCCEEDED(returnCode)) {
          uv_mutex_unlock(&ODBC::g_odbcMutex);
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLEndTran FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
          SetError("[odbc] Error ending potential transactions when closing the connection\0");
          return;
        }
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLEndTran succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);

        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): Calling SQLDisconnect(ConnectionHandle = %p)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC);
        returnCode = SQLDisconnect(
          odbcConnectionObject->hDBC // ConnectionHandle
        );
        if (!SQL_SUCCEEDED(returnCode)) {
          uv_mutex_unlock(&ODBC::g_odbcMutex);
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLDisconnect FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
          SetError("[odbc] Error disconnecting when closing the connection\0");
          return;
        }
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLDisconnect succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);

        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): Calling SQLFreeHandle(HandleType = SQL_HANDLE_DBC, Handle = %p)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC);
        returnCode = SQLFreeHandle(
          SQL_HANDLE_DBC,            // HandleType
          odbcConnectionObject->hDBC // Handle
        );
        if (!SQL_SUCCEEDED(returnCode)) {
          uv_mutex_unlock(&ODBC::g_odbcMutex);
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLFreeHandle FAILED: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
          SetError("[odbc] Error freeing connection handle when closing the connection\0");
          return;
        }
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::CloseAsyncWorker::Execute(): SQLFreeHandle succeeded: SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, returnCode);
        odbcConnectionObject->hDBC = SQL_NULL_HANDLE;
      }
      uv_mutex_unlock(&ODBC::g_odbcMutex);
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

    ODBCConnection               *odbcConnectionObject;
    Napi::Reference<Napi::Array>  napiParameters;
    QueryData                    *data;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): Running SQL '%s'\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, (char*)data->sql);
      
      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      if (odbcConnectionObject->hDBC == SQL_NULL_HANDLE) {
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLHDBC was SQL_NULL_HANDLE!\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);
        SetError("[odbc] Database connection handle was no longer valid. Cannot run a query after closing the connection.");
        return;
      } else {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): Running SQLAllocHandle(HandleType = SQL_HANDLE_STMT, InputHandle = %p, OutputHandlePtr = %p)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, odbcConnectionObject->hDBC, &(data->hSTMT));
        data->sqlReturnCode = SQLAllocHandle(
          SQL_HANDLE_STMT,            // HandleType
          odbcConnectionObject->hDBC, // InputHandle
          &(data->hSTMT)              // OutputHandlePtr
        );
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLAllocHandle returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, odbcConnectionObject->hDBC);
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

          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): Calling SQLExecute(Statment Handle = %p)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT);
          data->sqlReturnCode = SQLExecute(data->hSTMT);
          if (!SQL_SUCCEEDED(data->sqlReturnCode) && data->sqlReturnCode != SQL_NO_DATA) {
            DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecute returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
            this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
            SetError("[odbc] Error executing the sql statement\0");
            return;
          }
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecute passed with SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        }
        // querying without parameters, can just use SQLExecDirect
        else {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): Calling SQLExecDirect(Statment Handle = %p, StatementText = %s, TextLength = SQL_NTS)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->sql);
          data->sqlReturnCode = SQLExecDirect(
            data->hSTMT,
            data->sql,
            SQL_NTS
          );
          if (!SQL_SUCCEEDED(data->sqlReturnCode) && data->sqlReturnCode != SQL_NO_DATA) {
            DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecDirect returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
            this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
            SetError("[odbc] Error executing the sql statement\0");
            return;
          }
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLExecDirect passed with SQLRETURN = %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
        }

        if (data->sqlReturnCode != SQL_NO_DATA) {
          data->sqlReturnCode = odbcConnectionObject->RetrieveResultSet(data);
          if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
            this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
            SetError("[odbc] Error retrieving the result set from the statement\0");
            return;
          }
        }
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p]ODBCConnection::QueryAsyncWorker::OnOk()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data, &napiParameters);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

  public:
    QueryAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Value napiParameterArray, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {
        if (napiParameterArray.IsArray()) {
          napiParameters = Napi::Persistent(napiParameterArray.As<Napi::Array>());
        } else {
          napiParameters = Napi::Reference<Napi::Array>();
        }
      }

    ~QueryAsyncWorker() {
      uv_mutex_lock(&ODBC::g_odbcMutex);
      // It is possible the connection handle has been freed, which freed the
      // statement handle as well. Freeing again would cause a segfault.
      if (this->odbcConnectionObject->hDBC != SQL_NULL_HANDLE) {
        this->data->sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, this->data->hSTMT);
      }
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
  Napi::Value napiParameterArray = env.Null();

  // check if parameters were passed or not
  if (info.Length() == 3 && info[0].IsString() && info[1].IsArray() && info[2].IsFunction()) {
    napiParameterArray = info[1];
    Napi::Array napiArray = napiParameterArray.As<Napi::Array>();
    data->bindValueCount = (SQLSMALLINT)napiArray.As<Napi::Array>().Length();
    data->parameters = new Parameter*[data->bindValueCount];
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&napiArray, data->parameters);
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

  worker = new QueryAsyncWorker(this, napiParameterArray, data, callback);
  worker->Queue();
  return env.Undefined();
}

// If we have a parameter with input/output params (e.g. calling a procedure),
// then we need to take the Parameter structures of the QueryData and create
// a Napi::Array from those that were overwritten.
void ODBCConnection::ParametersToArray(Napi::Reference<Napi::Array> *napiParameters, QueryData *data, unsigned char *overwriteParameters) {
  Parameter **parameters = data->parameters;
  Napi::Array napiArray = napiParameters->Value();
  Napi::Env env = napiParameters->Env();

  for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
    if (overwriteParameters[i] == 1) {
      Napi::Value value;

      // check for null data
      if (parameters[i]->StrLen_or_IndPtr == SQL_NULL_DATA) {
        value = env.Null();
      } else {
        switch(parameters[i]->ParameterType) {
          case SQL_REAL:
          case SQL_NUMERIC:
          case SQL_DECIMAL:
            value = Napi::Number::New(env, atof((const char*)parameters[i]->ParameterValuePtr));
            break;
          // Napi::Number
          case SQL_FLOAT:
          case SQL_DOUBLE:
            value = Napi::Number::New(env, *(double*)parameters[i]->ParameterValuePtr);
            break;
          case SQL_TINYINT:
          case SQL_SMALLINT:
          case SQL_INTEGER:
            value = Napi::Number::New(env, *(int*)parameters[i]->ParameterValuePtr);
            break;
          // Napi::BigInt
          case SQL_BIGINT:
            value = Napi::BigInt::New(env, *(int64_t*)parameters[i]->ParameterValuePtr);
            break;
          // Napi::ArrayBuffer
          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            value = Napi::ArrayBuffer::New(env, parameters[i]->ParameterValuePtr, parameters[i]->StrLen_or_IndPtr);
            break;
          // Napi::String (char16_t)
          case SQL_WCHAR:
          case SQL_WVARCHAR:
          case SQL_WLONGVARCHAR:
            value = Napi::String::New(env, (const char16_t*)parameters[i]->ParameterValuePtr, parameters[i]->StrLen_or_IndPtr/sizeof(SQLWCHAR));
            break;
          // Napi::String (char)
          case SQL_CHAR:
          case SQL_VARCHAR:
          case SQL_LONGVARCHAR:
          default:
            value = Napi::String::New(env, (const char*)parameters[i]->ParameterValuePtr);
            break;
        }
      }
      napiArray.Set(i, value);
    } 
  }
}


/******************************************************************************
 ***************************** CALL PROCEDURE *********************************
 *****************************************************************************/

// CallProcedureAsyncWorker, used by CreateProcedure function (see below)
class CallProcedureAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    Napi::Reference<Napi::Array>    napiParameters;
    QueryData      *data;
    unsigned char  *overwriteParams;

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

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::CallProcedureAsyncWorker::Execute(): Calling SQLProcedureColumns(StatementHandle = %p, CatalogName = %s, NameLength1 = %d, SchemaName = %s, NameLength2 = %d, ProcName = %s, NameLength3 = %d, ColumnName = %ld, NameLength4 = %d)\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->hSTMT, data->catalog, SQL_NTS, data->schema, SQL_NTS, data->procedure, SQL_NTS, NULL, SQL_NTS);
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

      #define SQLPROCEDURECOLUMNS_COLUMN_TYPE_INDEX     4
      #define SQLPROCEDURECOLUMNS_DATA_TYPE_INDEX       5
      #define SQLPROCEDURECOLUMNS_COLUMN_SIZE_INDEX     7
      #define SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX  9
      #define SQLPROCEDURECOLUMNS_NULLABLE_INDEX       11

      // get stored column parameter data from the result set
      for (int i = 0; i < data->parameterCount; i++) {
        data->parameters[i]->InputOutputType = data->storedRows[i][SQLPROCEDURECOLUMNS_COLUMN_TYPE_INDEX].smallint_data;
        data->parameters[i]->ParameterType = data->storedRows[i][SQLPROCEDURECOLUMNS_DATA_TYPE_INDEX].smallint_data; // DataType -> ParameterType
        data->parameters[i]->ColumnSize = data->storedRows[i][SQLPROCEDURECOLUMNS_COLUMN_SIZE_INDEX].integer_data; // ParameterSize -> ColumnSize
        data->parameters[i]->Nullable = data->storedRows[i][SQLPROCEDURECOLUMNS_NULLABLE_INDEX].smallint_data;

        if (data->parameters[i]->InputOutputType == SQL_PARAM_OUTPUT) {
          SQLSMALLINT bufferSize = 0;
          data->parameters[i]->StrLen_or_IndPtr = *(new SQLLEN());
          switch(data->parameters[i]->ParameterType) {
            case SQL_DECIMAL:
            case SQL_NUMERIC:
              bufferSize = (data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR);
              data->parameters[i]->ValueType = SQL_C_CHAR;
              data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize];
              data->parameters[i]->BufferLength = bufferSize;
              data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
              break;

            case SQL_DOUBLE:
            case SQL_FLOAT:
              data->parameters[i]->ValueType = SQL_C_DOUBLE;
              data->parameters[i]->ParameterValuePtr = new SQLDOUBLE();
              data->parameters[i]->BufferLength = sizeof(SQLDOUBLE);
              data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
              break;

            case SQL_TINYINT:
              data->parameters[i]->ValueType = SQL_C_UTINYINT;
              data->parameters[i]->ParameterValuePtr = new SQLCHAR();
              data->parameters[i]->BufferLength = sizeof(SQLCHAR);
              data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
              break;

            case SQL_SMALLINT:
              data->parameters[i]->ValueType = SQL_C_USHORT;
              data->parameters[i]->ParameterValuePtr = new SQLUSMALLINT();
              data->parameters[i]->BufferLength = sizeof(SQLUSMALLINT);
              data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
              break;

            case SQL_INTEGER:
              data->parameters[i]->ValueType = SQL_C_SLONG;
              data->parameters[i]->ParameterValuePtr = new SQLINTEGER();
              data->parameters[i]->BufferLength = sizeof(SQLINTEGER);
              data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
              break;

            case SQL_BIGINT:
              data->parameters[i]->ValueType = SQL_C_SBIGINT;
              data->parameters[i]->ParameterValuePtr = new SQLUBIGINT();
              data->parameters[i]->BufferLength = sizeof(SQLUBIGINT);
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

      // We saved a reference to parameters passed it. Need to tell which
      // parameters we have to overwrite, now that we have 
      this->overwriteParams = new unsigned char[data->parameterCount]();
      for (int i = 0; i < data->parameterCount; i++) {
        if (data->parameters[i]->InputOutputType == SQL_PARAM_OUTPUT || data->parameters[i]->InputOutputType == SQL_PARAM_INPUT_OUTPUT) {
          this->overwriteParams[i] = 1;
        } else {
          this->overwriteParams[i] = 0;
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

      odbcConnectionObject->ParametersToArray(&napiParameters, data, overwriteParams);
      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data, &napiParameters);

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

  public:
    CallProcedureAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Value napiParameterArray, QueryData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {
        if (napiParameterArray.IsArray()) {
          napiParameters = Napi::Persistent(napiParameterArray.As<Napi::Array>());
        } else {
          napiParameters = Napi::Reference<Napi::Array>();
        }
      }

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
  Napi::Value napiParameterArray = env.Null();

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
    napiParameterArray = info[3];
    Napi::Array napiArray = napiParameterArray.As<Napi::Array>();
    data->bindValueCount = (SQLSMALLINT)napiArray.Length();
    data->parameters = new Parameter*[data->bindValueCount];
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&napiArray, data->parameters);
  } else if ((info.Length() == 4 && info[4].IsFunction()) || (info.Length() == 5 && info[3].IsNull() && info[4].IsFunction())) {
    data->parameters = 0;
  } else {
    Napi::TypeError::New(env, "[odbc]: Wrong function signature in call to Connection.callProcedure({string}, {array}[optional], {function}).").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[info.Length() - 1].As<Napi::Function>();

  CallProcedureAsyncWorker *worker = new CallProcedureAsyncWorker(this, napiParameterArray, data, callback);
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
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::TablesProcedureAsyncWorker::OnOk()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Napi::Reference<Napi::Array> empty = Napi::Reference<Napi::Array>();
      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data, &empty);
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

      Napi::Reference<Napi::Array> empty = Napi::Reference<Napi::Array>();
      Napi::Array rows = odbcConnectionObject->ProcessDataForNapi(env, data, &empty);

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
  data->boundRow = new void*[data->columnCount]();

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

    // ensuring ColumnSize values are valid according to ODBC docs
    if (column->DataType == SQL_TYPE_DATE && column->ColumnSize < 10) {
        // ODBC docs say this should be 10, but some drivers have bugs that
        // return invalid values. eg. 4D
        // Ensure that it is a minimum of 10.
        column->ColumnSize = 10;
    }

    if (column->DataType == SQL_TYPE_TIME && column->ColumnSize < 8) {
        // ODBC docs say this should be 8, but some drivers have bugs that
        // return invalid values. eg. 4D
        // Ensure that it is a minimum of 8.
        column->ColumnSize = 8;
    }

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLDescribeCol passed: ColumnName = %s, NameLength = %d, DataType = %d, ColumnSize = %lu, DecimalDigits = %d, Nullable = %d\n", this->hENV, this->hDBC, data->hSTMT, column->ColumnName, column->NameLength, column->DataType, column->ColumnSize, column->DecimalDigits, column->Nullable);
    // bind depending on the column
    switch(column->DataType) {

      case SQL_REAL:
      case SQL_DECIMAL:
      case SQL_NUMERIC:
        column->buffer_size = (column->ColumnSize + 2) * sizeof(SQLCHAR);
        column->bind_type = SQL_C_CHAR;
        data->boundRow[i] = new SQLCHAR[column->buffer_size]();
        break;

      case SQL_FLOAT:
      case SQL_DOUBLE:
        column->buffer_size = sizeof(SQLDOUBLE);
        column->bind_type = SQL_C_DOUBLE;
        data->boundRow[i] = new SQLDOUBLE();
        break;
        
      case SQL_TINYINT:
        column->buffer_size = sizeof(SQLCHAR);
        column->bind_type = SQL_C_UTINYINT;
        data->boundRow[i] = new SQLCHAR();
        break;

      case SQL_SMALLINT:
        column->buffer_size = sizeof(SQLUSMALLINT);
        column->bind_type = SQL_C_USHORT;
        data->boundRow[i] = new SQLUSMALLINT();
        break;

      case SQL_INTEGER:
        column->buffer_size = sizeof(SQLINTEGER);
        column->bind_type = SQL_C_SLONG;
        data->boundRow[i] = new SQLINTEGER();
        break;

      case SQL_BIGINT :
       column->buffer_size = sizeof(SQLUBIGINT);
       column->bind_type = SQL_C_UBIGINT;
       data->boundRow[i] = new SQLUBIGINT();
       break;

      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
        column->buffer_size = column->ColumnSize;
        column->bind_type = SQL_C_BINARY;
        data->boundRow[i] = new SQLCHAR[column->buffer_size]();
        break;

      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
        column->buffer_size = (column->ColumnSize + 1) * sizeof(SQLWCHAR);
        column->bind_type = SQL_C_WCHAR;
        data->boundRow[i] = new SQLWCHAR[column->buffer_size]();
        break;

      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      default:
        column->buffer_size = (column->ColumnSize * MAX_UTF8_BYTES + 1) * sizeof(SQLCHAR);
        column->bind_type = SQL_C_CHAR;
        data->boundRow[i] = new SQLCHAR[column->buffer_size]();
        break;
    }

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLBindCol(StatementHandle = %p, ColumnNumber = %d, TargetType = %d, TargetValuePtr = %p, BufferLength = %ld, StrLen_or_Ind = %ld\n", this->hENV, this->hDBC, data->hSTMT, data->hSTMT, i + 1, column->bind_type, data->boundRow[i], column->buffer_size, column->StrLen_or_IndPtr);
    // SQLBindCol binds application data buffers to columns in the result set.
    data->sqlReturnCode = SQLBindCol(
      data->hSTMT,              // StatementHandle
      i + 1,                    // ColumnNumber
      column->bind_type,        // TargetType
      data->boundRow[i],        // TargetValuePtr
      column->buffer_size,      // BufferLength
      &column->StrLen_or_IndPtr // StrLen_or_Ind
    );

    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLBindCol FAILED: SQLRETURN = %d\n", this->hENV, this->hDBC, data->hSTMT, data->sqlReturnCode);
      return data->sqlReturnCode;
    }
    data->columns[i] = column;
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

      if (data->columns[i]->StrLen_or_IndPtr == SQL_NULL_DATA) {
        row[i].size = SQL_NULL_DATA;
      } else {
        switch (data->columns[i]->bind_type) {
        case SQL_C_DOUBLE:
          row[i].double_data = *(SQLDOUBLE*)data->boundRow[i];
          break;

        case SQL_C_UTINYINT:
          row[i].tinyint_data = *(SQLCHAR*)data->boundRow[i];
          break;

        case SQL_C_USHORT:
          row[i].smallint_data = *(SQLUSMALLINT*)data->boundRow[i];
          break;

        case SQL_C_SLONG:
          row[i].integer_data = *(SQLINTEGER*)data->boundRow[i];
          break;

        case SQL_C_UBIGINT:
          row[i].ubigint_data = *(SQLUBIGINT*)data->boundRow[i];
          break;

        case SQL_C_BINARY:
          row[i].char_data = new SQLCHAR[row[i].size]();
          memcpy(row[i].char_data, data->boundRow[i], row[i].size);
          break;

        case SQL_C_WCHAR:
          row[i].size = strlen16((const char16_t *)data->boundRow[i]);
          row[i].wchar_data = new SQLWCHAR[row[i].size]();
          memcpy(row[i].wchar_data, data->boundRow[i], row[i].size * sizeof(SQLWCHAR));
          break;

        case SQL_C_CHAR:
        default:
          row[i].size = strlen((const char *)data->boundRow[i]);
          // Although fields going from SQL_C_CHAR to Napi::String use
          // row[i].size, NUMERIC data uses atof() which requires a
          // null terminator. Need to add an aditional byte.
          row[i].char_data = new SQLCHAR[row[i].size + 1]();
          memcpy(row[i].char_data, data->boundRow[i], row[i].size);
          break;

        // TODO: Unhandled C types:
        // SQL_C_SSHORT
        // SQL_C_SHORT
        // SQL_C_STINYINT
        // SQL_C_TINYINT
        // SQL_C_ULONG
        // SQL_C_LONG
        // SQL_C_FLOAT
        // SQL_C_BIT
        // SQL_C_STINYINT
        // SQL_C_TINYINT
        // SQL_C_SBIGINT
        // SQL_C_BOOKMARK
        // SQL_C_VARBOOKMARK
        // All C interval data types
        // SQL_C_TYPE_DATE
        // SQL_C_TYPE_TIME
        // SQL_C_TYPE_TIMESTAMP
        // SQL_C_TYPE_NUMERIC
        // SQL_C_GUID
      }
      row[i].bind_type = data->columns[i]->bind_type;
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
