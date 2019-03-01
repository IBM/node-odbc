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

    InstanceAccessor("isConnected", &ODBCConnection::ConnectedGetter, nullptr),
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

  return Napi::Boolean::New(env, this->autocommit);
}

ODBCConnection::ODBCConnection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCConnection>(info) {

  this->hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());

  this->connectionTimeout = 0;
  this->loginTimeout = 5;

  this->isConnected = true;
  this->autocommit = true;
}

ODBCConnection::~ODBCConnection() {

  DEBUG_PRINTF("ODBCConnection::~ODBCConnection\n");
  SQLRETURN sqlReturnCode;

  this->Free();

  this->isConnected = false;
}

SQLRETURN ODBCConnection::Free() {

  SQLRETURN returnCode = SQL_SUCCESS;

  DEBUG_PRINTF("ODBCConnection::Free\n");

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

// /******************************************************************************
//  ****************************** GET ATTRIBUTE *********************************
//  *****************************************************************************/

// // GetAttributeAsyncWorker, used by GetAttribute function (see below)
// class GetAttributeAsyncWorker : public Napi::AsyncWorker {

//   public:
//   GetAttributeAsyncWorker(ODBCConnection *odbcConnectionObject, SQLINTEGER attribute, Napi::Function& callback) : Napi::AsyncWorker(callback),
//     attribute(attribute),
//     odbcConnectionObject(odbcConnectionObject) {}

//   ~GetAttributeAsyncWorker() {}

//   private:
//   ODBCConnection *odbcConnectionObject;
//   SQLINTEGER attribute;
//   char buf[1024];
//   SQLINTEGER sLen = 0;
//   SQLPOINTER valuePtr;
//   SQLRETURN sqlReturnCode;

//   void Execute() {

//     DEBUG_PRINTF("ODBCConnection::GetAttribute::Execute\n");

//     valuePtr = (char*)&buf;

//     sqlReturnCode = SQLGetConnectAttr(odbcConnectionObject->hDBC, // ConnectionHandle
//                                       attribute,                  // Attribute
//                                       valuePtr,                   // ValuePtr
//                                       sizeof(buf),                // BufferLength
//                                       &sLen);                     // StringLengthPtr

//     if (!SQL_SUCCEEDED(sqlReturnCode)) {
//       SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::GetAttributeAsyncWorker calling SQLGetConnectAttr"));
//     }
//   }

//   void OnOK() {

//     DEBUG_PRINTF("ODBCConnection::GetAttribute::OnOK\n");

//     Napi::Env env = Env();
//     Napi::HandleScope scope(env);

//     Napi::Value attributeValue;

//     // TODO: What about string data being returned?
//     if (sqlReturnCode == SQL_NO_DATA) {
//       attributeValue = env.Null();
//     } else {
//       attributeValue = Napi::Number::New(env, *(int*)valuePtr);
//     }

//     std::vector<napi_value> callbackArguments;
//     callbackArguments.push_back(env.Null());
//     callbackArguments.push_back(attributeValue);

//     Callback().Call(callbackArguments);
//   }
// };

// /*
//  *  ODBCConnection::GetAttribute (Async)
//  * 
//  *    Description: Get an attribute from the connection asynchronously by
//  *                 calling SQLGetConnectAttr.
//  * 
//  *    Parameters:
//  * 
//  *      const Napi::CallbackInfo& info:
//  *        The information passed by Napi from the JavaScript call, including
//  *        arguments from the JavaScript function.  
//  *   
//  *         info[0]: Function: callback function, in the following format:
//  *            function(error)
//  *              error: An error object if there was an error calling SQL. 
//  * 
//  *    Return:
//  *      Napi::Value:
//  *        Undefined. (The return values are attached to the callback function).
//  */
// Napi::Value ODBCConnection::GetAttribute(const Napi::CallbackInfo& info) {

//   DEBUG_PRINTF("ODBCConnection::GetAttribute\n");

//   Napi::Env env = info.Env();
//   Napi::HandleScope scope(env);

//   SQLINTEGER attribute = Napi::Number(env, info[0]).Int32Value();
//   Napi::Function callback = info[1].As<Napi::Function>();

//   GetAttributeAsyncWorker *worker = new GetAttributeAsyncWorker(this, attribute, callback);
//   worker->Queue();

//   return env.Undefined();
// }



// Napi::Value ODBCConnection::SetAttribute(const Napi::CallbackInfo& info) {
//   Napi::Env env = info.Env();
//   Napi::HandleScope scope(env);

//   SQLINTEGER attribute = Napi::Number(env, info[0]).Int32Value();
//   char* cValue;
//   SQLINTEGER sLen = 0;
//   SQLRETURN sqlReturnCode = -1;
//   //check if the second arg was a Number or a String  
//   if(info[1].IsNumber() )
//   {
//     int param = Napi::Number(env, info[1]).Int32Value();
//     // Doc https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_73/cli/rzadpfnsconx.htm
//     sqlReturnCode = SQLSetConnectAttr(this->hDBC, //SQLHDBC hdbc -Connection Handle
//                                       attribute, //SQLINTEGER fAttr -Connection Attr to Set
//                                       &param, //SQLPOINTER vParam -Value for fAttr
//                                       0); //SQLINTEGER sLen -Length of input value (if string)
//   }
//   else if(info[1].IsString())
//   {
//     std::string arg1 = Napi::String(env , info[1]).Utf8Value();
//     std::vector<char> newCString(arg1.begin(), arg1.end());
//     newCString.push_back('\0');
//     cValue = &newCString[0];
//     sLen = strlen(cValue);
//     sqlReturnCode = SQLSetConnectAttr(this->hDBC, attribute, cValue, sLen);
//   }
//   if(sqlReturnCode != SQL_SUCCESS){
//     // SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, hDBC, (char *) "[node-odbc] Error in ODBCConnection::SetAttribute"));
//     return env.Null();
//   }
//   return Napi::Boolean::New(env, 1);
// }

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

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::Execute\n");

      sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, SQL_ROLLBACK);

      sqlReturnCode = odbcConnectionObject->Free();

      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::CloseAsyncWorker"));
        return;
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::CloseAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      odbcConnectionObject->isConnected = false;

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

  DEBUG_PRINTF("ODBCConnection::Close\n");

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
class CreateStatementAsyncWorker : public Napi::AsyncWorker {

