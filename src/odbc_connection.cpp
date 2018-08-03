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
    InstanceMethod("endTransactionSync", &ODBCConnection::EndTransaction),

    InstanceMethod("getInfo", &ODBCConnection::GetInfo),
    InstanceMethod("getInfoSync", &ODBCConnection::GetInfoSync),

    InstanceMethod("columns", &ODBCConnection::Columns),
    InstanceMethod("columnsSync", &ODBCConnection::ColumnsSync),

    InstanceMethod("tables", &ODBCConnection::Tables),
    InstanceMethod("tablesSync", &ODBCConnection::TablesSync),

    InstanceAccessor("connected", &ODBCConnection::ConnectedGetter, &ODBCConnection::ConnectedSetter),
    InstanceAccessor("connectTimeout", &ODBCConnection::ConnectTimeoutGetter, &ODBCConnection::ConnectTimeoutSetter),
    InstanceAccessor("loginTimeout", &ODBCConnection::LoginTimeoutGetter, &ODBCConnection::LoginTimeoutSetter)
  });

  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCConnection", constructorFunction);

  return exports;
}

ODBCConnection::ODBCConnection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCConnection>(info) {

  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());

}

ODBCConnection::~ODBCConnection() {

  DEBUG_PRINTF("ODBCConnection::~ODBCConnection\n");
  SQLRETURN sqlReturnCode;

  this->Free(&sqlReturnCode);
}

void ODBCConnection::Free(SQLRETURN *sqlReturnCode) {

  DEBUG_PRINTF("ODBCConnection::Free\n");

  if (m_hDBC) {

    uv_mutex_lock(&ODBC::g_odbcMutex);

    // If an application calls SQLDisconnect before it has freed all statements
    // associated with the connection, the driver, after it successfully
    // disconnects from the data source, frees those statements and all
    // descriptors that have been explicitly allocated on the connection.
    *sqlReturnCode = SQLDisconnect(m_hDBC);

    if (SQL_SUCCEEDED(*sqlReturnCode)) {

      //Before it calls SQLFreeHandle with a HandleType of SQL_HANDLE_DBC, an
      //application must call SQLDisconnect for the connection if there is a
      // connection on this handle. Otherwise, the call to SQLFreeHandle
      //returns SQL_ERROR and the connection remains valid.
      *sqlReturnCode = SQLFreeHandle(SQL_HANDLE_DBC, m_hDBC);

      if (SQL_SUCCEEDED(*sqlReturnCode)) {

        m_hDBC = NULL;
        this->connected = false;

      } else {

        // TODO: throw an error
        return;
      }

    } else {

      // TODO: Error?
      return;
    }
    
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
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

/******************************************************************************
 *********************************** OPEN *************************************
 *****************************************************************************/

 // OpenAsyncWorker, used by Open function (see below)
class OpenAsyncWorker : public Napi::AsyncWorker {

  public:
    OpenAsyncWorker(ODBCConnection *odbcConnectionObject, std::string connectionString, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      connectionString(connectionString) {}

    ~OpenAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::OpenAsyncWorker::Execute : connectTimeout=%i, loginTimeout = %i\n", *&(odbcConnectionObject->connectTimeout), *&(odbcConnectionObject->loginTimeout));
      
      uv_mutex_lock(&ODBC::g_odbcMutex); 
      
      if (odbcConnectionObject->connectTimeout > 0) {
        //NOTE: SQLSetConnectAttr requires the thread to be locked
        sqlReturnCode = SQLSetConnectAttr(
          odbcConnectionObject->m_hDBC,                              // ConnectionHandle
          SQL_ATTR_CONNECTION_TIMEOUT,                               // Attribute
          (SQLPOINTER) size_t(odbcConnectionObject->connectTimeout), // ValuePtr
          SQL_IS_UINTEGER);                                          // StringLength
      }
      
      if (odbcConnectionObject->loginTimeout > 0) {
        //NOTE: SQLSetConnectAttr requires the thread to be locked
        sqlReturnCode = SQLSetConnectAttr(
          odbcConnectionObject->m_hDBC,                            // ConnectionHandle
          SQL_ATTR_LOGIN_TIMEOUT,                                  // Attribute
          (SQLPOINTER) size_t(odbcConnectionObject->loginTimeout), // ValuePtr
          SQL_IS_UINTEGER);                                        // StringLength
      }

      //SQLSMALLINT connectionLength = connectionString.length() + 1; // using SQL_NTS intead
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
      }

      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::OpenAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        odbcConnectionObject->connected = true;
        callbackArguments.push_back(env.Null());
      } else {
        Napi::Value objError = ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC);

        callbackArguments.push_back(objError);
      }

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    std::string connectionString;
    SQLRETURN sqlReturnCode;
};

