/*
  Copyright (c) 2021, IBM
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
#include "odbc_cursor.h"

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

typedef struct QueryOptions {
  bool         use_cursor;
  SQLTCHAR    *cursor_name;
  SQLSMALLINT  cursor_name_length;
  SQLULEN      fetch_size;
} QueryOptions;

#define QUERY_OPTION_PROPERTY_NAME_CURSOR     "cursor"
#define QUERY_OPTION_PROPERTY_NAME_FETCH_SIZE "fetchSize"

QueryOptions
parse_query_options
(
  Napi::Env   env,
  Napi::Value options_value
)
{
  QueryOptions query_options;

  if (options_value.IsNull())
  {
    query_options.use_cursor  = false;
    query_options.cursor_name = NULL;

    return query_options;
  }

  Napi::Object options_object = options_value.As<Napi::Object>();

  // .cursor property
  if (options_object.HasOwnProperty(QUERY_OPTION_PROPERTY_NAME_CURSOR))
  {
    Napi::Value cursor_value =
      options_object.Get(QUERY_OPTION_PROPERTY_NAME_CURSOR);
    
    if (cursor_value.IsString())
    {
      query_options.use_cursor  = true;
#ifdef UNICODE
      std::u16string cursor_string;
      cursor_string = cursor_value.As<Napi::String>().Utf16Value();
#else
      std::string cursor_string;
      cursor_string = cursor_value.As<Napi::String>().Utf8Value();
#endif
      query_options.cursor_name = (SQLTCHAR *)cursor_string.c_str();
      query_options.cursor_name_length = cursor_string.length();
    }
    else if (cursor_value.IsBoolean())
    {
      query_options.use_cursor  = cursor_value.As<Napi::Boolean>().Value();
      query_options.cursor_name = NULL;
    }
    else
    {
      Napi::TypeError::New(env, "Connection.query options: ." QUERY_OPTION_PROPERTY_NAME_CURSOR " must be a STRING or a BOOLEAN value.").ThrowAsJavaScriptException();
    }
  }
  else
  {
    query_options.use_cursor  = false;
    query_options.cursor_name = NULL;
  }
  // END .cursor property

  // .fetchSize property
  if (options_object.HasOwnProperty(QUERY_OPTION_PROPERTY_NAME_FETCH_SIZE))
  {
    Napi::Value fetch_size_value =
      options_object.Get(QUERY_OPTION_PROPERTY_NAME_FETCH_SIZE);

    if (fetch_size_value.IsNumber())
    {
      // even if the user didn't explicitly set use_cursor to true, if they are
      // passing a fetch size, it should be assumed.
      query_options.use_cursor = true;

      query_options.fetch_size =
        (SQLULEN)fetch_size_value.As<Napi::Number>().Int64Value();

      // Having a fetch size less than 1 doesn't make sense, so switch to a sane
      // value.
      if (query_options.fetch_size < 1)
      {
        query_options.fetch_size = 1;
      }
    }
    else
    {
      Napi::TypeError::New(env, "Connection.query options: ." QUERY_OPTION_PROPERTY_NAME_FETCH_SIZE " must be a NUMBER value.").ThrowAsJavaScriptException();
      return query_options;
    }
  }
  else
  {
    query_options.fetch_size = 1;
  }
  // END .fetchSize property


  return query_options;
}

SQLRETURN
set_fetch_size
(
  StatementData *data,
  SQLULEN        fetch_size
)
{
  data->sqlReturnCode =
  SQLSetStmtAttr
  (  
    data->hSTMT,  
    SQL_ATTR_ROW_ARRAY_SIZE,  
    (SQLPOINTER) fetch_size,  
    0
  );

  data->fetch_size = fetch_size;

  if (!SQL_SUCCEEDED(data->sqlReturnCode))
  {
    return data->sqlReturnCode;
  }

  data->row_status_array =
    new SQLUSMALLINT[fetch_size]();

  data->sqlReturnCode =
  SQLSetStmtAttr
  (  
    data->hSTMT,
    SQL_ATTR_ROW_STATUS_PTR,
    (SQLPOINTER) data->row_status_array,
    0
  );

  return data->sqlReturnCode;
}

// QueryAsyncWorker, used by Query function (see below)
class QueryAsyncWorker : public ODBCAsyncWorker {

  private:

    ODBCConnection               *odbcConnectionObject;
    Napi::Reference<Napi::Array>  napiParameters;
    StatementData                *data;
    QueryOptions                  query_options;

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

        if (query_options.use_cursor == true)
        {
          if (query_options.cursor_name != NULL)
          {
            data->sqlReturnCode =
            SQLSetCursorName
            (
              data->hSTMT,
              query_options.cursor_name,
              query_options.cursor_name_length
            );

            if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
              DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::QueryAsyncWorker::Execute(): SQLSetCursorName returned %d\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC, data->hSTMT, data->sqlReturnCode);
              this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
              SetError("[odbc] Error setting the cursor name on the statement\0");
              return;
            }
          }

          // TODO: DEBUG PRINTF information
          set_fetch_size
          (
            data,
            query_options.fetch_size
          );

          if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
            this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
            SetError("[odbc] Error setting the fetch size on the statement\0");
            return;
          }
        }
        else
        {
          set_fetch_size
          (
            data,
            1
          );
        }

        // querying with parameters, need to prepare, bind, execute
        if (data->bindValueCount > 0) {
          // binds all parameters to the query
          data->sqlReturnCode =
          SQLPrepare
          (
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

          data->sqlReturnCode =
          SQLNumParams
          (
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
          data->sqlReturnCode =
          SQLExecDirect
          (
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
          data->sqlReturnCode = prepare_for_fetch(data);
          if (data->column_count > 0 && query_options.use_cursor == false)
          {
            data->sqlReturnCode = fetch_all_and_store(data);
            if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
              this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
              SetError("[odbc] Error retrieving the result set from the statement\0");
              return;
            }
          }
        }
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p]ODBCConnection::QueryAsyncWorker::OnOk()\n", odbcConnectionObject->hENV, odbcConnectionObject->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      if (query_options.use_cursor)
      {
        // arguments for the ODBCCursor constructor
        std::vector<napi_value> cursor_arguments;
        cursor_arguments.push_back(Napi::External<StatementData>::New(env, data));
        // cursor_arguments.push_back(napiParameters.Value());
  
        // create a new ODBCCursor object as a Napi::Value
        Napi::Value cursorObject = ODBCCursor::constructor.New(cursor_arguments);

        // return cursor
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(cursorObject);

        Callback().Call(callbackArguments);
      }
      else
      {
        Napi::Array rows = process_data_for_napi(env, data, napiParameters.Value());

        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(rows);

        // return results
        Callback().Call(callbackArguments);
      }

      return;
    }

  public:
    QueryAsyncWorker
    (
      ODBCConnection *odbcConnectionObject,
      QueryOptions    query_options,
      Napi::Value     napiParameterArray,
      StatementData  *data,
      Napi::Function& callback
    ) 
    :
    ODBCAsyncWorker(callback),
    odbcConnectionObject(odbcConnectionObject),
    data(data),
    query_options(query_options) 
    {
      if (napiParameterArray.IsArray()) {
        napiParameters = Napi::Persistent(napiParameterArray.As<Napi::Array>());
      } else {
        napiParameters = Napi::Reference<Napi::Array>();
      }
    }

    ~QueryAsyncWorker() {
      if (!query_options.use_cursor)
      {
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

  StatementData *data               = new StatementData();
                 data->henv         = this->hENV;
                 data->hdbc         = this->hDBC;
                 data->fetch_array  = this->connectionOptions.fetchArray;
  QueryOptions   query_options;
  Napi::Value    napiParameterArray = env.Null();
  size_t         argument_count     = info.Length();

  // For the C++ node-addon-api code, all of the function signatures must
  // include a callback function as the final parameter because we need to
  // have a function to pass to the AsyncWorker as a Callback. The JavaScript
  // wrapper functions enforce the correct number of arguments, so just need
  // to check for null/undefined in a given spot.
  if
  (
    argument_count != 4   ||
    info[0].IsNull()      ||
    info[0].IsUndefined() ||
    !info[0].IsString()   ||
    !(
      info[1].IsArray() ||
      info[1].IsNull() ||
      info[1].IsUndefined()
    ) ||
    !(
      info[2].IsObject() ||
      info[2].IsNull() ||
      info[2].IsUndefined()
    ) ||
    info[3].IsNull()      ||
    info[3].IsUndefined() ||
    !info[3].IsFunction()
  )
  {
    Napi::TypeError::New(env, "[node-odbc]: Wrong function signature in call to Connection.query({string}, {array}[optional], {object}[optional], {function}).").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Store the SQL query string in the data structure
  Napi::String sql = info[0].ToString();
  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

  // Store the callback function to call at the end of the AsyncWorker
  Napi::Function callback = info[3].As<Napi::Function>();

  // If info[1] is not null or undefined, it is an array holding our parameters
  if (!(info[1].IsNull() || info[1].IsUndefined()))
  {
    napiParameterArray = info[1];
    Napi::Array napiArray = napiParameterArray.As<Napi::Array>();
    data->bindValueCount = (SQLSMALLINT)napiArray.As<Napi::Array>().Length();
    data->parameters = new Parameter*[data->bindValueCount];
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&napiArray, data->parameters);
  }

  // if info[2] is not null or undefined or an array or a function, it is an
  // object holding the query options
  if (
    (
      !info[2].IsObject()   ||
      info[2].IsNull()      ||
      info[2].IsUndefined() ||
      info[2].IsArray()     ||
      info[2].IsFunction()
    )
  )
  {
    query_options = parse_query_options(env, env.Null());
  }
  else
  {
    query_options = parse_query_options(env, info[2].As<Napi::Object>());
  }

  // Have parsed the arguments, now create the AsyncWorker and queue the work
  QueryAsyncWorker *worker;
  worker = new QueryAsyncWorker(this, query_options, napiParameterArray, data, callback);
  worker->Queue();

  return env.Undefined();
}

// If we have a parameter with input/output params (e.g. calling a procedure),
// then we need to take the Parameter structures of the StatementData and create
// a Napi::Array from those that were overwritten.
void ODBCConnection::ParametersToArray(Napi::Reference<Napi::Array> *napiParameters, StatementData *data, unsigned char *overwriteParameters) {
  Parameter **parameters = data->parameters;
  Napi::Array napiArray = napiParameters->Value();
  Napi::Env env = napiParameters->Env();

  for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
    Parameter *parameter = parameters[i];
    if (overwriteParameters[i] == 1) {
      Napi::Value value;

      // check for null data
      if (*parameter->StrLen_or_IndPtr == SQL_NULL_DATA) {
        value = env.Null();
      } else {
        // Create a JavaScript value based on the C type that was bound to
        switch(parameter->ValueType) {
          case SQL_C_DOUBLE:
            value = Napi::Number::New(env, *(double*)parameter->ParameterValuePtr);
            break;
          case SQL_C_SLONG:
            value = Napi::Number::New(env, *(SQLINTEGER*)parameter->ParameterValuePtr);
            break;
          case SQL_C_ULONG:
            value = Napi::Number::New(env, *(SQLUINTEGER*)parameter->ParameterValuePtr);
            break;
          // Napi::BigInt
          case SQL_C_SBIGINT:
#if NAPI_VERSION > 5
            if (parameter->isbigint ==  true) {
              value = Napi::BigInt::New(env, *(int64_t*)parameter->ParameterValuePtr);
            } else {
              value = Napi::Number::New(env, *(int64_t*)parameter->ParameterValuePtr);
            }
#else
              value = Napi::Number::New(env, *(int64_t*)parameter->ParameterValuePtr);
#endif
            break;
          case SQL_C_UBIGINT:
            value = Napi::BigInt::New(env, *(uint64_t*)parameter->ParameterValuePtr);
            break;
          case SQL_C_SSHORT:
            value = Napi::Number::New(env, *(signed short*)parameter->ParameterValuePtr);
            break;
          case SQL_C_USHORT:
            value = Napi::Number::New(env, *(unsigned short*)parameter->ParameterValuePtr);
          case SQL_BIGINT:
#if NAPI_VERISON > 5
            value = Napi::BigInt::New(env, *(int64_t*)parameters[i]->ParameterValuePtr);
#else
            value = Napi::Number::New(env, *(int64_t*)parameters[i]->ParameterValuePtr);
#endif
            break;
          // Napi::ArrayBuffer
          case SQL_C_BINARY:
            // value = Napi::Buffer<SQLCHAR>::New(env, (SQLCHAR*)parameter->ParameterValuePtr, *parameter->StrLen_or_IndPtr);
            value = Napi::Buffer<SQLCHAR>::Copy(env, (SQLCHAR*)parameter->ParameterValuePtr, *parameter->StrLen_or_IndPtr);
            break;
          // Napi::String (char16_t)
          case SQL_C_WCHAR:
            value = Napi::String::New(env, (const char16_t*)parameter->ParameterValuePtr, *parameter->StrLen_or_IndPtr/sizeof(SQLWCHAR));
            break;
          // Napi::String (char)
          case SQL_C_CHAR:
          default:
            value = Napi::String::New(env, (const char*)parameter->ParameterValuePtr);
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
    StatementData  *data;
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

      
      data->sqlReturnCode = prepare_for_fetch(data);
      data->sqlReturnCode = fetch_all_and_store(data);
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

      data->sqlReturnCode = prepare_for_fetch(data);
      data->sqlReturnCode = fetch_all_and_store(data);
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

        Parameter *parameter = data->parameters[i];

        data->parameters[i]->InputOutputType = data->storedRows[i][SQLPROCEDURECOLUMNS_COLUMN_TYPE_INDEX].smallint_data;
        data->parameters[i]->ParameterType = data->storedRows[i][SQLPROCEDURECOLUMNS_DATA_TYPE_INDEX].smallint_data; // DataType -> ParameterType
        data->parameters[i]->ColumnSize = data->storedRows[i][SQLPROCEDURECOLUMNS_COLUMN_SIZE_INDEX].integer_data; // ParameterSize -> ColumnSize
        data->parameters[i]->Nullable = data->storedRows[i][SQLPROCEDURECOLUMNS_NULLABLE_INDEX].smallint_data;

        // For each parameter, need to manipulate the data buffer and C type
        // depending on what the InputOutputType is:
        //
        // SQL_PARAM_INPUT:
        //   No changes required, data should be able to be parsed based on
        //   the C type defined when the parameters were read from JavaScript
        //
        // SQL_PARAM_INPUT_OUTPUT:
        //   Need to preserve the data so that it can be sent in correctly,
        //   but also need to ensure that the buffer is large enough to return
        //   any value returned on the out portion. Also, preserve the bind type
        //   on both the input and output:
        //
        //   e.g. A user sends in a character string for a BIGINT field. Resize
        //        the buffer for the character string to hold any potential
        //        value on the out portion, but keep it bound to SQL_C_CHAR.
        
        // declare buffersize, used for many of the code paths below
        SQLSMALLINT bufferSize = 0;
        switch (parameter->InputOutputType) {
          case SQL_PARAM_INPUT_OUTPUT:
          {
            switch(parameter->ParameterType)
            {
              case SQL_BIGINT: {
                switch(parameter->ValueType)
                {
                  case SQL_C_SBIGINT:
                    // Don't need to do anything, should be bound correctly
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;

                  case SQL_C_CHAR:
                  default:
                    // TODO: Don't just hardcode 21
                    bufferSize = 21;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                }
                break;
              }
              case SQL_BINARY:
              case SQL_VARBINARY:
              case SQL_LONGVARBINARY: {
                switch(parameter->ValueType)
                {
                  case SQL_C_BINARY:
                  default: {
                    bufferSize = parameter->ColumnSize * sizeof(SQLCHAR);
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }
              case SQL_INTEGER: {
                switch(parameter->ValueType)
                {
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 12
                    bufferSize = 12;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }
              case SQL_SMALLINT: {
                switch(parameter->ValueType)
                {
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 7
                    bufferSize = 7;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_TINYINT: {
                switch(parameter->ValueType)
                {
                  case SQL_C_CHAR:
                  default: {
                    parameter->BufferLength = sizeof(SQLCHAR);
                    break;
                  }
                }
                break;
              }

              case SQL_DECIMAL:
              case SQL_NUMERIC: {
                switch(parameter->ValueType)
                {
                  case SQL_C_DOUBLE: {
                    parameter->BufferLength = sizeof(SQLDOUBLE);
                    break;
                  }
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // ColumnSize + sign + decimal + null-terminator
                    bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 3);
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }
              
              case SQL_BIT: {
                switch(parameter->ValueType)
                {
                  case SQL_C_BIT:
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 2
                    bufferSize = 2;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_REAL: {
                switch(parameter->ValueType)
                {
                  case SQL_C_DOUBLE: {
                    parameter->BufferLength = sizeof(SQLDOUBLE);
                    break;
                  }
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 15
                    bufferSize = 15;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_FLOAT: {
                switch(parameter->ValueType)
                {
                  case SQL_C_DOUBLE: {
                    parameter->BufferLength = sizeof(SQLDOUBLE);
                    break;
                  }
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 25
                    bufferSize = 25;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_DOUBLE: {
                switch(parameter->ValueType)
                {
                  case SQL_C_DOUBLE: {
                    parameter->BufferLength = sizeof(SQLDOUBLE);
                    break;
                  }
                  case SQL_C_SBIGINT: {
                    parameter->BufferLength = sizeof(SQLBIGINT);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    // TODO: don't just hardcode 25
                    bufferSize = 25;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_WCHAR:
              case SQL_WVARCHAR:
              case SQL_WLONGVARCHAR: {
                switch(parameter->ValueType)
                {
                  case SQL_C_WCHAR: {
                    bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1);
                    SQLWCHAR *temp = new SQLWCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize * sizeof(SQLWCHAR);
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR) * MAX_UTF8_BYTES;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              case SQL_CHAR:
              case SQL_VARCHAR:
              case SQL_LONGVARCHAR: {
                switch(parameter->ValueType)
                {
                  case SQL_C_CHAR:
                  default: {
                    bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR) * MAX_UTF8_BYTES;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }

              // It is possible that a driver-specific value was returned.
              // If so, just go with whatever C type they bound with with
              // reasonable values.
              default: {
                switch(parameter->ValueType)
                {
                  case SQL_C_BINARY: {
                    bufferSize = parameter->ColumnSize * sizeof(SQLCHAR);
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                  case SQL_C_CHAR:
                  default: {
                    bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLCHAR) * MAX_UTF8_BYTES;
                    SQLCHAR *temp = new SQLCHAR[bufferSize]();
                    memcpy(temp, parameter->ParameterValuePtr, parameter->BufferLength);
                    parameter->ParameterValuePtr = temp;
                    parameter->BufferLength = bufferSize;
                    break;
                  }
                }
                break;
              }
            }
            break;
          }
          case SQL_PARAM_OUTPUT:
          {
            switch(parameter->ParameterType)
            {
              case SQL_DECIMAL:
              case SQL_NUMERIC:
                bufferSize = (data->parameters[i]->ColumnSize + 3) * sizeof(SQLCHAR);
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
                data->parameters[i]->ValueType = SQL_C_SSHORT;
                data->parameters[i]->ParameterValuePtr = new SQLSMALLINT();
                data->parameters[i]->BufferLength = sizeof(SQLSMALLINT);
                data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
                break;

              case SQL_INTEGER:
                data->parameters[i]->ValueType = SQL_C_SLONG;
                data->parameters[i]->ParameterValuePtr = new SQLINTEGER();
                data->parameters[i]->BufferLength = sizeof(SQLINTEGER);
                data->parameters[i]->DecimalDigits = data->storedRows[i][SQLPROCEDURECOLUMNS_DECIMAL_DIGITS_INDEX].smallint_data;
                break;

              case SQL_BIGINT:
                parameter->ValueType = SQL_C_SBIGINT;
                parameter->ParameterValuePtr = new SQLBIGINT();
                parameter->BufferLength = sizeof(SQLBIGINT);
                parameter->isbigint = true;
                break;

              case SQL_BINARY:
              case SQL_VARBINARY:
              case SQL_LONGVARBINARY:
                bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize) * sizeof(SQLCHAR);
                data->parameters[i]->ValueType = SQL_C_BINARY;
                data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize]();
                data->parameters[i]->BufferLength = bufferSize;
                break;

              case SQL_WCHAR:
              case SQL_WVARCHAR:
              case SQL_WLONGVARCHAR:
                bufferSize = (SQLSMALLINT)(data->parameters[i]->ColumnSize + 1) * sizeof(SQLWCHAR);
                data->parameters[i]->ValueType = SQL_C_WCHAR;
                data->parameters[i]->ParameterValuePtr = new SQLWCHAR[bufferSize]();
                data->parameters[i]->BufferLength = bufferSize;
                break;

              case SQL_CHAR:
              case SQL_VARCHAR:
              case SQL_LONGVARCHAR:
              default:
                bufferSize = (SQLSMALLINT)((data->parameters[i]->ColumnSize * MAX_UTF8_BYTES) + 1) * sizeof(SQLCHAR);
                data->parameters[i]->ValueType = SQL_C_CHAR;
                data->parameters[i]->ParameterValuePtr = new SQLCHAR[bufferSize]();
                data->parameters[i]->BufferLength = bufferSize;
                break;
            }
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

      data->sqlReturnCode = prepare_for_fetch(data);
      data->sqlReturnCode = fetch_all_and_store(data);
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
      Napi::Array rows = process_data_for_napi(env, data, napiParameters.Value());

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

  public:
    CallProcedureAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Value napiParameterArray, StatementData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
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

  StatementData *data              = new StatementData();
                 data->henv        = this->hENV;
                 data->hdbc        = this->hDBC;
                 data->fetch_array = this->connectionOptions.fetchArray;
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
    StatementData  *data;

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

      data->sqlReturnCode = prepare_for_fetch(data);
      data->sqlReturnCode = fetch_all_and_store(data);
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

      Napi::Array empty = Napi::Array::New(env);
      Napi::Array rows = process_data_for_napi(env, data, empty);
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

  public:

    TablesAsyncWorker(ODBCConnection *odbcConnectionObject, StatementData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
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
  StatementData* data              = new StatementData();
                 data->henv        = this->hENV;
                 data->hdbc        = this->hDBC;
                 data->fetch_array = this->connectionOptions.fetchArray;
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
    StatementData *data;

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

      data->sqlReturnCode = prepare_for_fetch(data);
      data->sqlReturnCode = fetch_all_and_store(data);
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

      Napi::Array empty = Napi::Array::New(env);
      Napi::Array rows = process_data_for_napi(env, data, empty);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);
      Callback().Call(callbackArguments);
    }

  public:

    ColumnsAsyncWorker(ODBCConnection *odbcConnectionObject, StatementData *data, Napi::Function& callback) : ODBCAsyncWorker(callback),
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

  StatementData* data               = new StatementData();
                 data->henv         = this->hENV;
                 data->hdbc         = this->hDBC;
                 data->fetch_array  = this->connectionOptions.fetchArray;
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

SQLRETURN
prepare_for_fetch
(
  StatementData *data
)
{
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::prepare_for_fetch()\n", data->henv, data->hdbc, data->hSTMT);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::prepare_for_fetch(): Running SQLRowCount(StatementHandle = %p, RowCount = %ld)\n", data->henv, data->hdbc, data->hSTMT, data->hSTMT, data->rowCount);
  data->sqlReturnCode =
  SQLRowCount
  (
    data->hSTMT,    // StatementHandle
    &data->rowCount // RowCountPtr
  );
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    // if SQLRowCount failed, return early with the returnCode
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::prepare_for_fetch(): SQLRowCount FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::prepare_for_fetch(): SQLRowCount passed: SQLRETURN = %d, RowCount = %ld\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode, data->rowCount);

  data->sqlReturnCode =
  bind_buffers
  (
    data
  );

  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    // if BindColumns failed, return early with the returnCode
    return data->sqlReturnCode;
  }

  return data->sqlReturnCode;
}


SQLRETURN
bind_buffers
(
  StatementData *data
)
{
  SQLRETURN return_code;

  return_code =
  SQLNumResultCols
  (
    data->hSTMT,
    &data->column_count
  );

  // Create Columns for the column data to go into
  data->columns       = new Column*[data->column_count]();
  data->bound_columns = new ColumnBuffer[data->column_count]();

  return_code =
  SQLSetStmtAttr
  (
    data->hSTMT,
    SQL_ATTR_ROW_BIND_TYPE,
    SQL_BIND_BY_COLUMN,
    IGNORED_PARAMETER
  );

  return_code =
  SQLSetStmtAttr
  (
    data->hSTMT,
    SQL_ATTR_ROWS_FETCHED_PTR,
    (SQLPOINTER) &data->rows_fetched,
    IGNORED_PARAMETER
  );

  for (int i = 0; i < data->column_count; i++)
  {
    Column *column = new Column();
    column->ColumnName = new SQLTCHAR[256]();

    // TODO: Could just allocate one large SQLLEN buffer that was of size
    // column_count * fetch_size, then just do the pointer arithmetic for it..
    data->bound_columns[i].length_or_indicator_array =
      new SQLLEN[data->fetch_size]();

    // TODO: Change 256 to max column name length
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLDescribeCol(StatementHandle = %p, ColumnNumber = %d, ColumnName = %s, BufferLength = %d, NameLength = %d, DataType = %d, ColumnSize = %lu, DecimalDigits = %d, Nullable = %d)\n", data->henv, data->hdbc, data->hSTMT, data->hSTMT, i + 1, column->ColumnName, 256, column->NameLength, column->DataType, column->ColumnSize, column->DecimalDigits, column->Nullable);

    return_code = 
    SQLDescribeCol
    (
      data->hSTMT,               // StatementHandle
      i + 1,                     // ColumnNumber
      column->ColumnName,        // ColumnName
      256,                       // BufferLength
      &column->NameLength,       // NameLengthPtr
      &column->DataType,         // DataTypePtr
      &column->ColumnSize,       // ColumnSizePtr
      &column->DecimalDigits,    // DecimalDigitsPtr
      &column->Nullable          // NullablePtr
    );
    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLDescribeCol FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
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

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLDescribeCol passed: ColumnName = %s, NameLength = %d, DataType = %d, ColumnSize = %lu, DecimalDigits = %d, Nullable = %d\n", data->henv, data->hdbc, data->hSTMT, column->ColumnName, column->NameLength, column->DataType, column->ColumnSize, column->DecimalDigits, column->Nullable);
    // bind depending on the column
    switch(column->DataType) {

      case SQL_REAL:
      case SQL_DECIMAL:
      case SQL_NUMERIC:
      {
        size_t character_count = column->ColumnSize + 2;
        column->buffer_size = character_count * sizeof(SQLCHAR);
        column->bind_type = SQL_C_CHAR;
        data->bound_columns[i].buffer =
          new SQLCHAR[character_count * data->fetch_size]();
        break;
      }

      case SQL_FLOAT:
      case SQL_DOUBLE:
      {
        column->buffer_size = sizeof(SQLDOUBLE);
        column->bind_type = SQL_C_DOUBLE;
        data->bound_columns[i].buffer =
          new SQLDOUBLE[data->fetch_size]();
        break;
      }

      case SQL_TINYINT:
      {
        column->buffer_size = sizeof(SQLCHAR);
        column->bind_type = SQL_C_UTINYINT;
        data->bound_columns[i].buffer =
          new SQLCHAR[data->fetch_size]();
        break;
      }

      case SQL_SMALLINT:
      {
        column->buffer_size = sizeof(SQLSMALLINT);
        column->bind_type = SQL_C_SHORT;
        data->bound_columns[i].buffer =
          new SQLSMALLINT[data->fetch_size]();
        break;
      }

      case SQL_INTEGER:
      {
        column->buffer_size = sizeof(SQLINTEGER);
        column->bind_type = SQL_C_SLONG;
        data->bound_columns[i].buffer =
          new SQLINTEGER[data->fetch_size]();
        break;
      }

      case SQL_BIGINT:
      {
        column->buffer_size = sizeof(SQLUBIGINT);
        column->bind_type = SQL_C_UBIGINT;
        data->bound_columns[i].buffer =
          new SQLBIGINT[data->fetch_size]();
        break;
      }

      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
      {
        column->buffer_size = column->ColumnSize;
        column->bind_type = SQL_C_BINARY;
        data->bound_columns[i].buffer =
          new SQLCHAR[column->buffer_size * data->fetch_size]();
        break;
      }

      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
      {
        size_t character_count = column->ColumnSize + 1;
        column->buffer_size = character_count * sizeof(SQLWCHAR);
        column->bind_type = SQL_C_WCHAR;
        data->bound_columns[i].buffer =
          new SQLWCHAR[character_count * data->fetch_size]();
        break;
      }

      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      default:
      {
        size_t character_count = column->ColumnSize * MAX_UTF8_BYTES + 1;
        column->buffer_size = character_count * sizeof(SQLCHAR);
        column->bind_type = SQL_C_CHAR;
        data->bound_columns[i].buffer =
          new SQLCHAR[character_count * data->fetch_size]();
        break;
      }
    }

    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): Running SQLBindCol(StatementHandle = %p, ColumnNumber = %d, TargetType = %d, TargetValuePtr = %p, BufferLength = %ld, StrLen_or_Ind = %ld\n", data->henv, data->hdbc, data->hSTMT, data->hSTMT, i + 1, column->bind_type, data->bound_columns[i].buffer, column->buffer_size, column->StrLen_or_IndPtr);
    // SQLBindCol binds application data buffers to columns in the result set.
    return_code =
    SQLBindCol
    (
      data->hSTMT,                                       // StatementHandle
      i + 1,                                             // ColumnNumber
      column->bind_type,                                 // TargetType
      data->bound_columns[i].buffer,                     // TargetValuePtr
      column->buffer_size,                               // BufferLength
      data->bound_columns[i].length_or_indicator_array   // StrLen_or_Ind
    );

    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLBindCol FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
      return data->sqlReturnCode;
    }
    data->columns[i] = column;
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCConnection::BindColumns(): SQLBindCol succeeded: StrLeng_or_IndPtr = %ld\n", data->henv, data->hdbc, data->hSTMT, column->StrLen_or_IndPtr);
  }
  return return_code;
}

SQLRETURN
fetch_and_store
(
  StatementData *data
)
{
  data->sqlReturnCode =
  SQLFetch
  (
    data->hSTMT
  );

  if (SQL_SUCCEEDED(data->sqlReturnCode))
  {
    // iterate through all of the rows fetched (but not the fetch size)
    for (size_t row_index = 0; row_index < data->rows_fetched; row_index++)
    {
      // Copy the data over if the row status array indicates success
      if
      (
        data->row_status_array[row_index] == SQL_ROW_SUCCESS ||
        data->row_status_array[row_index] == SQL_ROW_SUCCESS_WITH_INFO
      )
      {
        ColumnData *row = new ColumnData[data->column_count]();

        // Iterate over each column, putting the data in the row object
        for (int column_index = 0; column_index < data->column_count; column_index++)
        {
          if (data->bound_columns[column_index].length_or_indicator_array[row_index] == SQL_NULL_DATA) {
            row[column_index].size = SQL_NULL_DATA;
          } else {
            switch (data->columns[column_index]->bind_type) {

              case SQL_C_DOUBLE:
                row[column_index].double_data =
                  ((SQLDOUBLE *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_UTINYINT:
                row[column_index].tinyint_data =
                  ((SQLCHAR *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_SSHORT:
              case SQL_C_SHORT:
                row[column_index].smallint_data =
                  ((SQLSMALLINT *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_USHORT:
                row[column_index].usmallint_data =
                  ((SQLUSMALLINT *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_SLONG:
                row[column_index].integer_data =
                  ((SQLINTEGER *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_UBIGINT:
                row[column_index].ubigint_data =
                  ((SQLBIGINT *)(data->bound_columns[column_index].buffer))[row_index];
                break;

              case SQL_C_BINARY:
              {
                row[column_index].char_data = new SQLCHAR[row[column_index].size]();
                memcpy(
                  row[column_index].char_data,
                  (SQLCHAR *)data->bound_columns[column_index].buffer + row_index * data->columns[column_index]->buffer_size,
                  row[column_index].size
                );
                break;
              }

              case SQL_C_WCHAR:
              {
                SQLWCHAR *memory_start = (SQLWCHAR *)data->bound_columns[column_index].buffer + (row_index * data->columns[column_index]->ColumnSize + 1);
                row[column_index].size = strlen16((const char16_t *)memory_start);
                row[column_index].wchar_data = new SQLWCHAR[row[column_index].size + 1]();
                memcpy
                (
                  row[column_index].wchar_data,
                  memory_start,
                  row[column_index].size * sizeof(SQLWCHAR)
                );
                break;
              }

              case SQL_C_CHAR:
              default:
              {
                SQLCHAR *memory_start = (SQLCHAR *)data->bound_columns[column_index].buffer + (row_index * data->columns[column_index]->buffer_size);
                row[column_index].size = strlen((const char *)memory_start);
                // Although fields going from SQL_C_CHAR to Napi::String use
                // row[column_index].size, NUMERIC data uses atof() which requires
                // a null terminator. Need to add an aditional byte.
                row[column_index].char_data = new SQLCHAR[row[column_index].size + 1]();
                memcpy
                (
                  row[column_index].char_data,
                  memory_start,
                  row[column_index].size
                );
                break;
              }

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
          row[column_index].bind_type = data->columns[column_index]->bind_type;
          }
        }
        data->storedRows.push_back(row);
      }
    }
  }

  // If SQL_SUCCEEDED failed and return code isn't SQL_NO_DATA, there is an error
  if(!SQL_SUCCEEDED(data->sqlReturnCode) && data->sqlReturnCode != SQL_NO_DATA) {
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_and_store(): SQLFetch FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_and_store(): SQLFetch succeeded: Stored %lu rows of data, each with %d columns\n", data->henv, data->hdbc, data->hSTMT, data->storedRows.size(), data->column_count);

  // if the last row status is SQL_ROW_NOROW, or the last call to SQLFetch
  // returned SQL_NO_DATA, we have reached the end of the result set. Set
  // result_set_end_reached to true so that SQLFetch doesn't get called again.
  // SQLFetch again (sort of a short-circuit)
  if (data->row_status_array[data->fetch_size - 1] == SQL_ROW_NOROW ||
      data->sqlReturnCode == SQL_NO_DATA)
  {
    data->result_set_end_reached = true;
  }

  return data->sqlReturnCode;
}

SQLRETURN
fetch_all_and_store
(
  StatementData *data
)
{
  do {
    data->sqlReturnCode = fetch_and_store(data);
  } while (SQL_SUCCEEDED(data->sqlReturnCode));

  // If SQL_SUCCEEDED failed and return code isn't SQL_NO_DATA, there is an error
  if(data->sqlReturnCode != SQL_NO_DATA) {
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_all_and_store(): SQLFetch FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_all_and_store(): SQLFetch succeeded: Stored %lu rows of data, each with %d columns\n", data->henv, data->hdbc, data->hSTMT, data->storedRows.size(), data->column_count);

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_all_and_store(): Running SQLCloseCursor(StatementHandle = %p) (Running multiple times)\n", data->henv, data->hdbc, data->hSTMT, data->hSTMT);
  // will return either SQL_ERROR or SQL_NO_DATA
  data->sqlReturnCode = SQLCloseCursor(data->hSTMT);
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_all_and_store(): SQLCloseCursor FAILED: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
    return data->sqlReturnCode;
  }
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] fetch_all_and_store(): SQLCloseCursor succeeded\n", data->henv, data->hdbc, data->hSTMT);
  return data->sqlReturnCode;
}

// All of data has been loaded into data->storedRows. Have to take the data
// stored in there and convert it it into JavaScript to be given to the
// Node.js runtime.
Napi::Array process_data_for_napi(Napi::Env env, StatementData *data, Napi::Array napiParameters) {

  Column **columns = data->columns;
  SQLSMALLINT columnCount = data->column_count;

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
  if (napiParameters.IsEmpty()) {
    rows.Set(Napi::String::New(env, PARAMETERS), env.Undefined());
  } else {
    rows.Set(Napi::String::New(env, PARAMETERS), napiParameters);
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

    if (data->fetch_array == true) {
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
              case SQL_C_TINYINT:
              case SQL_C_UTINYINT:
              case SQL_C_STINYINT:
                value  = Napi::Number::New(env, storedRow[j].tinyint_data);
                break;
              case SQL_C_SHORT:
              case SQL_C_USHORT:
              case SQL_C_SSHORT:
                value  = Napi::Number::New(env, storedRow[j].smallint_data);
                break;
              case SQL_C_LONG:
              case SQL_C_ULONG:
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
#if NAPI_VERSION > 5
                value = Napi::BigInt::New(env, (int64_t)storedRow[j].ubigint_data);
#else
                value = Napi::Number::New(env, (int64_t)storedRow[j].ubigint_data);
#endif
                break;
              default:
                value = Napi::String::New(env, (char*)storedRow[j].char_data);
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
      // TODO: here
      if (data->fetch_array == true) {
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