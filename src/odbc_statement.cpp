/*
  Copyright (c) 2019, 2021 IBM
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
*/

#include <napi.h>
#include <time.h>
#include <string>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"

Napi::FunctionReference ODBCStatement::constructor;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports) {
  DEBUG_PRINTF("ODBCStatement::Init\n");

  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCStatement", {
    InstanceMethod("prepare", &ODBCStatement::Prepare),
    InstanceMethod("bind", &ODBCStatement::Bind),
    InstanceMethod("execute", &ODBCStatement::Execute),
    InstanceMethod("close", &ODBCStatement::Close),
  });

  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  return exports;
}


ODBCStatement::ODBCStatement(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCStatement>(info) {
  this->data = new QueryData();
  this->odbcConnection = info[0].As<Napi::External<ODBCConnection>>().Data();
  this->data->hSTMT = *(info[1].As<Napi::External<SQLHSTMT>>().Data());
}

ODBCStatement::~ODBCStatement() {
  this->Free();
  delete data;
  data = NULL;
}

SQLRETURN ODBCStatement::Free() {
  DEBUG_PRINTF("ODBCStatement::Free\n");

  if (this->data && this->data->hSTMT && this->data->hSTMT != SQL_NULL_HANDLE) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    this->data->sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, this->data->hSTMT);
    this->data->hSTMT = SQL_NULL_HANDLE;
    // data->clear();
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  // TODO: Actually fix this
  return SQL_SUCCESS;
}

/******************************************************************************
 ********************************* PREPARE ************************************
 *****************************************************************************/

// PrepareAsyncWorker, used by Prepare function (see below)
class PrepareAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCStatement *odbcStatement;
    ODBCConnection *odbcConnection;
    QueryData *data;

  public:
    PrepareAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
    odbcStatement(odbcStatement),
    odbcConnection(odbcStatement->odbcConnection),
    data(odbcStatement->data) {}

    ~PrepareAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): Running SQLPrepare(StatementHandle = %p, StatementText = %s, TextLength = %d)\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->hSTMT, data->sql, SQL_NTS);
      data->sqlReturnCode = SQLPrepare(
        data->hSTMT, // StatementHandle
        data->sql,   // StatementText
        SQL_NTS      // TextLength
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLPrepare returned code %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error preparing the statement\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLPrepare succeeded: SQLRETURN = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);

      // front-load the work of SQLNumParams and SQLDescribeParam here, so we
      // can convert NAPI/JavaScript values to C values immediately in Bind
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): Running SQLNumParams(StatementHandle = %p, ParameterCountPtr = %p\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->hSTMT, &data->parameterCount);
      data->sqlReturnCode = SQLNumParams(
        data->hSTMT,          // StatementHandle
        &data->parameterCount // ParameterCountPtr
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLNumParams FAILED: SQLRETURN = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving number of parameter markers to be bound to the statement\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLNumParams succeeded: SQLRETURN = %d, ParameterCount = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode, data->parameterCount);

      data->parameters = new Parameter*[data->parameterCount];
      for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
        data->parameters[i] = new Parameter();
      }

      // data->sqlReturnCode = ODBC::DescribeParameters(data->hSTMT, data->parameters, data->parameterCount);
      // if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      //   DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): DescribeParameters returned code %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
      //   this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
      //   SetError("[odbc] Error retrieving information about the parameters in the statement\0");
      //   return;
      // }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::OnOk()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }
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
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::Prepare()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 0 must be a string , Argument 1 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

  if(this->data->hSTMT == SQL_NULL_HANDLE) {
    Napi::Error error = Napi::Error::New(env, "Statment handle is no longer valid. Cannot prepare SQL on an invalid statment handle.");
    std::vector<napi_value> callbackArguments;
    callbackArguments.push_back(error.Value());
    callback.Call(callbackArguments);
    return env.Undefined();
  }

  data->sql = ODBC::NapiStringToSQLTCHAR(sql);

  PrepareAsyncWorker *worker = new PrepareAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 *********************************** BIND *************************************
 *****************************************************************************/

// BindAsyncWorker, used by Bind function (see below)
class BindAsyncWorker : public ODBCAsyncWorker {

  public:

    BindAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
    odbcStatement(odbcStatement),
    odbcConnection(odbcStatement->odbcConnection),
    data(odbcStatement->data) {}

  private:

    ODBCStatement *odbcStatement;
    ODBCConnection *odbcConnection;
    QueryData *data;

    ~BindAsyncWorker() { }

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::BindAsyncWorker::Execute()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      // // front-load the work of SQLNumParams and SQLDescribeParam here, so we
      // // can convert NAPI/JavaScript values to C values immediately in Bind
      // DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): Running SQLNumParams(StatementHandle = %p, ParameterCountPtr = %p\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->hSTMT, &data->parameterCount);
      // data->sqlReturnCode = SQLNumParams(
      //   data->hSTMT,          // StatementHandle
      //   &data->parameterCount // ParameterCountPtr
      // );
      // if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      //   DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLNumParams FAILED: SQLRETURN = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
      //   this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
      //   SetError("[odbc] Error retrieving number of parameter markers to be bound to the statement\0");
      //   return;
      // }
      // DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): SQLNumParams succeeded: SQLRETURN = %d, ParameterCount = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode, data->parameterCount);