/*
 *  ODBCConnection::Open
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'open'.
 *   
 *        info[0]: String: connection string
 *        info[1]: Function: callback function:
 *            function(error)
 *              error: An error object if the connection was not opened, or
 *                     null if operation was successful. 
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::Open(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::Open\n");

  // TODO MI: Check that two arguments, a string and a function function, was passed in

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString = info[0].As<Napi::String>();
  Napi::Function callback = info[1].As<Napi::Function>();

  //copy the connection string to the work data  
  #ifdef UNICODE
    std::u16string tempString = connectionString.Utf16Value();
  #else
    std::string tempString = connectionString.Utf8Value();
  #endif

  OpenAsyncWorker *worker = new OpenAsyncWorker(this, tempString, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::OpenSync
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'openSync'.
 *   
 *        info[0]: String: connection string
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, indicates whether the connection was opened successfully
 */
Napi::Value ODBCConnection::OpenSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::OpenSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // check arguments
  REQ_STR_ARG(0, connectionString);

  SQLRETURN sqlReturnCode;

  //copy the connection string to the work data  
  #ifdef UNICODE
    std::u16string tempString = connectionString.Utf16Value();
  #else
    std::string tempString = connectionString.Utf8Value();
  #endif

  uv_mutex_lock(&ODBC::g_odbcMutex); 
      
  if (this->connectTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    sqlReturnCode = SQLSetConnectAttr(
      this->m_hDBC,                              // ConnectionHandle
      SQL_ATTR_CONNECTION_TIMEOUT,               // Attribute
      (SQLPOINTER) size_t(this->connectTimeout), // ValuePtr
      SQL_IS_UINTEGER);                          // StringLength
  }
  
  if (this->loginTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    sqlReturnCode = SQLSetConnectAttr(
      this->m_hDBC,                            // ConnectionHandle
      SQL_ATTR_LOGIN_TIMEOUT,                  // Attribute
      (SQLPOINTER) size_t(this->loginTimeout), // ValuePtr
      SQL_IS_UINTEGER);                        // StringLength
  }

  //SQLSMALLINT connectionLength = connectionString.length() + 1; // using SQL_NTS intead
  std::vector<unsigned char> sqlVec(tempString.begin(), tempString.end());
  sqlVec.push_back('\0');
  SQLCHAR *connectionStringPtr = &sqlVec[0];

  //Attempt to connect
  //NOTE: SQLDriverConnect requires the thread to be locked
  sqlReturnCode = SQLDriverConnect(
    this->m_hDBC,   // ConnectionHandle
    NULL,                           // WindowHandle
    connectionStringPtr,            // InConnectionString
    SQL_NTS,                        // StringLength1
    NULL,                           // OutConnectionString
    0,                              // BufferLength - in characters
    NULL,                           // StringLength2Ptr
    SQL_DRIVER_NOPROMPT);           // DriverCompletion
  
  if (SQL_SUCCEEDED(sqlReturnCode)) {

    this->connected = true;

    HSTMT hStmt;
    
    //allocate a temporary statment
    sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, this->m_hDBC, &hStmt);
    
    //try to determine if the driver can handle
    //multiple recordsets
    sqlReturnCode = SQLGetFunctions(
      this->m_hDBC,
      SQL_API_SQLMORERESULTS, 
      &(this->canHaveMoreResults));

    if (!SQL_SUCCEEDED(sqlReturnCode)) {
      this->canHaveMoreResults = 0;
    }
    
    //free the handle
    sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    return Napi::Boolean::New(env, true);

  } else {

    SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH + 1];
    SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
    SQLINTEGER sqlcode = 0;
    SQLSMALLINT length = 0;

    memset(msg, '\0', SQL_MAX_MESSAGE_LENGTH + 1);
    memset(sqlstate, '\0', SQL_SQLSTATE_SIZE + 1);
    sqlReturnCode = SQLGetDiagRec(SQL_HANDLE_DBC, this->m_hDBC, 1, sqlstate, &sqlcode, msg, SQL_MAX_MESSAGE_LENGTH + 1, &length);

    return Napi::Boolean::New(env, false); // TODO: This returns an error... but that is going to conflict with returning true.
  }
}

