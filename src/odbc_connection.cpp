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

#include <string.h>
#include <napi.h>
#include <uv.h>
//#include <node_version.h>
#include <time.h>
#include <iostream>
#include <string>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

using namespace Napi;

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
    InstanceMethod("open", &ODBCConnection::Open),
    InstanceMethod("openSync", &ODBCConnection::OpenSync),
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

    InstanceMethod("getInfoSync", &ODBCConnection::GetInfoSync),

    InstanceMethod("columns", &ODBCConnection::Columns),
    InstanceMethod("tables", &ODBCConnection::Tables),

    InstanceAccessor("connected", &ODBCConnection::ConnectedGetter, &ODBCConnection::ConnectedSetter),
    InstanceAccessor("connectTimeout", &ODBCConnection::ConnectTimeoutGetter, &ODBCConnection::ConnectTimeoutSetter),
    InstanceAccessor("loginTimeout", &ODBCConnection::LoginTimeoutGetter, &ODBCConnection::LoginTimeoutSetter)
  });

  // // Constructor Template
  // constructor->SetClassName(Napi::String::New(env, "ODBCConnection"));

  // // Reserve space for one Handle<Value>
  // Local<ObjectTemplate> instance_template = constructor->InstanceTemplate();
  
  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCConnection", constructorFunction);

  return exports;
}

ODBCConnection::ODBCConnection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCConnection>(info) {}

ODBCConnection::~ODBCConnection() {
  DEBUG_PRINTF("ODBCConnection::~ODBCConnection\n");
  this->Free();
}

void ODBCConnection::Free() {
  DEBUG_PRINTF("ODBCConnection::Free\n");
  if (m_hDBC) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (m_hDBC) {
      SQLDisconnect(m_hDBC);
      SQLFreeHandle(SQL_HANDLE_DBC, m_hDBC);
      m_hDBC = NULL;
    }
    
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

/*
 * New
 */

Napi::Value ODBCConnection::New(const Napi::CallbackInfo& info) {
  // DEBUG_PRINTF("ODBCConnection::New\n");

  // Napi::Env env = info.Env();
  // Napi::HandleScope scope(env);
  
  // REQ_EXT_ARG(0, js_henv);
  // REQ_EXT_ARG(1, js_hdbc);
  
  // HENV hENV = static_cast<HENV>(js_henv->Value());
  // HDBC hDBC = static_cast<HDBC>(js_hdbc->Value());
  
  // ODBCConnection* conn = new ODBCConnection(hENV, hDBC);
  
  // conn->Wrap(info.Holder());
  
  // //set default connectTimeout to 0 seconds
  // conn->connectTimeout = 0;
  // //set default loginTimeout to 5 seconds
  // conn->loginTimeout = 5;

  // return info.Holder();
}

Napi::Value ODBCConnection::ConnectedGetter(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Boolean::New(env, this->connected ? true : false);
}

void ODBCConnection::ConnectedSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO: Throw a javascript error. Can't set this yourself!
}

Napi::Value ODBCConnection::ConnectTimeoutGetter(const Napi::CallbackInfo& info) {
  
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Number::New(env, this->connectTimeout);
}