      // data->parameters = new Parameter*[data->parameterCount];
      // for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
      //   data->parameters[i] = new Parameter();
      // }

      data->sqlReturnCode = ODBC::DescribeParameters(data->hSTMT, data->parameters, data->parameterCount);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::PrepareAsyncWorker::Execute(): DescribeParameters returned code %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving information about the parameters in the statement\0");
        return;
      }

      data->sqlReturnCode = ODBC::BindParameters(data->hSTMT, data->parameters, data->parameterCount);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::BindAsyncWorker::Execute(): BindParameters returned code %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error binding parameters to the statement\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::BindAsyncWorker::OnOk()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCStatement::Bind(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::Bind()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

  Napi::Env env = info.Env(); 
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() || !info[1].IsFunction() ) {
    Napi::TypeError::New(env, "Function signature is: bind(array, function)").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array napiArray = info[0].As<Napi::Array>();
  this->napiParameters = Napi::Persistent(napiArray);
  Napi::Function callback = info[1].As<Napi::Function>();

  if(this->data->hSTMT == SQL_NULL_HANDLE) {
    Napi::Error error = Napi::Error::New(env, "Statment handle is no longer valid. Cannot bind SQL on an invalid statment handle.");
    std::vector<napi_value> callbackArguments;
    callbackArguments.push_back(error.Value());
    callback.Call(callbackArguments);
    return env.Undefined();
  }

  // if the parameter count isnt right, end right away
  if (data->parameterCount != (SQLSMALLINT)this->napiParameters.Value().Length() || data->parameters == NULL) {
    std::vector<napi_value> callbackArguments;

    Napi::Error error = Napi::Error::New(env, Napi::String::New(env, "[node-odbc] Error in Statement::BindAsyncWorker::Bind: The number of parameters in the prepared statement (" + std::to_string(data->parameterCount) + ") doesn't match the number of parameters passed to bind (" + std::to_string((SQLSMALLINT)this->napiParameters.Value().Length()) + "}."));
    callbackArguments.push_back(error.Value());

    callback.Call(callbackArguments);
    return env.Undefined();
  }

  // converts NAPI/JavaScript values to values used by SQLBindParameter
  ODBC::StoreBindValues(&napiArray, this->data->parameters);

  BindAsyncWorker *worker = new BindAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ********************************* EXECUTE ************************************
 *****************************************************************************/

// ExecuteAsyncWorker, used by Execute function (see below)
class ExecuteAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCConnection *odbcConnection;
    ODBCStatement *odbcStatement;
    QueryData *data;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::Execute()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::Execute(): Running SQLExecute(StatementHandle = %p)\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->hSTMT);
      data->sqlReturnCode = SQLExecute(
        data->hSTMT // StatementHandle
      );
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::Execute(): SQLExecute FAILED: SQLRETURN = %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error executing the statement\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::Execute(): SQLExecute succeeded\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      data->sqlReturnCode = this->odbcConnection->RetrieveResultSet(data);
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::Execute(): RetrieveResultSet returned %d\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error retrieving the result from the statement\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::ExecuteAsyncWorker::OnOk()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows = odbcConnection->ProcessDataForNapi(env, data, &odbcStatement->napiParameters);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

  public:
    ExecuteAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnection(odbcStatement->odbcConnection),
      odbcStatement(odbcStatement),
      data(odbcStatement->data) {}

    ~ExecuteAsyncWorker() {}
};

Napi::Value ODBCStatement::Execute(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::Execute()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback;

  // ensuring the passed parameters are correct
  if (info.Length() == 1 && info[0].IsFunction()) {
    callback = info[0].As<Napi::Function>();
  } else {
    Napi::TypeError::New(env, "execute: first argument must be a function").ThrowAsJavaScriptException();
    return(env.Undefined());
  }

  if(this->data->hSTMT == SQL_NULL_HANDLE) {
    Napi::Error error = Napi::Error::New(env, "Statment handle is no longer valid. Cannot execute SQL on an invalid statment handle.");
    std::vector<napi_value> callbackArguments;
    callbackArguments.push_back(error.Value());
    callback.Call(callbackArguments);
    return env.Undefined();
  }

  ExecuteAsyncWorker *worker = new ExecuteAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

// CloseStatementAsyncWorker, used by Close function (see below)
class CloseStatementAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCStatement *odbcStatement;
    QueryData *data;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::CloseAsyncWorker::Execute()\n", odbcStatement->odbcConnection->hENV, odbcStatement->odbcConnection->hDBC, data->hSTMT);

      data->sqlReturnCode = odbcStatement->Free();
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][[SQLHDBC: %p] ODBCStatement::CloseAsyncWorker::Execute(): SQLFreeHandle returned %d\n", odbcStatement->odbcConnection->hENV, odbcStatement->odbcConnection->hDBC, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error closing the Statement\0");
        return;
      }
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCStatement::CloseStatementAsyncWorker::OnOk()\n", odbcStatement->odbcConnection->hENV, odbcStatement->odbcConnection->hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }

  public:
    CloseStatementAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcStatement(odbcStatement),
      data(odbcStatement->data) {}

    ~CloseStatementAsyncWorker() {}
};

Napi::Value ODBCStatement::Close(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCStatement::Close()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CloseStatementAsyncWorker *worker = new CloseStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}