  private:
    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;
    HSTMT hSTMT;

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::CreateStatementAsyncWorker:Execute - hDBC=%X hDBC=%X\n",
       odbcConnectionObject->hENV,
       odbcConnectionObject->hDBC,
      );

      uv_mutex_lock(&ODBC::g_odbcMutex);
      sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &hSTMT);
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        printf("\nERROR CREATING STATEMENT IN EXECUTE");
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::CreateStatementAsyncWorker"));
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

  public:
    CreateStatementAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
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

  DEBUG_PRINTF("ODBCConnection::CreateStatement\n");

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
class QueryAsyncWorker : public Napi::AsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData      *data;

    void Execute() {

      DEBUG_PRINTF("\nODBCConnection::QueryAsyncWorke::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
      
      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      data->sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_STMT,
        odbcConnectionObject->hDBC,
        &(data->hSTMT)
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker::Execute"));
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

        if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
          //printf("\nSQLRETURN is %d", data->sqlReturnCode);
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
          return;
        }

        data->sqlReturnCode = SQLNumParams(
          data->hSTMT,
          &data->parameterCount
        );

        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in Statement::PrepareAsyncWorker::Execute"));
          return;
        }

        if (data->parameterCount != data->bindValueCount) {
          SetError("[node-odbc] The number of parameters in the statement does not equal the number of bind values passed to the function.");
          return;
        }

        data->sqlReturnCode = ODBC::DescribeParameters(data->hSTMT, data->parameters, data->parameterCount);

        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in Statement::PrepareAsyncWorker::Execute"));
        }

        data->sqlReturnCode = ODBC::BindParameters(data->hSTMT, data->parameters, data->parameterCount);

        if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
          //printf("\nSQLRETURN is %d", data->sqlReturnCode);
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
          return;
        }

        data->sqlReturnCode = SQLExecute(data->hSTMT);

        if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
          //printf("\nSQLRETURN is %d", data->sqlReturnCode);
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
          return;
        }

      } 
      // querying without parameters, can just execdirect
      else {
        data->sqlReturnCode = SQLExecDirect(
          data->hSTMT,
          data->sql,
          SQL_NTS
        );

        if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
          SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
          return;
        }
      }

      SQLRowCount(data->hSTMT, &data->rowCount);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
        return;
      }

      ODBC::BindColumns(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
        return;
      }

      ODBC::FetchAll(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
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

  public:
    QueryAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~QueryAsyncWorker() {
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

  DEBUG_PRINTF("ODBCConnection::Query\n");

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
class CallProcedureAsyncWorker : public Napi::AsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData      *data;

    void Execute() {

      DEBUG_PRINTF("\nODBCConnection::CallProcedureAsyncWorker::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
      
      // allocate a new statement handle
      uv_mutex_lock(&ODBC::g_odbcMutex);
      data->sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &(data->hSTMT));
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::CallProcedureAsyncWorker::Execute"));
      }

      // TODO: Check here against tables()/columns() to see how they handle null
      data->sqlReturnCode = SQLProcedureColumns(  
        data->hSTMT, // StatementHandle
        NULL,        // CatalogName
        SQL_NTS,     // NameLengh1
        (SQLTCHAR*)"MARK\0",    // SchemaName
        SQL_NTS,     // NameLength2
        (SQLTCHAR*)"MAXBAL\0",   // ProcName
        SQL_NTS,     // NameLength3
        NULL,        // ColumnName
        SQL_NTS      // NameLength4
      );

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::CallProcedureAsyncWorker when calling SQLProcedureColumns."));
        return;
      }

      SQLRowCount(data->hSTMT, &data->rowCount);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::CallProcedureAsyncWorker"));
        return;
      }

      ODBC::BindColumns(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::CallProcedureAsyncWorker"));
        return;
      }

      ODBC::FetchAll(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::CallProcedureAsyncWorker"));
        return;
      }

      data->parameterCount = data->storedRows.size();

      // BindParameter now using INOUT found in data->storedRows[i][4].data, check against SQL_PARAM_INPUT etc. Might also have to build bindData up from other columns
      Parameter *parameters = new Parameter[data->parameterCount];

      // get stored column parameter data from the result set
      for (int i = 0; i < data->parameterCount; i++) {
        parameters[i].InputOutputType = *(SQLSMALLINT*)data->storedRows[i][4].data;
        parameters[i].ParameterType = *(SQLSMALLINT*)data->storedRows[i][5].data; // DataType -> ParameterType
        parameters[i].ColumnSize = *(SQLSMALLINT*)data->storedRows[i][7].data; // ParameterSize -> ColumnSize
        parameters[i].DecimalDigits = *(SQLSMALLINT*)data->storedRows[i][9].data;
        parameters[i].Nullable = *(SQLSMALLINT*)data->storedRows[i][11].data;
      }

      data->sqlReturnCode = ODBC::BindParameters(data->hSTMT, data->parameters, data->parameterCount);

      // create the statement to call the stored procedure using the ODBC Call escape sequence:
      SQLTCHAR callString[255];
      // need to create the string "?,?,?,?" where the number of '?' is the number of parameters;
      SQLTCHAR parameterString[(data->parameterCount * 2) - 1];
      // TODO: Can maybe add this for loop to the one above.
      for (int i = 0; i < data->parameterCount; i++) {
        if (i == (data->parameterCount - 1)) {
          strcat((char *)parameterString, "?"); // for last parameter, don't add ','
        } else {
          strcat((char *)parameterString, "?,");
        }
      }

      sprintf((char *)callString, "{call %s (?)}", data->sql);

      data->sqlReturnCode = SQLExecDirect(
        data->hSTMT,
        callString,
        SQL_NTS
      );

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("\nError in SQLExecDirect");
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
        return;
      }

      // TODO: Calling these twice is going to cause a memory leak! Need to reevaluate...

      SQLRowCount(data->hSTMT, &data->rowCount);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("\nError in SQLRowCount");
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
        return;
      }

      ODBC::BindColumns(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("\nError in ODBC::BindColumns");
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
        return;
      }

      ODBC::FetchAll(data);

      if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("\nError in FetchAll");
        SetError(ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::QueryAsyncWorker"));
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

  public:
    CallProcedureAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~CallProcedureAsyncWorker() {
      delete data;
    }
};