void ODBCConnection::ConnectTimeoutSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (value.IsNumber()) {
    this->connectTimeout = value.As<Napi::Number>().Uint32Value();
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

/*
 * Open
 * 
 */

class OpenAsyncWorker : public Napi::AsyncWorker {

  public:
    OpenAsyncWorker(ODBCConnection *odbcConnectionObject, std::string connectionString, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      connectionString(connectionString) {}

    ~OpenAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::OpenAsyncWorker::Execute : connectTimeout=%i, loginTimeout = %i\n", *&(odbcConnectionObject->connectTimeout), *&(odbcConnectionObject->loginTimeout));

      printf("\nHDBC IS %d", odbcConnectionObject->m_hDBC);

      printf("\nExecute");
      std::cout << std::endl << connectionString;
      
      uv_mutex_lock(&ODBC::g_odbcMutex); 
      
      // if (odbcConnectionObject->connectTimeout > 0) {
      //   //NOTE: SQLSetConnectAttr requires the thread to be locked
      //   sqlReturnCode = SQLSetConnectAttr(
      //     odbcConnectionObject->m_hDBC,                              //ConnectionHandle
      //     SQL_ATTR_CONNECTION_TIMEOUT,               //Attribute
      //     (SQLPOINTER) size_t(odbcConnectionObject->connectTimeout), //ValuePtr
      //     SQL_IS_UINTEGER);                          //StringLength

      //     printf("\nSQLRETURN1 IS %d", sqlReturnCode);
      // }
      
      // if (odbcConnectionObject->loginTimeout > 0) {
      //   //NOTE: SQLSetConnectAttr requires the thread to be locked
      //   sqlReturnCode = SQLSetConnectAttr(
      //     odbcConnectionObject->m_hDBC,                            //ConnectionHandle
      //     SQL_ATTR_LOGIN_TIMEOUT,                  //Attribute
      //     (SQLPOINTER) size_t(odbcConnectionObject->loginTimeout), //ValuePtr
      //     SQL_IS_UINTEGER);                        //StringLength

      //     printf("\nSQLRETURN2 IS %d", sqlReturnCode);
      // }

      SQLSMALLINT connectionLength = connectionString.length() + 1;
      std::vector<unsigned char> sqlVec(connectionString.begin(), connectionString.end());
      sqlVec.push_back('\0');
      SQLCHAR *connectionStringPtr = &sqlVec[0];

      //Attempt to connect
      //NOTE: SQLDriverConnect requires the thread to be locked
      sqlReturnCode = SQLDriverConnect(
        odbcConnectionObject->m_hDBC,   // ConnectionHandle
        NULL,                           // WindowHandle
        connectionStringPtr,            // InConnectionString
        SQL_NTS,                        // StringLength1
        NULL,                           // OutConnectionString
        0,                              // BufferLength - in characters
        NULL,                           // StringLength2Ptr
        SQL_DRIVER_NOPROMPT);           // DriverCompletion

      printf("\nSQLRETURN3 IS %d", sqlReturnCode);
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {



        HSTMT hStmt;
        
        //allocate a temporary statment
        sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &hStmt);
        
        //try to determine if the driver can handle
        //multiple recordsets
        sqlReturnCode = SQLGetFunctions(
          odbcConnectionObject->m_hDBC,
          SQL_API_SQLMORERESULTS, 
          &(odbcConnectionObject->canHaveMoreResults));

        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          odbcConnectionObject->canHaveMoreResults = 0;
        }
        
        //free the handle
        sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
      } else {
        SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH + 1];
        SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
        SQLINTEGER sqlcode = 0;
        SQLSMALLINT length = 0;

        memset(msg, '\0', SQL_MAX_MESSAGE_LENGTH + 1);
        memset(sqlstate, '\0', SQL_SQLSTATE_SIZE + 1);
        sqlReturnCode = SQLGetDiagRec(SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC, 1, sqlstate, &sqlcode, msg, SQL_MAX_MESSAGE_LENGTH + 1, &length);

        printf("\nSQLRETURN4 IS %d", sqlReturnCode);

        std::cout << "Giving you some error info" << std::endl;
        std::cout << msg << std::endl;
        std::cout << sqlstate << std::endl;
      }

      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::OpenAsyncWorker::OnOK\n");

      printf("OnOK");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        printf("\nif");
        odbcConnectionObject->connected = true;
        callbackArguments.push_back(env.Null());
      } else {
        printf("\nelse");
        Napi::Value objError = ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC);

        callbackArguments.push_back(objError);
      }

      Callback().Call(callbackArguments);

      //free(connectionString);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    std::string connectionString;
    SQLRETURN sqlReturnCode;
};

Napi::Value ODBCConnection::Open(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::Open\n");

  // TODO MI: Check that two arguments, a string and a function function, was passed in

  printf("\nINFO LENGTH IS %d", info.Length());

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString = info[0].As<Napi::String>();
  Napi::Function callback = info[1].As<Napi::Function>();

  int connectionLength;
  void* connectionStringPtr;

  //copy the connection string to the work data  
  #ifdef UNICODE
    connectionLength = (connectionString.Utf16Value().length() + 1) * sizeof(char16_t);
    std::u16string tempString = connectionString.Utf16Value();
    std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    connectionStringPtr = &sqlVec[0];
  #else
    connectionLength = connectionString.Utf8Value().length() + 1;
    std::string tempString = connectionString.Utf8Value();
    std::vector<char> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    connectionStringPtr = &sqlVec[0];
  #endif

  // std::cout << tempString << std::endl;
  // std::cout << "HEY THE PTR IS " << *((char*)connectionStringPtr) << std::endl;

  OpenAsyncWorker *worker = new OpenAsyncWorker(this, tempString, callback);
  worker->Queue();

  return env.Undefined();
}
/*
 * OpenSync
 */

