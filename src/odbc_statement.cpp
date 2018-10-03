/*
  Copyright (c) 2013, Dan VerWeire<dverweire@gmail.com>

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
  
  SQL_ERROR Value is: -1
  SQL_SUCCESS Value is: 0
  SQL_SQL_SUCCESS_WITH_INFO Value is: 1
  SQL_NEED_DATA Value is: 99

*/

#include <napi.h>
#include <uv.h>
#include <time.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

Napi::FunctionReference ODBCStatement::constructor;

HENV ODBCStatement::m_hENV;
HDBC ODBCStatement::m_hDBC;
HSTMT ODBCStatement::m_hSTMT;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports) {

  DEBUG_PRINTF("ODBCStatement::Init\n");

  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCStatement", {

    InstanceMethod("executeDirect", &ODBCStatement::ExecuteDirect),
    InstanceMethod("executeDirectSync", &ODBCStatement::ExecuteDirectSync),

    InstanceMethod("executeNonQuery", &ODBCStatement::ExecuteNonQuery),
    InstanceMethod("executeNonQuerySync", &ODBCStatement::ExecuteNonQuerySync),

    InstanceMethod("prepare", &ODBCStatement::Prepare),
    InstanceMethod("prepareSync", &ODBCStatement::PrepareSync),
    
    InstanceMethod("bind", &ODBCStatement::Bind),
    InstanceMethod("bindSync", &ODBCStatement::BindSync),

    InstanceMethod("execute", &ODBCStatement::Execute),
    InstanceMethod("executeSync", &ODBCStatement::ExecuteSync),

    InstanceMethod("close", &ODBCStatement::Close),
    InstanceMethod("closeSync", &ODBCStatement::CloseSync)
  });

  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCStatement", constructorFunction);

  return exports;
}

ODBCStatement::ODBCStatement(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCStatement>(info) {

  this->data = new QueryData();

  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  this->data->hSTMT = *(info[2].As<Napi::External<SQLHSTMT>>().Data());
}

ODBCStatement::~ODBCStatement() {
  this->Free();
}

void ODBCStatement::Free() {

  DEBUG_PRINTF("ODBCStatement::Free\n");

  // delete data;
  
  if (m_hSTMT) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    SQLFreeHandle(SQL_HANDLE_STMT, m_hSTMT);
    m_hSTMT = NULL;
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

/******************************************************************************
 **************************** EXECUTE NON QUERY *******************************
 *****************************************************************************/

// ExecuteNonQueryAsyncWorker, used by ExecuteNonQuery function (see below)
class ExecuteNonQueryAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteNonQueryAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~ExecuteNonQueryAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteNonQueryAsyncWorker in Execute()\n");

      data->sqlReturnCode = SQLExecute(data->hSTMT);

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {

      } else {
        SetError("Error");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::AsyncWorkerExecuteNonQuery in OnOk()\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);
      
      SQLLEN rowCount = 0;

      data->sqlReturnCode = SQLRowCount(data->hSTMT, &rowCount);
      
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        rowCount = 0;
      }

      uv_mutex_lock(&ODBC::g_odbcMutex);
      SQLFreeStmt(data->hSTMT, SQL_CLOSE);
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());                       // callbackArguments[0]  
      callbackArguments.push_back(Napi::Number::New(env, rowCount)); // callbackArguments[1]
      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCStatement::AsyncWorkerExecuteNonQuery in OnError()\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::ExecuteNonQueryAsyncWorker"));
      callbackArguments.push_back(env.Undefined());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

/*
 *  ODBCStatement::ExecuteNonQuery (Async)
 *    Description: Executes a bound and prepared statement and returns only the
 *                 number of rows affected.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetchAll() function takes one argument.
 * 
 *        info[0]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a problem getting results,
 *                     or null if operation was successful.
 *              result: The number of rows affected by the executed query.
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback).
 */
