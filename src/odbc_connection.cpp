/*
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

#include <napi.h>
#include <uv.h>
#include <time.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"

Napi::FunctionReference ODBCConnection::constructor;

Napi::String ODBCConnection::OPTION_SQL;
Napi::String ODBCConnection::OPTION_PARAMS;
Napi::String ODBCConnection::OPTION_NORESULTS;

Napi::Object ODBCConnection::Init(Napi::Env env, Napi::Object exports) {

  DEBUG_PRINTF("ODBCConnection::Init\n");
  Napi::HandleScope scope(env);

  OPTION_SQL = Napi::String::New(env, "sql");
  OPTION_PARAMS = Napi::String::New(env, "params");
  OPTION_NORESULTS = Napi::String::New(env, "noResults");

  Napi::Function constructorFunction = DefineClass(env, "ODBCConnection", {

    InstanceMethod("close", &ODBCConnection::Close),
    InstanceMethod("closeSync", &ODBCConnection::CloseSync),

    InstanceMethod("createStatement", &ODBCConnection::CreateStatement),
    InstanceMethod("createStatementSync", &ODBCConnection::CreateStatementSync),

    InstanceMethod("query", &ODBCConnection::Query),
    InstanceMethod("querySync", &ODBCConnection::QuerySync),

    InstanceMethod("beginTransaction", &ODBCConnection::BeginTransaction),
    InstanceMethod("beginTransactionSync", &ODBCConnection::BeginTransactionSync),

    InstanceMethod("endTransaction", &ODBCConnection::EndTransaction),
    InstanceMethod("endTransactionSync", &ODBCConnection::EndTransactionSync),

    InstanceMethod("getInfo", &ODBCConnection::GetInfo),
    InstanceMethod("getInfoSync", &ODBCConnection::GetInfoSync),

    InstanceMethod("columns", &ODBCConnection::Columns),
    InstanceMethod("columnsSync", &ODBCConnection::ColumnsSync),

    InstanceMethod("tables", &ODBCConnection::Tables),
    InstanceMethod("tablesSync", &ODBCConnection::TablesSync),

    InstanceAccessor("connected", &ODBCConnection::ConnectedGetter, nullptr),
    InstanceAccessor("connectTimeout", &ODBCConnection::ConnectTimeoutGetter, &ODBCConnection::ConnectTimeoutSetter),
    InstanceAccessor("loginTimeout", &ODBCConnection::LoginTimeoutGetter, &ODBCConnection::LoginTimeoutSetter)
  });

  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  return exports;
}

ODBCConnection::ODBCConnection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCConnection>(info) {

  this->hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());

  this->connectionTimeout = 0;
  this->loginTimeout = 5;
}

ODBCConnection::~ODBCConnection() {

  DEBUG_PRINTF("ODBCConnection::~ODBCConnection\n");
  SQLRETURN sqlReturnCode;

  this->Free(&sqlReturnCode);
}

void ODBCConnection::Free(SQLRETURN *sqlReturnCode) {

  DEBUG_PRINTF("ODBCConnection::Free\n");

  uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (hDBC) {
      SQLDisconnect(hDBC);
      SQLFreeHandle(SQL_HANDLE_DBC, hDBC);
      hDBC = NULL;
    }
    
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  return;
}

Napi::Value ODBCConnection::ConnectedGetter(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Boolean::New(env, this->isConnected ? true : false);
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
class CloseAsyncWorker : public Napi::AsyncWorker {

  public:
    CloseAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::Execute\n");

      odbcConnectionObject->Free(&sqlReturnCode);

      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        SetError("ERROR");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::CloseAsyncWorker"));

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
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

  DEBUG_PRINTF("ODBCConnection::Close\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::CloseSync
 * 
 *    Description: Closes the connection synchronously.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, closeSync()
 *        takes now arguments
 *    Return:
 *      Napi::Value:
 *        A Boolean that is true if the connection was correctly closed.
 */
Napi::Value ODBCConnection::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode;

  this->Free(&sqlReturnCode);

  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->hDBC)).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);

  } else {
    return Napi::Boolean::New(env, true);
  }
}