Napi::Value ODBCConnection::OpenSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::OpenSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString = info[0].As<Napi::String>();
  Napi::Function callback = info[1].As<Napi::Function>();

  bool err;
  int connectionLength;
  void* connectionStringPtr;
  SQLRETURN ret;
  Napi::Value objError;

  //copy the connection string to the work data  
  #ifdef UNICODE
    connectionLength = (connectionString.Utf16Value().length() + 1) * sizeof(char16_t);
    std::u16string tempString = connectionString.Utf16Value();
    std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    connectionStringPtr = &sqlVec[0];
  #else
    connectionLength = connectionString.Utf8Value().length() + 1;
    std::string tempString = connectionString.Utf8Value();
    std::vector<char> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    connectionStringPtr = &sqlVec[0];
  #endif
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  if (this->connectTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      this->m_hDBC,                              //ConnectionHandle
      SQL_ATTR_CONNECTION_TIMEOUT,               //Attribute
      (SQLPOINTER) size_t(this->connectTimeout), //ValuePtr
      SQL_IS_UINTEGER);                          //StringLength
  }

  if (this->loginTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      this->m_hDBC,                            //ConnectionHandle
      SQL_ATTR_LOGIN_TIMEOUT,                  //Attribute
      (SQLPOINTER) size_t(this->loginTimeout), //ValuePtr
      SQL_IS_UINTEGER);                        //StringLength
  }
  
  //Attempt to connect
  //NOTE: SQLDriverConnect requires the thread to be locked
  ret = SQLDriverConnect(
    this->m_hDBC,                   //ConnectionHandle
    NULL,                           //WindowHandle
    (SQLTCHAR*) connectionStringPtr,//InConnectionString
    connectionLength,               //StringLength1
    NULL,                           //OutConnectionString
    0,                              //BufferLength - in characters
    NULL,                           //StringLength2Ptr
    SQL_DRIVER_NOPROMPT);           //DriverCompletion

  if (!SQL_SUCCEEDED(ret)) {
    err = true;
    
    objError = ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->self()->m_hDBC);
  }
  else {
    HSTMT hStmt;
    
    //allocate a temporary statment
    ret = SQLAllocHandle(SQL_HANDLE_STMT, this->m_hDBC, &hStmt);
    
    //try to determine if the driver can handle
    //multiple recordsets
    ret = SQLGetFunctions(
      this->m_hDBC,
      SQL_API_SQLMORERESULTS, 
      &(this->canHaveMoreResults));

    if (!SQL_SUCCEEDED(ret)) {
      this->canHaveMoreResults = 0;
    }
  
    //free the handle
    ret = SQLFreeHandle( SQL_HANDLE_STMT, hStmt);
    
    this->connected = true;
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  free(connectionString);
  
  if (err) {
    Napi::Error(env, objError).ThrowAsJavaScriptException();
    return env.Null();
  }
  else {
    return Napi::Boolean::New(env, true);
  }
}

/*
 * Close
 * 
 */

class CloseAsyncWorker : public Napi::AsyncWorker {

  public:
    CloseAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::Execute\n");
      
      //TODO: check to see if there are any open statements
      //on this connection
      
      odbcConnectionObject->Free(); // should this return a SQLRETURN code?
      
      sqlReturnCode = 0;
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        odbcConnectionObject->connected = false;
        callbackArguments.push_back(env.Null());
      } else {
        Napi::String error = Napi::String::New(env, "Error closing database.");
        callbackArguments.push_back(error);
      }

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
};

Napi::Value ODBCConnection::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::Close\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_FUN_ARG(0, cb);

  Napi::Function callback = info[0].As<Napi::Function>();

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * CloseSync
 */

Napi::Value ODBCConnection::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //TODO: check to see if there are any open statements
  //on this connection
  
  this->Free();
  
  this->connected = false;

  return Napi::Boolean::New(env, true);
}

/*
 * CreateStatement
 * 
 */

class CreateStatementAsyncWorker : public Napi::AsyncWorker {

  public:
    CreateStatementAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~CreateStatementAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker:Execute\n");
      //DEBUG_PRINTF("ODBCConnection::UV_CreateStatement m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
      //  data->conn->m_hENV,
      //  data->conn->m_hDBC,
      //  data->hSTMT
      //);
      
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      //allocate a new statment handle
      SQLAllocHandle( SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &hSTMT);

      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      //DEBUG_PRINTF("ODBCConnection::UV_AfterCreateStatement m_hDBC=%X m_hDBC=%X hSTMT=%X\n",
      //  data->conn->m_hENV,
      //  data->conn->m_hDBC,
      //  data->hSTMT
      //);
      
      std::vector<napi_value> statementArguments;
      statementArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->m_hENV)));
      statementArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->m_hDBC)));
      statementArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));
      
      // create a new ODBCStatement object as a Napi::Value
      Napi::Value statementObject = ODBCStatement::constructor.Call(statementArguments);


      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());      // callbackArguments[0]
      callbackArguments.push_back(statementObject); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    HSTMT hSTMT;
};