Napi::Value ODBCStatement::ExecuteNonQuery(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteNonQuery\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if(!info[0].IsFunction()){
    Napi::Error::New(env, "Argument 1 Must be a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  ExecuteNonQueryAsyncWorker *worker = new ExecuteNonQueryAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCStatement::ExecuteNonQuerySync
 *    Description: Executes a bound and prepared statement and returns only the
 *                 number of rows affected.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        executeNonQuerySync() function takes no arguments.
 * 
 *    Return:
 *      Napi::Value:
 *        Number, the number of rows affected by the executed statement.
 */
Napi::Value ODBCStatement::ExecuteNonQuerySync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLLEN rowCount = 0;

  data->sqlReturnCode = SQLExecute(data->hSTMT);

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        
    data->sqlReturnCode = SQLRowCount(data->hSTMT, &rowCount);

    if (SQL_SUCCEEDED(data->sqlReturnCode)) {
      rowCount = 0;
    }

    // TODO: If we fail to free the statement, but still got the row count,
    // what should be done?
    uv_mutex_lock(&ODBC::g_odbcMutex);
    data->sqlReturnCode = SQLFreeStmt(data->hSTMT, SQL_CLOSE);
    uv_mutex_unlock(&ODBC::g_odbcMutex);

    return Napi::Number::New(env, rowCount);
  }
  
  return ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::PrepareAsyncWorker");
}

/******************************************************************************
 ****************************** EXECUTE DIRECT ********************************
 *****************************************************************************/

// ExecuteDirectAsyncWorker, used by ExecuteDirect function (see below)
class ExecuteDirectAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteDirectAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~ExecuteDirectAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("\nODBCConnection::QueryAsyncWorke::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);

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
      } else {
        SetError("Error");
      }
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i, data->noResultObject=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;


        //check to see if the result set has columns
        if (data->columnCount == 0) {
          //this most likely means that the query was something like
          //'insert into ....'

          callbackArguments.push_back(env.Null());
          callbackArguments.push_back(env.Undefined());

        } else {

          std::vector<napi_value> resultArguments;
          resultArguments.push_back(Napi::External<HENV>::New(env, &(odbcStatementObject->m_hENV)));
          resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcStatementObject->m_hDBC)));
          resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
          resultArguments.push_back(Napi::Boolean::New(env, false)); // canFreeHandle 

          resultArguments.push_back(Napi::External<QueryData>::New(env, data));

          // create a new ODBCResult object as a Napi::Value
          Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

          callbackArguments.push_back(env.Null());
          callbackArguments.push_back(resultObject);

      }

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCStatement::ExecuteDirectAsyncWorker in OnError()\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::ExecuteDirectAsyncWorker"));
      callbackArguments.push_back(env.Undefined());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

/*
 *  ODBCStatement::ExecuteDirect (Async)
 *    Description: Directly executes a statement without preparing it and
 *                 binding parameters.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetchAll() function takes one argument.
 * 
 *        info[0]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a problem getting results,
 *                     or null if operation was successful.
 *              result: The number of rows affected by the executed query.
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback).
 */
Napi::Value ODBCStatement::ExecuteDirect(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::ExecuteDirect\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  // REQ_STRO_ARG(0, sql);
  // REQ_FUN_ARG(1, callback);
  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 1 must be a string , Argument 2 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

  #ifdef UNICODE
    std::u16string tempString = sql.Utf16Value();
  #else
    std::string tempString = sql.Utf8Value();
  #endif
  std::vector<SQLTCHAR> *sqlVec = new std::vector<SQLTCHAR>(tempString.begin(), tempString.end());
  sqlVec->push_back('\0');
  data->sql = &(*sqlVec)[0];

  ExecuteDirectAsyncWorker *worker = new ExecuteDirectAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();

}

/*
 *  ODBCStatement::ExecuteDirectSync
 *    Description: Directly executes a statement without preparing it and
 *                 binding parameters.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        executeDirectSync() function takes one argument, an SQL string to
 *        execute.
 * 
 *    Return:
 *      Napi::Value:
 *        An ODBCResult object that can get results from the execution, or null
 *        if there was an error in the execution.
 */
Napi::Value ODBCStatement::ExecuteDirectSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "executeDirectSync takes one argument (string)").ThrowAsJavaScriptException();
  }

  Napi::String sql = info[0].ToString();

  #ifdef UNICODE
    std::u16string tempString = sql.Utf16Value();
  #else
    std::string tempString = sql.Utf8Value();
  #endif
  std::vector<SQLTCHAR> *sqlVec = new std::vector<SQLTCHAR>(tempString.begin(), tempString.end());
  sqlVec->push_back('\0');
  data->sql = &(*sqlVec)[0];

  data->sqlReturnCode = SQLExecDirect(
    this->m_hSTMT,
    data->sql, 
    SQL_NTS);  

  if(!SQL_SUCCEEDED(data->sqlReturnCode)) {
    Napi::Error objError = Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *)"[node-odbc] Error in ODBCStatement::ExecuteDirectSync"));
    objError.ThrowAsJavaScriptException();
    return env.Null();

  } else {
    // arguments for the ODBCResult constructor
    std::vector<napi_value> resultArguments;
    resultArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hENV)));
    resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
    resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
    resultArguments.push_back(Napi::Boolean::New(env, true)); // canFreeHandle 
    resultArguments.push_back(Napi::External<QueryData>::New(env, data));
    
    // create a new ODBCResult object as a Napi::Value
    Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

    return resultObject;
  }
}