/******************************************************************************
 ***************************** CREATE STATEMENT *******************************
 *****************************************************************************/

// CreateStatementAsyncWorker, used by CreateStatement function (see below)
class CreateStatementAsyncWorker : public Napi::AsyncWorker {

  public:
    CreateStatementAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CreateStatementAsyncWorker() {}


    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker:Execute - hDBC=%X hDBC=%X\n",
       odbcConnectionObject->hENV,
       odbcConnectionObject->hDBC,
      );

      uv_mutex_lock(&ODBC::g_odbcMutex);
      sqlReturnCode = SQLAllocHandle( SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &hSTMT);
      uv_mutex_unlock(&ODBC::g_odbcMutex);


      if (SQL_SUCCEEDED(sqlReturnCode)) {
        return;
      } else {
        SetError("ERROR");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker::OnOK - hDBC=%X hDBC=%X hSTMT=%X\n",
        odbcConnectionObject->hENV,
        odbcConnectionObject->hDBC,
        hSTMT
      );

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // arguments for the ODBCStatement constructor
      std::vector<napi_value> statementArguments;
      statementArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->hENV)));
      statementArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->hDBC)));
      statementArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));
      
      // create a new ODBCStatement object as a Napi::Value
      Napi::Value statementObject = ODBCStatement::constructor.New(statementArguments);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());      // callbackArguments[0]
      callbackArguments.push_back(statementObject); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker::OnError - hDBC=%X hDBC=%X hSTMT=%X\n",
        odbcConnectionObject->hENV,
        odbcConnectionObject->hDBC,
        hSTMT
      );

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::CreateStatementAsyncWorker"));
      callbackArguments.push_back(env.Null()); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
    HSTMT hSTMT;
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

  DEBUG_PRINTF("ODBCConnection::CreateStatement\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CreateStatementAsyncWorker *worker = new CreateStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::CreateStatementSync
 * 
 *    Description: Create an ODBCStatement to manually prepare, bind, and
 *                 execute.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment.
 *        createStatementSync takes no parameters
 * 
 *    Return:
 *      Napi::Value:
 *        ODBCStatement, the object with the new SQLHSTMT assigned
 */
Napi::Value ODBCConnection::CreateStatementSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLHSTMT hSTMT;

  uv_mutex_lock(&ODBC::g_odbcMutex);
      
  //allocate a new statment handle
  SQLAllocHandle(SQL_HANDLE_STMT, this->hDBC, &hSTMT);

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  std::vector<napi_value> statementArguments;
  statementArguments.push_back(Napi::External<HENV>::New(env, &(this->hENV)));
  statementArguments.push_back(Napi::External<HDBC>::New(env, &(this->hDBC)));
  statementArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));
  
  // create a new ODBCStatement object as a Napi::Value
  return ODBCStatement::constructor.New(statementArguments);;
}

/******************************************************************************
 ********************************** QUERY *************************************
 *****************************************************************************/

// QueryAsyncWorker, used by Query function (see below)
class QueryAsyncWorker : public Napi::AsyncWorker {

  public:
    QueryAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~QueryAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("\nODBCConnection::QueryAsyncWorke::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
      
      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      data->sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &(data->hSTMT));
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        return;
      }


      // querying with parameters, need to prepare, bind, execute
      if (data->paramCount > 0) {

        // binds all parameters to the query
        data->sqlReturnCode = SQLPrepare(
          data->hSTMT,
          data->sql, 
          SQL_NTS
        );

        ODBC::BindParameters(data);

        data->sqlReturnCode = SQLExecute(data->hSTMT);

      } 
      // querying without parameters, can just execdirect
      else {
        data->sqlReturnCode = SQLExecDirect(
          data->hSTMT,
          data->sql,
          SQL_NTS
        );
      }

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError("ERROR");
        return;
      } else {
        SQLRowCount(data->hSTMT, &data->rowCount);
        ODBC::BindColumns(data);
        ODBC::FetchAll(data);
        return;
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
      callbackArguments.push_back(env.Null());

      // return results object
      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    QueryData      *data;
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

  DEBUG_PRINTF("ODBCConnection::Query\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String sql = info[0].ToString();

  QueryData *data = new QueryData;
  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

  // check if parameters were passed or not
  if (info.Length() == 3 && info[1].IsArray()) {
    Napi::Array parameterArray = info[1].As<Napi::Array>();
    data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));
  } else {
    data->params = 0;
  }

  QueryAsyncWorker *worker;

  Napi::Function callback = info[info.Length() - 1].As<Napi::Function>();
  worker = new QueryAsyncWorker(this, data, callback);
  worker->Queue();
  return env.Undefined();
}