Napi::Value ODBCConnection::CreateStatement(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::CreateStatement\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_FUN_ARG(0, cb);

  Napi::Function callback = info[0].As<Napi::Function>();

  CreateStatementAsyncWorker *worker = new CreateStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * CreateStatementSync
 * 
 */

Napi::Value ODBCConnection::CreateStatementSync(const Napi::CallbackInfo& info) {
  // DEBUG_PRINTF("ODBCConnection::CreateStatementSync\n");
  // Napi::HandleScope scope(env);

  // HSTMT hSTMT;

  // uv_mutex_lock(&ODBC::g_odbcMutex);
  
  // SQLAllocHandle(
  //   SQL_HANDLE_STMT, 
  //   this->m_hDBC, 
  //   &hSTMT);
  
  // uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  // Napi::Value params[3];
  // params[0] = Napi::External::New(env, &m_hENV);
  // params[1] = Napi::External::New(env, &m_hDBC);
  // params[2] = Napi::External::New(env, &hSTMT);
  
  // Napi::Object js_result; //(Napi::Function::New(env, ODBCStatement::constructor)->NewInstance(3, params));
  
  // return js_result;
}

/*
 * Query
 */

class QueryAsyncWorker : public Napi::AsyncWorker {

  public:
    QueryAsyncWorker(ODBCConnection *odbcConnectionObject, query_work_data *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~QueryAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::UV_Query\n");

      Parameter prm;
      SQLRETURN ret;
      
      uv_mutex_lock(&ODBC::g_odbcMutex);

      //allocate a new statment handle
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject, &hSTMT);

      uv_mutex_unlock(&ODBC::g_odbcMutex);

      // SQLExecDirect will use bound parameters, but without the overhead of SQLPrepare
      // for a single execution.
      if (data->paramCount) {
        for (int i = 0; i < data->paramCount; i++) {
          prm = data->params[i];


          /*DEBUG_TPRINTF(
            SQL_T("ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%i size=%i length=%i &length=%X\n"), i, prm.ValueType, prm.ParameterType,
            prm.BufferLength, prm.ColumnSize, prm.length, &data->params[i].length);*/

          ret = SQLBindParameter(
            data->hSTMT,                        //StatementHandle
            i + 1,                              //ParameterNumber
            SQL_PARAM_INPUT,                    //InputOutputType
            prm.ValueType,
            prm.ParameterType,
            prm.ColumnSize,
            prm.DecimalDigits,
            prm.ParameterValuePtr,
            prm.BufferLength,
            &data->params[i].StrLen_or_IndPtr);

          if (ret == SQL_ERROR) {
            data->result = ret;
            return;
          }
        }
      }

      // execute the query directly
      ret = SQLExecDirect(
        data->hSTMT,
        (SQLTCHAR *)data->sql,
        data->sqlLen);

      // this will be checked later in UV_AfterQuery
      data->result = ret;
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->result=%i, data->noResultObject=%i\n", data->result, data->noResultObject);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;


      if (data->result != SQL_ERROR && data->noResultObject) {

        //We have been requested to not create a result object
        //this means we should release the handle now and call back
        //with env.True()
        
        uv_mutex_lock(&ODBC::g_odbcMutex);
        
        SQLFreeHandle(SQL_HANDLE_STMT, hSTMT);
      
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(Napi::Boolean::New(env, true));

      } else {

        bool* canFreeHandle = new bool(true);

        std::vector<napi_value> resultArguments;
        
        resultArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));
        resultArguments.push_back(Napi::External<bool>::New(env, canFreeHandle));

        // create a new ODBCResult object as a Napi::Value
        Napi::Value resultObject = ODBCResult::constructor.Call(resultArguments);

        // Check now to see if there was an error (as there may be further result sets)
        if (data->result == SQL_ERROR) {
          callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] SQL_ERROR"));
        } else {
          callbackArguments.push_back(env.Null());
        }

        callbackArguments.push_back(resultObject);
      }

      Callback().Call(callbackArguments);

      if (data->paramCount) {
        Parameter prm;
        // free parameters
        for (int i = 0; i < data->paramCount; i++) {
          if (prm = data->params[i], prm.ParameterValuePtr != NULL) {
            switch (prm.ValueType) {
              case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break; 
              case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
              case SQL_C_LONG:    delete (int64_t *)prm.ParameterValuePtr; break;
              case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
              case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
            }
          }
        }
        
        free(data->params);
      }
      
      free(data->sql);
      free(data->catalog);
      free(data->schema);
      free(data->table);
      free(data->type);
      free(data->column);
      free(data);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    query_work_data *data;
    HSTMT hSTMT;
};