/*  TODO: Change
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
Napi::Value ODBCConnection::CallProcedure(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("\nODBCConnection::CallProcedure");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData();
  std::vector<Napi::Value> values;

  // check if parameters were passed or not
  if (info.Length() == 3 && info[0].IsString() && info[1].IsArray() && info[2].IsFunction()) {
    Napi::Array parameterArray = info[1].As<Napi::Array>();
    data->bindValueCount = parameterArray.Length();
    data->parameters = new Parameter*[data->bindValueCount]();
    for (SQLSMALLINT i = 0; i < data->bindValueCount; i++) {
      data->parameters[i] = new Parameter();
    }
    ODBC::StoreBindValues(&parameterArray, data->parameters);
  } else if ((info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) || (info.Length() == 3 && info[0].IsString() && info[1].IsNull() && info[2].IsFunction())) {
    data->parameters = 0;
  } else {
    Napi::TypeError::New(env, "[node-odbc]: Wrong function signature in call to Connection.callProcedure({string}, {array}[optional], {function}).").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[info.Length() - 1].As<Napi::Function>();

  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

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

  DEBUG_PRINTF("ODBCConnection::GetUsername\n");

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

  Napi::Error(env, Napi::String::New(env, ODBC::GetSQLError(SQL_HANDLE_DBC, this->hDBC, (char *) "[node-odbc] Error in ODBCConnection::GetInfo"))).ThrowAsJavaScriptException();
  return env.Null();
}

/******************************************************************************
 ********************************** TABLES ************************************
 *****************************************************************************/