/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

/*
 *  ODBCConnection::Close (Async)
 * 
 *    Description: TODO
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

    void OnError(Error &e) {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // TODO: Handle error in e. Or ignore error and handle it in
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Undefined()); // TODO: Needs to hold error

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

  REQ_FUN_ARG(0, callback);

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::CloseSync
 *    Description: TODO
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
    // TODO: throw Error
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

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker:Execute - m_hDBC=%X m_hDBC=%X\n",
       odbcConnectionObject->m_hENV,
       odbcConnectionObject->m_hDBC,
      );
      
      uv_mutex_lock(&ODBC::g_odbcMutex);
      sqlReturnCode = SQLAllocHandle( SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &hSTMT);
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        return;
      } else {
        SetError("TODO");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker::OnOK - m_hDBC=%X m_hDBC=%X hSTMT=%X\n",
        odbcConnectionObject->m_hENV,
        odbcConnectionObject->m_hDBC,
        hSTMT
      );

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      // arguments for the ODBCStatement constructor
      std::vector<napi_value> statementArguments;
      statementArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->m_hENV)));
      statementArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->m_hDBC)));
      statementArguments.push_back(Napi::External<HSTMT>::New(env, &hSTMT));
      
      // create a new ODBCStatement object as a Napi::Value
      Napi::Value statementObject = ODBCStatement::constructor.New(statementArguments);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());      // callbackArguments[0]
      callbackArguments.push_back(statementObject); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

    void OnError() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker::OnError - m_hDBC=%X m_hDBC=%X hSTMT=%X\n",
        odbcConnectionObject->m_hENV,
        odbcConnectionObject->m_hDBC,
        hSTMT
      );

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // TODO: Error
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null()); // callbackArguments[0]
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
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransactionSync'.
 *   
 *        info[0]: Function: callback function:
 *            function(error, statement)
 *              error: An error object if the transaction wasn't started, or
 *                     null if operation was successful.
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

  REQ_FUN_ARG(0, callback);

  CreateStatementAsyncWorker *worker = new CreateStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::CreateStatementSync
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment.
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
  SQLAllocHandle(SQL_HANDLE_STMT, this->m_hDBC, &hSTMT);

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  std::vector<napi_value> statementArguments;
  statementArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hENV)));
  statementArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
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

    ~QueryAsyncWorker() { 
      delete data;
    }

    void Execute() {

      DEBUG_PRINTF("\nODBCConnection::QueryAsyncWorke::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
      
      uv_mutex_lock(&ODBC::g_odbcMutex);

      //allocate a new statment handle
      data->sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &(data->hSTMT));

      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (data->paramCount > 0) {
        // binds all parameters to the query
        ODBC::BindParameters(data);
      }

      // execute the query directly
      data->sqlReturnCode = SQLExecDirect(
        data->hSTMT,
        (SQLCHAR *)data->sql,
        data->sqlLen
      );

      data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
      data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      // no result object should be created, just return with true instead
      if (data->noResultObject) {

        // TODO: FREE HANDLE
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(Napi::Boolean::New(env, true));

      } else {

        // arguments for the ODBCResult constructor
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
        resultArguments.push_back(Napi::Boolean::New(env, true)); // canFreeHandle 

        resultArguments.push_back(Napi::External<QueryData>::New(env, data));
        
        // create a new ODBCResult object as a Napi::Value
        Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

        // TODO: Error checking
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(resultObject);
      }

      // return results object
      Callback().Call(callbackArguments);
    }

    void OnError() {

      // TODO: Error
      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      // TODO: Error checking
      callbackArguments.push_back(env.Null()); // TODO: Error
      callbackArguments.push_back(env.Null());

      // return results object
      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    QueryData      *data;
};

Napi::Value ODBCConnection::Query(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::Query\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  REQ_STRO_ARG(0, sql);
  REQ_FUN_ARG(1, callback);

  QueryData *data = new QueryData;
  data->paramCount = 0;

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
    std::vector<char> *sqlVec = new std::vector<char>(tempString.begin(), tempString.end());
    sqlVec->push_back('\0');
    data->sql = &((*sqlVec)[0]);
  #endif

  DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
  

  return env.Undefined();
}

Napi::Value ODBCConnection::QuerySync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::QuerySync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  REQ_STRO_ARG(0, sql);

  QueryData *data = new QueryData;

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
    std::vector<char> *sqlVec = new std::vector<char>(tempString.begin(), tempString.end());
    sqlVec->push_back('\0');
    data->sql = &((*sqlVec)[0]);
  #endif

  DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);

  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new statment handle
  data->sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, this->m_hDBC, &(data->hSTMT));

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  if (data->paramCount > 0) {
    // binds all parameters to the query
    ODBC::BindParameters(data);
  }

  // execute the query directly
  data->sqlReturnCode = SQLExecDirect(
    data->hSTMT,
    (SQLCHAR *)data->sql,
    data->sqlLen
  );

  data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
  data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

  // no result object should be created, just return with true instead
  if (data->noResultObject) {

    // TODO: Free handle
    return Napi::Boolean::New(env, true);

  } else {

    // arguments for the ODBCResult constructor
    std::vector<napi_value> resultArguments;
    resultArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hENV)));
    resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
    resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
    resultArguments.push_back(Napi::Boolean::New(env, true)); // canFreeHandle 

    resultArguments.push_back(Napi::External<QueryData>::New(env, data));
    
    // create a new ODBCResult object as a Napi::Value
    return ODBCResult::constructor.New(resultArguments);
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

          sqlReturnCode = SQLGetInfo(odbcConnectionObject->m_hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

          if (SQL_SUCCEEDED(sqlReturnCode)) {
           return;
          } else {
            SetError("TODO");
          }

        default:
          SetError("TODO");
          //Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): The only supported Argument is SQL_USER_NAME.").ThrowAsJavaScriptException();
          //return env.Null();
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

    void OnError() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null()); // TODO: Error
      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLUSMALLINT infoType;
    SQLTCHAR userName[255];
    SQLSMALLINT userNameLength;
    SQLRETURN sqlReturnCode;
};

// GetInfo(Async)
Napi::Value ODBCConnection::GetInfo(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCConnection::GetInfo\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if ( !info[0].IsNumber() ) {
    Napi::TypeError::New(env, "ODBCConnection::GetInfoSync(): Argument 0 must be a Number.").ThrowAsJavaScriptException();
    return env.Null();
  }

  SQLUSMALLINT infoType = info[0].As<Napi::Number>().Int32Value();

  REQ_FUN_ARG(1, callback);

  GetInfoAsyncWorker *worker = new GetInfoAsyncWorker(this, infoType, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * GetInfoSync
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

  SQLUSMALLINT InfoType = info[0].As<Napi::Number>().Int32Value();

  switch (InfoType) {
    case SQL_USER_NAME:
      SQLRETURN sqlReturnCode;
      SQLTCHAR userName[255];
      SQLSMALLINT userNameLength;

      sqlReturnCode = SQLGetInfo(this->m_hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

      if (SQL_SUCCEEDED(sqlReturnCode)) {
        #ifdef UNICODE
          return Napi::String::New(env, (const char16_t *)userName);
        #else
          return Napi::String::New(env, (const char *) userName);
        #endif
      } else {
        // TODO: Error
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

    ~TablesAsyncWorker() { delete data; }

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &data->hSTMT );
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      SQLRETURN ret = SQLTables( 
        data->hSTMT, 
        (SQLTCHAR *) data->catalog, SQL_NTS, 
        (SQLTCHAR *) data->schema, SQL_NTS, 
        (SQLTCHAR *) data->table, SQL_NTS, 
        (SQLTCHAR *) data->type, SQL_NTS
      );
      
      data->sqlReturnCode = ret; 
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i, \n", data->sqlReturnCode, );
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());

      // return results here
      //check to see if the result set has columns
      if (data->columnCount == 0) {
        //this most likely means that the query was something like
        //'insert into ....'
        callbackArguments.push_back(env.Undefined());

      } else {

        Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, false);
        Napi::Object fields = ODBC::GetNapiColumns(env, data->columns, data->columnCount);

        // the result object returned
        Napi::Object result = Napi::Object::New(env);
        result.Set(Napi::String::New(env, "rows"), rows);
        result.Set(Napi::String::New(env, "fields"), fields);

        callbackArguments.push_back(result);
      }

      Callback().Call(callbackArguments);
    }

    void OnError(Error &e) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(Napi::String::New(env, e.Message()));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    QueryData *data;
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

  QueryData* data = new QueryData;
  
  if (!data) {
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

Napi::Value ODBCConnection::TablesSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return env.Undefined();
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

    ~ColumnsAsyncWorker() { delete data; }

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->m_hDBC, &data->hSTMT );
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      data->sqlReturnCode = SQLColumns( 
        data->hSTMT, 
        (SQLTCHAR *)data->catalog, SQL_NTS, 
        (SQLTCHAR *)data->schema, SQL_NTS, 
        (SQLTCHAR *)data->table, SQL_NTS, 
        (SQLTCHAR *)data->column, SQL_NTS
      );

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {

        data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
        data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

        //Only loop through the recordset if there are columns
        if (data->columnCount > 0) {
          ODBC::FetchAll(data); // fetches all data and puts it in data->storedRows
        }

        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          SetError("zoinks2!");
        }

      } else {

        SetError("zoinks1!");
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      // no result object should be created, just return with true instead
      if (data->noResultObject) {

        //free the handle
        data->sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, data->hSTMT);

        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(Napi::Boolean::New(env, true));

      } else {

        // arguments for the ODBCResult constructor
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<HENV>::New(env, &(odbcConnectionObject->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcConnectionObject->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
        resultArguments.push_back(Napi::Boolean::New(env, true)); // canFreeHandle 

        resultArguments.push_back(Napi::External<QueryData>::New(env, data));
        
        // create a new ODBCResult object as a Napi::Value
        Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

        // TODO: Error checking
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(resultObject);
      }

      // return results object
      Callback().Call(callbackArguments);
    }

    void OnError(const Error &e) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(Napi::String::New(env, e.Message()));
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
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'open'.
 *   
 *        info[0]: String: catalog
 *        info[1]: String: schema
 *        info[2]: String: table
 *        info[3]: String: type
 *        info[4]: Function: callback function:
 *            function(error, result)
 *              error: An error object if the connection was not opened, or
 *                     null if operation was successful.
 *              result: The ODBCResult object or a Boolean if noResultObject is
 *                      specified.
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback)
 */