Napi::Value ODBCConnection::Query(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::Query\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  Napi::String sql;
  Napi::Function callback;  

  query_work_data* data = (query_work_data *) calloc(1, sizeof(query_work_data));

  //Check arguments for different variations of calling this function
  if (info.Length() == 3) {
    //handle Query("sql string", [params], function cb () {});
    
    if ( !info[0].IsString() ) {
      Napi::TypeError::New(env, "Argument 0 must be an String.").ThrowAsJavaScriptException();
      return env.Null();
    }
    else if ( !info[1].IsArray() ) {
      Napi::TypeError::New(env, "Argument 1 must be an Array.").ThrowAsJavaScriptException();
      return env.Null();
    }
    else if ( !info[2].IsFunction() ) {
      Napi::TypeError::New(env, "Argument 2 must be a Function.").ThrowAsJavaScriptException();
      return env.Null();
    }

    sql = info[0].As<Napi::String>();
    
    Napi::Array parameterArray = info[1].As<Napi::Array>();
    data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));
    callback = info[2].As<Napi::Function>();
  }
  else if (info.Length() == 2 ) {
    //handle either Query("sql", cb) or Query({ settings }, cb)
    if (!info[1].IsFunction()) {
      Napi::TypeError::New(env, "ODBCConnection::Query(): Argument 1 must be a Function.").ThrowAsJavaScriptException();
      return env.Null();
    }
    
    callback = info[1].As<Napi::Function>();
    
    if (info[0].IsString()) {
      //handle Query("sql", function cb () {})    
      sql = info[0].As<Napi::String>();
    }
    else if (info[0].IsObject()) {
      //NOTE: going forward this is the way we should expand options
      //rather than adding more arguments to the function signature.
      //specify options on an options object.
      //handle Query({}, function cb () {});
      
      Napi::Object obj = info[0].As<Napi::Object>();
      
      Napi::String optionSqlKey = Napi::String::New(env, OPTION_SQL.Utf8Value());
      if (obj.Has(optionSqlKey) && obj.Get(optionSqlKey).IsString()) {
        sql = obj.Get(optionSqlKey).ToString();
      }
      else {
        sql = Napi::String::New(env, "");
      }
      
      Napi::String optionParamsKey = Napi::String::New(env, OPTION_PARAMS.Utf8Value());
      if (obj.Has(optionParamsKey) && obj.Get(optionParamsKey).IsArray()) {
        auto ugh = obj.Get(optionParamsKey);
        Napi::Array optionParamsArray = ugh.As<Napi::Array>();
        data->params = ODBC::GetParametersFromArray(&optionParamsArray, &(data->paramCount));
      }
      else {
        data->paramCount = 0;
      }
      
      Napi::String optionNoResultsKey = Napi::String::New(env, OPTION_NORESULTS.Utf8Value());
      if (obj.Has(optionNoResultsKey) && obj.Get(optionNoResultsKey).IsBoolean()) {
        data->noResultObject = obj.Get(optionNoResultsKey).ToBoolean().Value();
      }
      else {
        data->noResultObject = false;
      }
    }
    else {
      Napi::TypeError::New(env, "ODBCConnection::Query(): Argument 0 must be a String or an Object.").ThrowAsJavaScriptException();
      return env.Null();
    }
  }
  else {
    Napi::TypeError::New(env, "ODBCConnection::Query(): Requires either 2 or 3 Arguments. ").ThrowAsJavaScriptException();
    return env.Null();
  }
  //Done checking arguments

  #ifdef UNICODE
    std::u16string tempString = sql.Utf16Value();
    data->sqlLen = tempString.length();
    data->sqlSize = (data->sqlLen * sizeof(char16_t)) + sizeof(char16_t);
    std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    data->sql = &sqlVec[0];
  #else
    std::string tempString = sql.Utf8Value();
    data->sqlLen = tempString.length();
    data->sqlSize = data->sqlLen + 1;
    std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    data->sql = &sqlVec[0];
  #endif

  DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*) data->sql);
  

  QueryAsyncWorker *worker = new QueryAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * QuerySync
 */