// TablesAsyncWorker, used by Tables function (see below)
class TablesAsyncWorker : public Napi::AsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData *data;

    void Execute() {

      uv_mutex_lock(&ODBC::g_odbcMutex);
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT );     
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::TablesAsyncWorker"));
        return;
      } 
      
      data->sqlReturnCode = SQLTables( 
        data->hSTMT, 
        data->catalog, SQL_NTS, 
        data->schema, SQL_NTS, 
        data->table, SQL_NTS, 
        data->type, SQL_NTS
      );

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::TablesAsyncWorker"));
        return;
      }

      // SQLRowCount(data->hSTMT, &data->rowCount);
      
      ODBC::BindColumns(data);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::TablesAsyncWorker"));
        return;
      } 

      ODBC::FetchAll(data);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::TablesAsyncWorker"));
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

  public:

    TablesAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
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
class ColumnsAsyncWorker : public Napi::AsyncWorker {

  public:

    ColumnsAsyncWorker(ODBCConnection *odbcConnectionObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject),
      data(data) {}

    ~ColumnsAsyncWorker() {
      delete data;
    }

  private:

    ODBCConnection *odbcConnectionObject;
    QueryData *data;

    void Execute() {
 
      uv_mutex_lock(&ODBC::g_odbcMutex);
      SQLAllocHandle(SQL_HANDLE_STMT, odbcConnectionObject->hDBC, &data->hSTMT );
      uv_mutex_unlock(&ODBC::g_odbcMutex);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::ColumnsAsyncWorker");
        return;
      }
      
      data->sqlReturnCode = SQLColumns( 
        data->hSTMT, 
        data->catalog, SQL_NTS, 
        data->schema, SQL_NTS, 
        data->table, SQL_NTS, 
        data->column, SQL_NTS
      );

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::ColumnsAsyncWorker");
        return;
      }

      // SQLRowCount(data->hSTMT, &data->rowCount);

      ODBC::BindColumns(data);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::ColumnsAsyncWorker");
        return;
      }

      ODBC::FetchAll(data);

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCConnection::ColumnsAsyncWorker");
        return;
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
class BeginTransactionAsyncWorker : public Napi::AsyncWorker {

  public:

    BeginTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcConnectionObject(odbcConnectionObject) {}

    ~BeginTransactionAsyncWorker() {}

  private:

    ODBCConnection *odbcConnectionObject;
    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::Execute\n");
      
      //set the connection manual commits
      sqlReturnCode = SQLSetConnectAttr(
        odbcConnectionObject->hDBC,
        SQL_ATTR_AUTOCOMMIT,
        (SQLPOINTER) SQL_AUTOCOMMIT_OFF,
        SQL_NTS
      );

      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::BeginTransactionAsyncWorker"));
        return;
      }

      odbcConnectionObject->autocommit = false;
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::BeginTransactionAsyncWorker::OnOK\n");

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

  DEBUG_PRINTF("ODBCConnection::BeginTransaction\n");

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
class EndTransactionAsyncWorker : public Napi::AsyncWorker {

  private:

    ODBCConnection *odbcConnectionObject;
    SQLSMALLINT completionType;
    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::Execute\n");
      
      sqlReturnCode = SQLEndTran(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, completionType);
      
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::EndTransactionAsyncWorker"));
        return;
      }

      //Reset the connection back to autocommit
      sqlReturnCode = SQLSetConnectAttr(odbcConnectionObject->hDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_NTS);

      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, odbcConnectionObject->hDBC, (char *) "[node-odbc] Error in ODBCConnection::EndTransactionAsyncWorker"));
        return;
      }

      odbcConnectionObject->autocommit = true;
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::EndTransactionAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      std::vector<napi_value> callbackArguments;
      
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  public:

    EndTransactionAsyncWorker(ODBCConnection *odbcConnectionObject, SQLSMALLINT completionType, Napi::Function& callback) : Napi::AsyncWorker(callback),
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

  DEBUG_PRINTF("ODBCConnection::Commit\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env, "[node-odbc]: commit(callback): first argument must be a function").ThrowAsJavaScriptException();
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

  DEBUG_PRINTF("ODBCConnection::Rollback\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env, "[node-odbc]: rollback(callback): first argument must be a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  // calls EndTransactionAsyncWorker with SQL_ROLLBACK option
  EndTransactionAsyncWorker *worker = new EndTransactionAsyncWorker(this, SQL_ROLLBACK, callback);
  worker->Queue();

  return env.Undefined();
}