/*
 *  ODBCConnection::QuerySync
 * 
 *    Description: Returns the info requested from the connection.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'querySync'.
 *   
 *        info[0]: String: the SQL string to execute
 *        info[1]: Array: A list of parameters to bind to the statement
 * 
 *    Return:
 *      Napi::Value:
 *        String: The result of the info call.
 */
Napi::Value ODBCConnection::QuerySync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::QuerySync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData;

  Napi::String sql = info[0].ToString();

  if (info.Length() == 2 && info[1].IsArray()) {
    Napi::Array parameterArray = info[1].As<Napi::Array>();
    data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));
  }

  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

  DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);

  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new statment handle
  data->sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, this->hDBC, &(data->hSTMT));

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {

    if (data->paramCount > 0) {
      // binds all parameters to the query
      ODBC::BindParameters(data);
    }

    // execute the query directly
    data->sqlReturnCode = SQLExecDirect(
      data->hSTMT,
      data->sql,
      SQL_NTS
    );

    if (SQL_SUCCEEDED(data->sqlReturnCode)) {

      ODBC::BindColumns(data);
      ODBC::FetchAll(data);

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);
      return rows;
    } else {
      Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QuerySync")).ThrowAsJavaScriptException();
      return env.Null();
    }
  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QuerySync")).ThrowAsJavaScriptException();
    return env.Null();
  }
}

/******************************************************************************
 ******************************** GET INFO ************************************
 *****************************************************************************/

// GetInfoAsyncWorker, used by GetInfo function (see below)
class GetInfoAsyncWorker : public Napi::AsyncWorker {

  public:
    GetInfoAsyncWorker(ODBCConnection *odbcConnectionObject, SQLUSMALLINT infoType, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      infoType(infoType) {}

    ~GetInfoAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::GetInfoAsyncWorker:Execute");

      switch (infoType) {
        case SQL_USER_NAME:

          sqlReturnCode = SQLGetInfo(odbcConnectionObject->hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

          if (SQL_SUCCEEDED(sqlReturnCode)) {
           return;
          } else {
            SetError("Error");
          }

        default:
          SetError("Error");
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      #ifdef UNICODE
        callbackArguments.push_back(Napi::String::New(env, (const char16_t *)userName));
      #else
        callbackArguments.push_back(Napi::String::New(env, (const char *) userName));
      #endif

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::GetInfoAsyncWorker"));
      
      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLUSMALLINT infoType;
    SQLTCHAR userName[255];
    SQLSMALLINT userNameLength;
    SQLRETURN sqlReturnCode;
};

/*
 *  ODBCConnection::GetInfo
 * 
 *    Description: Returns the info requested from the connection.
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
Napi::Value ODBCConnection::GetInfo(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::GetInfo\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if ( !info[0].IsNumber() ) {
    Napi::TypeError::New(env, "ODBCConnection::GetInfo(): Argument 0 must be a Number.").ThrowAsJavaScriptException();
    return env.Null();
  }

  SQLUSMALLINT infoType = info[0].As<Napi::Number>().Int32Value();
  Napi::Function callback = info[0].As<Napi::Function>();

  GetInfoAsyncWorker *worker = new GetInfoAsyncWorker(this, infoType, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::GetInfoSync
 * 
 *    Description: Returns the info requested from the connection.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'getInfoSync'.
 *   
 *        info[0]: Number: option
 * 
 *    Return:
 *      Napi::Value:
 *        String: The result of the info call.
 */
Napi::Value ODBCConnection::GetInfoSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCConnection::GetInfoSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 1) {
    Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): Requires 1 Argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if ( !info[0].IsNumber() ) {
    Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): Argument 0 must be a Number.").ThrowAsJavaScriptException();
    return env.Null();
  }