Napi::Value ODBCConnection::QuerySync(const Napi::CallbackInfo& info) {
//   DEBUG_PRINTF("ODBCConnection::QuerySync\n");
//   Napi::HandleScope scope(env);

// #ifdef UNICODE
//   String::Value* sql;
// #else
//   String::Utf8Value* sql;
// #endif

//   Parameter* params = new Parameter[0];
//   Parameter prm;
//   SQLRETURN ret;
//   HSTMT hSTMT;
//   int paramCount = 0;
//   bool noResultObject = false;
  
//   //Check arguments for different variations of calling this function
//   if (info.Length() == 2) {
//     //handle QuerySync("sql string", [params]);
    
//     if ( !info[0].IsString() ) {
//       Napi::TypeError::New(env, "ODBCConnection::QuerySync(): Argument 0 must be an String.").ThrowAsJavaScriptException();
//       return env.Null();
//     }
//     else if (!info[1].IsArray()) {
//       Napi::TypeError::New(env, "ODBCConnection::QuerySync(): Argument 1 must be an Array.").ThrowAsJavaScriptException();
//       return env.Null();
//     }

// #ifdef UNICODE
//     sql = new String::Value(info[0].ToString());
// #else
//     sql = new String::Utf8Value(info[0].ToString());
// #endif

//     params = ODBC::GetParametersFromArray(
//       info[1].As<Napi::Array>(),
//       &paramCount);

//   }
//   else if (info.Length() == 1 ) {
//     //handle either QuerySync("sql") or QuerySync({ settings })

//     if (info[0].IsString()) {
//       //handle Query("sql")
// #ifdef UNICODE
//       sql = new String::Value(info[0].ToString());
// #else
//       sql = new String::Utf8Value(info[0].ToString());
// #endif
    
//       paramCount = 0;
//     }
//     else if (info[0].IsObject()) {
//       //NOTE: going forward this is the way we should expand options
//       //rather than adding more arguments to the function signature.
//       //specify options on an options object.
//       //handle Query({}, function cb () {});
      
//       Napi::Object obj = info[0].ToObject();
      
//       Napi::String optionSqlKey = Napi::String::New(env, OPTION_SQL);
//       if (obj->Has(optionSqlKey) && obj->Get(optionSqlKey).IsString()) {
// #ifdef UNICODE
//         sql = new String::Value(obj->Get(optionSqlKey)->ToString());
// #else
//         sql = new String::Utf8Value(obj->Get(optionSqlKey)->ToString());
// #endif
//       }
//       else {
// #ifdef UNICODE
//         sql = new String::Value(Napi::New(env, ""));
// #else
//         sql = new String::Utf8Value(Napi::New(env, ""));
// #endif
//       }

//       Napi::String optionParamsKey = Napi::New(env, OPTION_PARAMS);
//       if (obj->Has(optionParamsKey) && obj->Get(optionParamsKey)->IsArray()) {
//         params = ODBC::GetParametersFromArray(
//           obj->Get(optionParamsKey.As<Napi::Array>()),
//           &paramCount);
//       }
//       else {
//         paramCount = 0;
//       }
      
//       Napi::String optionNoResultsKey = Napi::New(env, OPTION_NORESULTS);
//       if (obj->Has(optionNoResultsKey) && obj->Get(optionNoResultsKey)->IsBoolean()) {
//         noResultObject = obj->Get(optionNoResultsKey)->ToBoolean()->Value();
//       }
//     }
//     else {
//       Napi::TypeError::New(env, "ODBCConnection::QuerySync(): Argument 0 must be a String or an Object.").ThrowAsJavaScriptException();
//       return env.Null();
//     }
//   }
//   else {
//     Napi::TypeError::New(env, "ODBCConnection::QuerySync(): Requires either 1 or 2 Arguments.").ThrowAsJavaScriptException();
//     return env.Null();
//   }
//   //Done checking arguments

//   uv_mutex_lock(&ODBC::g_odbcMutex);

//   //allocate a new statment handle
//   ret = SQLAllocHandle( SQL_HANDLE_STMT, 
//                   this->m_hDBC, 
//                   &hSTMT );

//   uv_mutex_unlock(&ODBC::g_odbcMutex);

//   DEBUG_PRINTF("ODBCConnection::QuerySync - hSTMT=%p\n", hSTMT);
  
//   if (SQL_SUCCEEDED(ret)) {
//     if (paramCount) {
//       for (int i = 0; i < paramCount; i++) {
//         prm = params[i];
        
//         DEBUG_PRINTF(
//           "ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%lli size=%lli length=%lli &length=%lli\n", i, prm.ValueType, prm.ParameterType, 
//           prm.BufferLength, prm.ColumnSize, prm.StrLen_or_IndPtr, params[i].StrLen_or_IndPtr);

//         ret = SQLBindParameter(
//           hSTMT,                    //StatementHandle
//           i + 1,                    //ParameterNumber
//           SQL_PARAM_INPUT,          //InputOutputType
//           prm.ValueType,
//           prm.ParameterType,
//           prm.ColumnSize,
//           prm.DecimalDigits,
//           prm.ParameterValuePtr,
//           prm.BufferLength,
//           &params[i].StrLen_or_IndPtr);
        
//         if (ret == SQL_ERROR) {break;}
//       }
//     }

//     if (SQL_SUCCEEDED(ret)) {
//       ret = SQLExecDirect(
//         hSTMT,
//         (SQLTCHAR *) **sql, 
//         sql->length());
//     }
    
//     // free parameters
//     for (int i = 0; i < paramCount; i++) {
//       if (prm = params[i], prm.ParameterValuePtr != NULL) {
//         switch (prm.ValueType) {
//           case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break;
//           case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
//           case SQL_C_LONG:    delete (int64_t *)prm.ParameterValuePtr; break;
//           case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
//           case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
//         }
//       }
//     }
    
//     free(params);
//   }
  
//   delete sql;
  
//   //check to see if there was an error during execution
//   if (ret == SQL_ERROR) {
//     Napi::Value objError = ODBC::GetSQLError(
//       SQL_HANDLE_STMT,
//       hSTMT,
//       (char *) "[node-odbc] Error in ODBCConnection::QuerySync"
//     );

//     Napi::Error::New(env, objError).ThrowAsJavaScriptException();
    
//     return env.Null();
//   }
//   else if (noResultObject) {
//     //if there is not result object requested then
//     //we must destroy the STMT ourselves.
//     uv_mutex_lock(&ODBC::g_odbcMutex);
    
//     SQLFreeHandle(SQL_HANDLE_STMT, hSTMT);
   
//     uv_mutex_unlock(&ODBC::g_odbcMutex);
    
//     return env.True();
//   }
//   else {
//     Napi::Value result[4];
//     bool* canFreeHandle = new bool(true);
    
//     result[0] = Napi::External::New(env, conn->m_hENV);
//     result[1] = Napi::External::New(env, conn->m_hDBC);
//     result[2] = Napi::External::New(env, hSTMT);
//     result[3] = Napi::External::New(env, canFreeHandle);
    
//     Napi::Object js_result = Napi::Function::New(env, ODBCResult::constructor)->NewInstance(4, result);

//     return js_result;
//   }
}


/*
 * GetInfoSync
 */