Napi::Value ODBCConnection::Columns(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // check arguments
  REQ_STRO_OR_NULL_ARG(0, catalog);
  REQ_STRO_OR_NULL_ARG(1, schema);
  REQ_STRO_OR_NULL_ARG(2, table);
  REQ_STRO_OR_NULL_ARG(3, type);
  REQ_FUN_ARG(4, callback);

  QueryData* data = new QueryData;
  
  // if (!data) {
  //   //Napi::LowMemoryNotification();
  //   Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
  //   return env.Null();
  // }

  data->sql = NULL;
  data->catalog = NULL;
  data->schema = NULL;
  data->table = NULL;
  data->type = NULL;
  data->column = NULL;

  if (catalog != env.Null()) {
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
 *  ODBCConnection::ColumnsSync
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'columnSync'.
 * 
 *    Return:
 *      Napi::Value:
 *        if noResultObject is specified, then Boolean indicating whether the
 *        query was successful. Otherwise, an ODBCResult object holding the
 *        hSTMT.
 */
Napi::Value ODBCConnection::ColumnsSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData;

  uv_mutex_lock(&ODBC::g_odbcMutex);
      
  SQLAllocHandle(SQL_HANDLE_STMT, this->m_hDBC, &data->hSTMT );
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  data->sqlReturnCode = SQLColumns( 
    data->hSTMT, 
    (SQLTCHAR *)data->catalog, SQL_NTS, 
    (SQLTCHAR *)data->schema, SQL_NTS, 
    (SQLTCHAR *)data->table, SQL_NTS, 
    (SQLTCHAR *)data->column, SQL_NTS
  );

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {

    data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
    data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

    //Only loop through the recordset if there are columns
    if (data->columnCount > 0) {
      ODBC::FetchAll(data); // fetches all data and puts it in data->storedRows
    }

    if (SQL_SUCCEEDED(data->sqlReturnCode)) {
      
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      // no result object should be created, just return with true instead
      if (data->noResultObject) {

        //free the handle
        data->sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, data->hSTMT);

        return Napi::Boolean::New(env, true);

      } else {

        // arguments for the ODBCResult constructor
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
        resultArguments.push_back(Napi::Boolean::New(env, true)); // canFreeHandle 

        resultArguments.push_back(Napi::External<QueryData>::New(env, data));
        
        // create a new ODBCResult object as a Napi::Value
        return ODBCResult::constructor.New(resultArguments);
      }
    } else {
      //TODO Thorw Error
    }

  } else {
    //TODO Thorw Error
  }
}