  SQLUSMALLINT infoType = info[0].ToNumber().Int32Value();

  switch (infoType) {
    case SQL_USER_NAME:
      SQLRETURN sqlReturnCode;
      SQLTCHAR userName[255];
      SQLSMALLINT userNameLength;

      sqlReturnCode = SQLGetInfo(this->hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        #ifdef UNICODE
          return Napi::String::New(env, (const char16_t*)userName);
        #else
          return Napi::String::New(env, (const char*) userName);
        #endif
      } else {
         Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): The only supported Argument is SQL_USER_NAME.").ThrowAsJavaScriptException();
        return env.Null();
      }

    default:
      Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): The only supported Argument is SQL_USER_NAME.").ThrowAsJavaScriptException();
      return env.Null();
  }
}

/******************************************************************************
 ********************************** TABLES ************************************
 *****************************************************************************/

// TablesAsyncWorker, used by Tables function (see below)
class TablesAsyncWorker : public Napi::AsyncWorker {

  public:
    TablesAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~TablesAsyncWorker() {}

    void Execute() {

      uv_mutex_lock(&ODBC::g_odbcMutex);
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT );     
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      data->sqlReturnCode = SQLTables( 
        data->hSTMT, 
        data->catalog, SQL_NTS, 
        data->schema, SQL_NTS, 
        data->table, SQL_NTS, 
        data->type, SQL_NTS
      );

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError("ERROR");
      } else {
        ODBC::BindColumns(data);
        ODBC::FetchAll(data);
        return;
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i, \n", data->sqlReturnCode, );
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::TablesAsyncWorker"));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    QueryData *data;
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
    Napi::Error::New(env, "tables() function takes 5 arguments.").ThrowAsJavaScriptException();
  }

  Napi::Function callback;
  QueryData* data = new QueryData();

  // Napi doesn't have LowMemoryNotification like NAN did. Throw standard error.
  if (!data) {
    Napi::Error::New(env, "Could not allocate enough memory to run query.").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::Error::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::Error::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::Error::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::Error::New(env, "tables: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[4].IsFunction()) { callback = info[4].As<Napi::Function>(); }
  else {
    Napi::Error::New(env, "tables: fifth argument must be a function").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  TablesAsyncWorker *worker = new TablesAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::TablesSync
 * 
 *    Description: Returns the list of table, catalog, or schema names, and
 *                 table types, stored in a specific data source.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'tablesSync'.
 *   
 *        info[0]: String: catalog
 *        info[1]: String: schema
 *        info[2]: String: table
 *        info[3]: String: type
 * 
 *    Return:
 *      Napi::Value:
 *        The ODBCResult object containing table info
 */
Napi::Value ODBCConnection::TablesSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 4) {
    Napi::Error::New(env, "tablesSync() function takes 4 arguments.").ThrowAsJavaScriptException();
  }

  QueryData* data = new QueryData();

  // Napi doesn't have LowMemoryNotification like NAN did. Throw standard error.
  if (!data) {
    Napi::Error::New(env, "Could not allocate enough memory to run query.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::Error::New(env, "tablesSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::Error::New(env, "tablesSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::Error::New(env, "tablesSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::Error::New(env, "tablesSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  uv_mutex_lock(&ODBC::g_odbcMutex);
  SQLAllocHandle(SQL_HANDLE_STMT, this->hDBC, &data->hSTMT );
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  data->sqlReturnCode = SQLTables( 
    data->hSTMT, 
    data->catalog, SQL_NTS, 
    data->schema, SQL_NTS, 
    data->table, SQL_NTS, 
    data->type, SQL_NTS
  );

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {

    ODBC::RetrieveData(data);

    if (SQL_SUCCEEDED(data->sqlReturnCode)) {
      
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);
      delete data;
      return rows;

    } else {
      Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
      delete data;
      return env.Null();
    }

  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }
}


/******************************************************************************
 ********************************* COLUMNS ************************************
 *****************************************************************************/

// ColumnsAsyncWorker, used by Columns function (see below)
class ColumnsAsyncWorker : public Napi::AsyncWorker {

  public:
    ColumnsAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~ColumnsAsyncWorker() {}

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT );
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      data->sqlReturnCode = SQLColumns( 
        data->hSTMT, 
        data->catalog, SQL_NTS, 
        data->schema, SQL_NTS, 
        data->table, SQL_NTS, 
        data->column, SQL_NTS
      );

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {

        // manipulates the fields of QueryData object, which can then be fetched
        ODBC::BindColumns(data);
        ODBC::FetchAll(data);

        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          SetError("ERROR");
        }

      } else {
        SetError("ERROR");
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      // return results object
      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::ColumnsAsyncWorker"));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    QueryData *data;
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
 *        info[3]: String: type
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
    Napi::Error::New(env, "columns: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::Error::New(env, "columns: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::Error::New(env, "columns: first argument must be a string or null").ThrowAsJavaScriptException();
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

/*
 *  ODBCConnection::ColumnsSync
 * 
 *    Description: Returns the list of column names in specified tables.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'columnSync'.
 * 
 *    Return:
 *      Napi::Value:
 *        Otherwise, an ODBCResult object holding the
 *        hSTMT.
 */
Napi::Value ODBCConnection::ColumnsSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData* data = new QueryData();

  // Napi doesn't have LowMemoryNotification like NAN did. Throw standard error.
  if (!data) {
    Napi::Error::New(env, "Could not allocate enough memory to run query.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    data->catalog = ODBC::NapiStringToSQLTCHAR(info[0].ToString());
  } else if (!info[0].IsNull()) {
    Napi::Error::New(env, "columnsSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[1].IsString()) {
    data->schema = ODBC::NapiStringToSQLTCHAR(info[1].ToString());
  } else if (!info[1].IsNull()) {
    Napi::Error::New(env, "columnsSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[2].IsString()) {
    data->table = ODBC::NapiStringToSQLTCHAR(info[2].ToString());
  } else if (!info[2].IsNull()) {
    Napi::Error::New(env, "columnsSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  if (info[3].IsString()) {
    data->type = ODBC::NapiStringToSQLTCHAR(info[3].ToString());
  } else if (!info[3].IsNull()) {
    Napi::Error::New(env, "columnsSync: first argument must be a string or null").ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }

  uv_mutex_lock(&ODBC::g_odbcMutex);   
  SQLAllocHandle(SQL_HANDLE_STMT, this->hDBC, &data->hSTMT );
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  data->sqlReturnCode = SQLColumns( 
    data->hSTMT, 
    data->catalog, SQL_NTS, 
    data->schema, SQL_NTS, 
    data->table, SQL_NTS, 
    data->column, SQL_NTS
  );

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {

    ODBC::RetrieveData(data);

    if (SQL_SUCCEEDED(data->sqlReturnCode)) {

      Napi::Array rows = ODBC::ProcessDataForNapi(env, data);
      delete data;
      return rows;

    } else {
      Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
      delete data;
      return env.Null();
    }

  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
    delete data;
    return env.Null();
  }
}

/******************************************************************************
 **************************** BEGIN TRANSACTION *******************************
 *****************************************************************************/

// BeginTransactionAsyncWorker, used by EndTransaction function (see below)
class BeginTransactionAsyncWorker : public Napi::AsyncWorker {

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;

  public:
    BeginTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~BeginTransactionAsyncWorker() {}

    void Execute() {

        DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::Execute\n");
        
        //set the connection manual commits
        sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_NTS);

        if (SQL_SUCCEEDED(sqlReturnCode)) {
          return;
        } else {
          SetError("Error");
        }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::BeginTransactionAsyncWorker"));

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

  DEBUG_PRINTF("ODBCConnection::BeginTransaction\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback;

  if (info[0].IsFunction()) { callback = info[4].As<Napi::Function>(); }
  else { Napi::Error::New(env, "beginTransaction: first argument must be a function").ThrowAsJavaScriptException(); }

  BeginTransactionAsyncWorker *worker = new BeginTransactionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::BeginTransactionSync
 * 
 *    Description: Begin a transaction by turning off SQL_ATTR_AUTOCOMMIT in
 *                 an AsyncWorker. Transaction is commited or rolledback in
 *                 EndTransaction or EndTransactionSync.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'beginTransactionSync'.
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, indicates whether the transaction was successfully started
 */
Napi::Value ODBCConnection::BeginTransactionSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode;

  //set the connection manual commits
  sqlReturnCode = SQLSetConnectAttr(
    this->hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_OFF,
    SQL_NTS);
  
  if (SQL_SUCCEEDED(sqlReturnCode)) {
    return Napi::Boolean::New(env, true);

  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->hDBC)).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
}

/******************************************************************************
 ***************************** END TRANSACTION ********************************
 *****************************************************************************/

 // EndTransactionAsyncWorker, used by EndTransaction function (see below)
class EndTransactionAsyncWorker : public Napi::AsyncWorker {

  private:
    ODBCConnection *odbcConnectionObject;
    SQLSMALLINT completionType;
    SQLRETURN sqlReturnCode;

  public:
    EndTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, SQLSMALLINT completionType, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      completionType(completionType) {}

    ~EndTransactionAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::Execute\n");
      
      //Call SQLEndTran
      sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, completionType);
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {

        //Reset the connection back to autocommit
        sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
        
        if (SQL_SUCCEEDED(sqlReturnCode)) {
          return;
        } else {
          SetError("ERROR");
        }
      } else {
        SetError("ERROR");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;
      
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::EndTransactionAsyncWorker"));

      Callback().Call(callbackArguments);
    }
};

/*
 *  ODBCConnection::EndTransaction (Async)
 * 
 *    Description: Ends a transaction by calling SQLEndTran on the connection
 *                 in an AsyncWorker.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransaction'.
 *   
 *        info[0]: Boolean: whether to rollback (true) or commit (false)
 *        info[1]: Function: callback function:
 *            function(error)
 *              error: An error object if the transaction wasn't ended, or
 *                     null if operation was successful.
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, indicates whether the transaction was successfully ended
 */
Napi::Value ODBCConnection::EndTransaction(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::EndTransaction\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Boolean rollback;
  Napi::Function callback;

  if (info[0].IsBoolean()) { rollback = info[0].ToBoolean(); }
  else { Napi::Error::New(env, "endTransaction: first argument must be a boolean").ThrowAsJavaScriptException(); }

  if (info[1].IsFunction()) { callback = info[1].As<Napi::Function>(); }
  else { Napi::Error::New(env, "endTransaction: second argument must be a function").ThrowAsJavaScriptException(); }

  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;

  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, completionType, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::EndTransactionSync
 * 
 *    Description: Ends a transaction by calling SQLEndTran on the connection.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransactionSync'.
 *   
 *        info[0]: Boolean: whether to rollback (true) or commit (false)
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, indicates whether the transaction was successfully ended
 */
Napi::Value ODBCConnection::EndTransactionSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Boolean rollback ;

  if (info[0].IsBoolean()) { rollback = info[0].ToBoolean(); }
  else { Napi::Error::New(env, "endTransactionSync: first argument must be a boolean").ThrowAsJavaScriptException(); }

  SQLRETURN sqlReturnCode;
  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;

  //Call SQLEndTran
  sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, this->hDBC, completionType);
  
  if (SQL_SUCCEEDED(sqlReturnCode)) {

    //Reset the connection back to autocommit
    sqlReturnCode = SQLSetConnectAttr(this->hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
    
    if (SQL_SUCCEEDED(sqlReturnCode)) {
      return Napi::Boolean::New(env, true);

    } else {
      Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->hDBC)).ThrowAsJavaScriptException();
      return Napi::Boolean::New(env, false);
    }
  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->hDBC)).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
}