Napi::Value ODBCConnection::GetInfoSync(const Napi::CallbackInfo& info) {
//   DEBUG_PRINTF("ODBCConnection::GetInfoSync\n");

//   Napi::Env env = info.Env();
//   Napi::HandleScope scope(env);

//   if (info.Length() == 1) {
//     if ( !info[0].IsNumber() ) {
//         Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): Argument 0 must be a Number.").ThrowAsJavaScriptException();
//         return env.Null();
//     }
//   }
//   else {
//     Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): Requires 1 Argument.").ThrowAsJavaScriptException();
//     return env.Null();
//   }

//   SQLUSMALLINT InfoType = info[0].As<Napi::Number>().DoubleValue();

//   switch (InfoType) {
//     case SQL_USER_NAME:
//       SQLRETURN ret;
//       SQLTCHAR userName[255];
//       SQLSMALLINT userNameLength;

//       ret = SQLGetInfo(this->m_hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

//       if (SQL_SUCCEEDED(ret)) {
// #ifdef UNICODE
//         return Napi::New(env, (uint16_t *)userName);
// #else
//         return Napi::New(env, (const char *) userName);
// #endif
//       }
//       break;

//     default:
//       Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): The only supported Argument is SQL_USER_NAME.").ThrowAsJavaScriptException();
//       return env.Null();
//   }
}


/*
 * Tables
 */
class TablesAsyncWorker : public Napi::AsyncWorker {

  public:
    TablesAsyncWorker(ODBCConnection *odbcConnectionObject, query_work_data *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~TablesAsyncWorker() {}

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      SQLAllocHandle(SQL_HANDLE_STMT, data->conn->m_hDBC, &data->hSTMT );
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      SQLRETURN ret = SQLTables( 
        data->hSTMT, 
        (SQLTCHAR *) data->catalog,   SQL_NTS, 
        (SQLTCHAR *) data->schema,   SQL_NTS, 
        (SQLTCHAR *) data->table,   SQL_NTS, 
        (SQLTCHAR *) data->type,   SQL_NTS
      );
      
      // this will be checked later in UV_AfterQuery
      data->result = ret; 
    }

    void OnOK() {

      // TODO: The same as query. Should use an outside function?
    }

  private:
    ODBCConnection *odbcConnectionObject;
    query_work_data *data;
};