/******************************************************************************
 **************************** BEGIN TRANSACTION *******************************
 *****************************************************************************/

// BeginTransactionAsyncWorker, used by EndTransaction function (see below)
class BeginTransactionAsyncWorker : public Napi::AsyncWorker {

  public:
    BeginTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~BeginTransactionAsyncWorker() {}

    void Execute() {

        DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::Execute\n");
        
        //set the connection manual commits
        sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_NTS);

        if (SQL_SUCCEEDED(sqlReturnCode)) {
          return;
        } else {
          SetError("TODO");
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

    void OnError(Error &e) {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      // TODO: Get error and return it
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
};

/*
 *  ODBCConnection::BeginTransaction (Async)
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransactionSync'.
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

  REQ_FUN_ARG(0, callback);
  
  /*
  if (!data) {
    Napi::LowMemoryNotification();
    Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
    return env.Null();
  }
  */

  BeginTransactionAsyncWorker *worker = new BeginTransactionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::BeginTransactionSync
 * 
 *    Description: TODO
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
    this->m_hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_OFF,
    SQL_NTS);
  
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    return Napi::Boolean::New(env, true);

  } else {
    // TODO: Get error

    //Local<Value> objError = ODBC::GetSQLError(SQL_HANDLE_DBC, conn->m_hDBC);
  
    //Nan::ThrowError(objError);
    
    return Napi::Boolean::New(env, false);
  }
}