/******************************************************************************
 ********************************* PREPARE ************************************
 *****************************************************************************/

// PrepareAsyncWorker, used by Prepare function (see below)
class PrepareAsyncWorker : public Napi::AsyncWorker {

  public:
    PrepareAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~PrepareAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in Execute()\n");
      
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       data->hSTMT
      );

      data->sqlReturnCode = SQLPrepare(
        data->hSTMT,
        data->sql, 
        SQL_NTS
      );

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        return;
      } else {
        SetError("ERROR");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in OnOk()\n");
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       data->hSTMT
      );

      Napi::Env env = Env();  
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(Napi::Boolean::New(env, true));
      Callback().Call(callbackArguments);

    }

    void OnError(const Napi::Error &e) {

      Napi::Env env = Env();  
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::PrepareAsyncWorker"));
      callbackArguments.push_back(Napi::Boolean::New(env, false));

      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

/*
 *  ODBCStatement:Prepare (Async)
 *    Description: Prepares an SQL string so that it can be bound with
 *                 parameters and then executed.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        prepare() function takes two arguments.
 * 
 *        info[0]: String: the SQL string to prepare.
 *        info[1]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a problem getting results,
 *                     or null if operation was successful.
 *              result: The number of rows affected by the executed query.
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback).
 */
Napi::Value ODBCStatement::Prepare(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::Prepare\n");

  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 0 must be a string , Argument 1 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

  #ifdef UNICODE
    std::u16string tempString = sql.Utf16Value();
  #else
    std::string tempString = sql.Utf8Value();
  #endif
  std::vector<SQLTCHAR> *sqlVec = new std::vector<SQLTCHAR>(tempString.begin(), tempString.end());
  sqlVec->push_back('\0');
  data->sql = &(*sqlVec)[0];

  PrepareAsyncWorker *worker = new PrepareAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCStatement::PrepareSync
 *    Description: Prepares an SQL string so that it can be bound with
 *                 parameters and then executed.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        prepareSync() function takes one argument.
 * 
 *        info[0]: String: the SQL string to prepare.
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, true if the prepare was successful, false if it wasn't
 */
Napi::Value ODBCStatement::PrepareSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String sql = info[0].ToString();

  #ifdef UNICODE
    std::u16string tempString = sql.Utf16Value();
  #else
    std::string tempString = sql.Utf8Value();
  #endif
  std::vector<SQLTCHAR> *sqlVec = new std::vector<SQLTCHAR>(tempString.begin(), tempString.end());
  sqlVec->push_back('\0');
  data->sql = &(*sqlVec)[0];

  data->sqlReturnCode = SQLPrepare(
    data->hSTMT,
    data->sql, 
    SQL_NTS
  ); 

  if(SQL_SUCCEEDED(data->sqlReturnCode)) {
    return Napi::Boolean::New(env, true);
  }
  else {
    //Napi::Error(env, ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
}

/******************************************************************************
 *********************************** BIND *************************************
 *****************************************************************************/

// BindAsyncWorker, used by Bind function (see below)
class BindAsyncWorker : public Napi::AsyncWorker {

  public:
    BindAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~BindAsyncWorker() { }

    void Execute() {   

      printf("BindAsyncWorker::Execute\n");

      if (data->paramCount > 0) {
        // binds all parameters to the query
        ODBC::BindParameters(data);
      }    
    }

    void OnOK() {

      printf("BindAsyncWorker::OnOk\n");

      DEBUG_PRINTF("ODBCStatement::UV_AfterBind\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(Napi::Boolean::New(env, true));

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      printf("BindAsyncWorker::OnError\n");

      DEBUG_PRINTF("ODBCStatement::UV_AfterBind\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::BindAsyncWorker"));
      callbackArguments.push_back(Napi::Boolean::New(env, false));

      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Bind(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::Bind\n");
  
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() || !info[1].IsFunction() ) {
    Napi::Error::New(env, "Argument 1 must be an Array").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array parameterArray = info[0].As<Napi::Array>();
  Napi::Function callback = info[1].As<Napi::Function>();

  this->data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));
  
  BindAsyncWorker *worker = new BindAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCStatement::BindSync
 *    Description: Binds values to a prepared statement so that it is ready
 *                 for execution.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        bindSync() function takes one argument, an array of values to bind.
 * 
 *    Return:
 *      Napi::Value:
 *        Boolean, true if the binding was successful, false if it wasn't
 */
Napi::Value ODBCStatement::BindSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::BindSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Array parameterArray = info[0].As<Napi::Array>();

  this->data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));

  if (data->paramCount > 0) {
    // binds all parameters to the query
    ODBC::BindParameters(data);
  }

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    return Napi::Boolean::New(env, true);
  } else {
    Napi::Error(env, ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->hSTMT)).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

}