Napi::Value ODBCConnection::Tables(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_STRO_OR_NULL_ARG(0, catalog);
  //REQ_STRO_OR_NULL_ARG(1, schema);
  //REQ_STRO_OR_NULL_ARG(2, table);
  //REQ_STRO_OR_NULL_ARG(3, type);
  Napi::String catalog = info[0].As<Napi::String>(); // or null?
  Napi::String schema = info[1].As<Napi::String>(); 
  Napi::String table = info[2].As<Napi::String>();
  Napi::String type = info[3].As<Napi::String>();
  Napi::Function callback = info[4].As<Napi::Function>();

  query_work_data* data = 
    (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    //Napi::LowMemoryNotification();
    Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
    return env.Null();
  }

  data->sql = NULL;
  data->catalog = NULL;
  data->schema = NULL;
  data->table = NULL;
  data->type = NULL;
  data->column = NULL;

  if (!(catalog.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = catalog.Utf16Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->catalog = &sqlVec[0];
    #else
      std::string tempString = catalog.Utf8Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->catalog = &sqlVec[0];
    #endif
  }

  if (!(schema.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = schema.Utf16Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->schema = &sqlVec[0];
    #else
      std::string tempString = schema.Utf8Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->schema = &sqlVec[0];
    #endif
  }
  
  if (!(table.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = table.Utf16Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->table = &sqlVec[0];
    #else
      std::string tempString = table.Utf8Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->table = &sqlVec[0];
    #endif
  }
  
  if (!(type.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = type.Utf16Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->type = &sqlVec[0];
    #else
      std::string tempString = type.Utf8Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->type = &sqlVec[0];
    #endif
  }

  TablesAsyncWorker *worker = new TablesAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

class ColumnsAsyncWorker : public Napi::AsyncWorker {

  public:
    ColumnsAsyncWorker(ODBCConnection *odbcConnectionObject, query_work_data *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~ColumnsAsyncWorker() {}

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      SQLAllocHandle(SQL_HANDLE_STMT, data->conn->m_hDBC, &data->hSTMT );
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      SQLRETURN ret = SQLColumns( 
        data->hSTMT, 
        (SQLTCHAR *) data->catalog,   SQL_NTS, 
        (SQLTCHAR *) data->schema,   SQL_NTS, 
        (SQLTCHAR *) data->table,   SQL_NTS, 
        (SQLTCHAR *) data->column,   SQL_NTS
      );
      
      // this will be checked later in UV_AfterQuery
      data->result = ret;
    }

    void OnOK() {

      // TODO: The same as query. Should use an outside function?
    }

  private:
    ODBCConnection *odbcConnectionObject;
    query_work_data *data;
};


/*
 * Columns
 */

Napi::Value ODBCConnection::Columns(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_STRO_OR_NULL_ARG(0, catalog);
  //REQ_STRO_OR_NULL_ARG(1, schema);
  //REQ_STRO_OR_NULL_ARG(2, table);
  //REQ_STRO_OR_NULL_ARG(3, type);
  Napi::String catalog = info[0].As<Napi::String>(); // or null?
  Napi::String schema = info[1].As<Napi::String>(); 
  Napi::String table = info[2].As<Napi::String>();
  Napi::String type = info[3].As<Napi::String>();
  Napi::Function callback = info[4].As<Napi::Function>();

  query_work_data* data = 
    (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    //Napi::LowMemoryNotification();
    Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
    return env.Null();
  }

  data->sql = NULL;
  data->catalog = NULL;
  data->schema = NULL;
  data->table = NULL;
  data->type = NULL;
  data->column = NULL;

  if (!(catalog.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = catalog.Utf16Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->catalog = &sqlVec[0];
    #else
      std::string tempString = catalog.Utf8Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->catalog = &sqlVec[0];
    #endif
  }

  if (!(schema.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = schema.Utf16Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->schema = &sqlVec[0];
    #else
      std::string tempString = schema.Utf8Value();
      std::vector<char16_t> sqlVec(tempString.begin(), tempString.end());
      sqlVec.push_back('\0');
      data->schema = &sqlVec[0];
    #endif
  }
  
  if (!(table.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = table.Utf16Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->table = &sqlVec[0];
    #else
      std::string tempString = table.Utf8Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->table = &sqlVec[0];
    #endif
  }
  
  if (!(type.Utf8Value() == "null")) {
    #ifdef UNICODE
      std::u16string tempString = type.Utf16Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->type = &sqlVec[0];
    #else
      std::string tempString = type.Utf8Value();
      std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
      sqlVec.push_back('\0');
      data->type = &sqlVec[0];
    #endif
  }

  ColumnsAsyncWorker *worker = new ColumnsAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * BeginTransactionSync
 * 
 */

Napi::Value ODBCConnection::BeginTransactionSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCConnection::BeginTransactionSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //set the connection manual commits
  SQLRETURN sqlReturnCode = SQLSetConnectAttr(this->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_NTS);
  
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->m_hDBC);  
    Napi::Error(env, error).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  
  return Napi::Boolean::New(env, true);
}

/*
 * BeginTransaction
 * 
 */

class BeginTransactionAsyncWorker : public Napi::AsyncWorker {

  public:
    BeginTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~BeginTransactionAsyncWorker() {}

    void Execute() {

        DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::Execute\n");
        
        //set the connection manual commits
        sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_NTS);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {
        callbackArguments.push_back(env.Null());
      } else {
        Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC);
        callbackArguments.push_back(error);
      }

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
};

Napi::Value ODBCConnection::BeginTransaction(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::BeginTransaction\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_FUN_ARG(0, cb);
  
  /*
  if (!data) {
    Napi::LowMemoryNotification();
    Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
    return env.Null();
  }
  */

  Napi::Function callback = info[0].As<Napi::Function>();

  BeginTransactionAsyncWorker *worker = new BeginTransactionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * EndTransaction
 * 
 */

class EndTransactionAsyncWorker : public Napi::AsyncWorker {

  public:
    EndTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, SQLSMALLINT completionType, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      completionType(completionType) {}

    ~EndTransactionAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::Execute\n");
      
      bool err = false;
      
      //Call SQLEndTran
      sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC, completionType);
      
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        err = true;
      }
      
      //Reset the connection back to autocommit
      SQLRETURN tempCode = SQLSetConnectAttr(odbcConnectionObject->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
      
      if (!SQL_SUCCEEDED(tempCode) && !err) {
        //there was not an earlier error,
        //so we shall pass the return code from
        //this last call.
        sqlReturnCode = tempCode;
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {
        callbackArguments.push_back(env.Null());
      } else {
        Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC);
        callbackArguments.push_back(error);
      }

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLSMALLINT completionType;
    SQLRETURN sqlReturnCode;
};

Napi::Value ODBCConnection::EndTransaction(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::EndTransaction\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // REQ_BOOL_ARG(0, rollback);
  // REQ_FUN_ARG(1, cb);
  
  // if (!data) {
  //   Napi::LowMemoryNotification();
  //   Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
  //   return env.Null();
  // }

  Napi::Boolean rollback = info[0].As<Napi::Boolean>();
  Napi::Function callback = info[1].As<Napi::Function>();

  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;

  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, completionType, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * EndTransactionSync
 * 
 */

Napi::Value ODBCConnection::EndTransactionSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::EndTransactionSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  //REQ_BOOL_ARG(0, rollback);
  Napi::Boolean rollback = info[0].As<Napi::Boolean>();
  
  Napi::Value error;
  SQLRETURN sqlReturnCode;
  bool isError = false;

  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;
  
  //Call SQLEndTran
  sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, this->m_hDBC, completionType);
  
  //check how the transaction went
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    isError = true;
    error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->m_hDBC);
  }
  
  //Reset the connection back to autocommit
  SQLRETURN tempCode = SQLSetConnectAttr(this->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
  
  //check how setting the connection attr went
  //but only process the code if an error has not already
  //occurred. If an error occurred during SQLEndTran,
  //that is the error that we want to throw.

  if (!SQL_SUCCEEDED(tempCode) && !isError) {
    //TODO: if this also failed, we really should
    //be restarting the connection or something to deal with this state
    isError = true;  
    error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, this->m_hDBC);
  }
  
  if (isError) {
    Napi::Error(env, error).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  else {
    return Napi::Boolean::New(env, true);
  }
}