/******************************************************************************
 ***************************** END TRANSACTION ********************************
 *****************************************************************************/

 // EndTransactionAsyncWorker, used by EndTransaction function (see below)
class EndTransactionAsyncWorker : public Napi::AsyncWorker {

  public:
    EndTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, SQLSMALLINT completionType, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      completionType(completionType) {}

    ~EndTransactionAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::Execute\n");
      
      //Call SQLEndTran
      sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC, completionType);
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {

        //Reset the connection back to autocommit
        sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
        
        if (SQL_SUCCEEDED(sqlReturnCode)) {
          return;
        } else {
          SetError("TODO");
        }
      } else {
        SetError("TODO");
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

    void OnError(Error &e) {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;

      // TODO:
      Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_DBC, odbcConnectionObject->m_hDBC);
      callbackArguments.push_back(error);

      Callback().Call(callbackArguments);
    }

  private:
    ODBCConnection *odbcConnectionObject;
    SQLSMALLINT completionType;
    SQLRETURN sqlReturnCode;
};

/*
 *  ODBCConnection::EndTransaction (Async)
 * 
 *    Description: TODO
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed from the JavaSript environment, including the
 *        function arguments for 'endTransactionSync'.
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

  REQ_BOOL_ARG(0, rollback);
  REQ_FUN_ARG(1, callback);
  
  // if (!data) {
  //   Napi::LowMemoryNotification();
  //   Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
  //   return env.Null();
  // }

  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;

  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, completionType, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCConnection::EndTransactionSync
 * 
 *    Description: TODO
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

  REQ_BOOL_ARG(0, rollback);
  
  // if (!data) {
  //   Napi::LowMemoryNotification();
  //   Napi::Error::New(env, "Could not allocate enough memory").ThrowAsJavaScriptException();
  //   return env.Null();
  // }

  SQLRETURN sqlReturnCode;
  SQLSMALLINT completionType = rollback.Value() ? SQL_ROLLBACK : SQL_COMMIT;

  //Call SQLEndTran
  sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, this->m_hDBC, completionType);
  
  if (SQL_SUCCEEDED(sqlReturnCode)) {

    //Reset the connection back to autocommit
    sqlReturnCode = SQLSetConnectAttr(this->m_hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);
    
    if (SQL_SUCCEEDED(sqlReturnCode)) {
      return Napi::Boolean::New(env, true);

    } else {
      // TODO: Throw Error
      return Napi::Boolean::New(env, false);
    }
  } else {
    // TODO: Throw Error
    return Napi::Boolean::New(env, false);
  }
}