/******************************************************************************
 ********************************* EXECUTE ************************************
 *****************************************************************************/

// ExecuteAsyncWorker, used by Execute function (see below)
class ExecuteAsyncWorker : public Napi::AsyncWorker {
  public:
    ExecuteAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcStatementObject(odbcStatementObject),
      data(odbcStatementObject->data) {}

    ~ExecuteAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::Execute()\n");

      data->sqlReturnCode = SQLExecute(data->hSTMT);

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        
        ODBC::BindColumns(data);

      } else {
        SetError("ERROR");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::OnOk()\n");  

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> resultArguments;

      resultArguments.push_back(Napi::External<HENV>::New(env, &(odbcStatementObject->m_hENV)));
      resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcStatementObject->m_hDBC)));
      resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
      resultArguments.push_back(Napi::Boolean::New(env, false)); // canFreeHandle 

      resultArguments.push_back(Napi::External<QueryData>::New(env, data));

      // create a new ODBCResult object as a Napi::Value
      Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(resultObject);

      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::OnError()\n");  

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::ExecuteAsyncWorker"));
      callbackArguments.push_back(Napi::Boolean::New(env, false));

      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Execute(const Napi::CallbackInfo& info) {
  
  DEBUG_PRINTF("ODBCStatement::Execute\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  REQ_FUN_ARG(0, callback); 

  ExecuteAsyncWorker *worker = new ExecuteAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCStatement::ExecuteSync
 *    Description: Executes a statement that has been prepared and potentially
 *                 bound.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        executeSync() function takes no arguments.
 * 
 *    Return:
 *      Napi::Value:
 *        An ODBCResult object that can get results from the execution, or null
 *        if there was an error in the execution.
 */
Napi::Value ODBCStatement::ExecuteSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::ExecuteSync()\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  data->sqlReturnCode = SQLExecute(data->hSTMT);

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    
    ODBC::BindColumns(data);

    std::vector<napi_value> resultArguments;
    resultArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hENV)));
    resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
    resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->hSTMT)));
    resultArguments.push_back(Napi::Boolean::New(env, false)); // canFreeHandle 

    resultArguments.push_back(Napi::External<QueryData>::New(env, data));

    // create a new ODBCResult object as a Napi::Value
    Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

    return resultObject;

  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::ExecuteSync")).ThrowAsJavaScriptException();
    return env.Null();
  }
}


/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

// CloseAsyncWorker, used by Close function (see below)
class CloseAsyncWorker : public Napi::AsyncWorker {
  public:
    CloseAsyncWorker(ODBCStatement *odbcStatementObject, int closeOption, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcStatementObject(odbcStatementObject),
      closeOption(closeOption),
      data(odbcStatementObject->data) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      if (closeOption == SQL_DESTROY) {
        odbcStatementObject->Free();
      } else {
        uv_mutex_lock(&ODBC::g_odbcMutex);
        data->sqlReturnCode = SQLFreeStmt(odbcStatementObject->m_hSTMT, closeOption);
        uv_mutex_unlock(&ODBC::g_odbcMutex);
      }

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        return;
      } else {
        SetError("Error");
      }

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::Execute()\n");
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::OnOk()\n");  

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }

    void OnError(const Napi::Error &e) {

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::OnError()\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCStatement::CloseAsyncWorker"));
      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    int closeOption;
    QueryData *data;
};

Napi::Value ODBCStatement::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::Close\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (info.Length() != 2 || !info[0].IsNumber() || !info[1].IsFunction()) {
    Napi::TypeError::New(env, "close takes two arguments (closeOption [int], callback [function])").ThrowAsJavaScriptException();
  }

  int closeOption = info[0].ToNumber().Int32Value();
  Napi::Function callback = info[1].As<Napi::Function>(); 

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, closeOption, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCStatement::CloseSync
 *    Description: Closes the statement and potentially the database connection
 *                 depending on the permissions given to this object and the
 *                 parameters passed in.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        closeSync() function takes an optional integer argument the
 *        specifies the option passed to SQLFreeStmt.
 * 
 *    Return:
 *      Napi::Value:
 *        A Boolean that is true if the connection was correctly closed.
 */
Napi::Value ODBCStatement::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  
  DEBUG_PRINTF("ODBCStatement::CloseSync closeOption=%i\n", closeOption);
  
  if (closeOption == SQL_DESTROY) {
    this->Free();
  } else {
    
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    data->sqlReturnCode = SQLFreeStmt(this->m_hSTMT, closeOption);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    return Napi::Boolean::New(env, true);
  } else {
    return Napi::Boolean::New(env, false);
  }
}