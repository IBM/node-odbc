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
#include "odbc_cursor.h"

Napi::FunctionReference ODBCStatement::constructor;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports) {

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
  this->data = new StatementData();
  this->odbcConnection = info[0].As<Napi::External<ODBCConnection>>().Data();
  this->data->hstmt = *(info[1].As<Napi::External<SQLHSTMT>>().Data());
  this->data->fetch_array         = this->odbcConnection->connectionOptions.fetchArray;
  this->data->maxColumnNameLength = this->odbcConnection->getInfoResults.max_column_name_length;
  this->data->get_data_supports   = this->odbcConnection->getInfoResults.sql_get_data_supports;
}

ODBCStatement::~ODBCStatement() {
  this->Free();
}

SQLRETURN ODBCStatement::Free() {

  SQLRETURN return_code =  SQL_SUCCESS;

  if (this->data)
  {
    if (
    this->data->hstmt &&
    this->data->hstmt != SQL_NULL_HANDLE
    )
    {
      uv_mutex_lock(&ODBC::g_odbcMutex);
      return_code =
      SQLFreeHandle
      (
        SQL_HANDLE_STMT,
        this->data->hstmt
      );
      this->data->hstmt = SQL_NULL_HANDLE;
      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    delete this->data;
    this->data = NULL;
  }

  return return_code;
}

/******************************************************************************
 ********************************* PREPARE ************************************
 *****************************************************************************/

// PrepareAsyncWorker, used by Prepare function (see below)
class PrepareAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCStatement  *odbcStatement;
    ODBCConnection *odbcConnection;
    StatementData  *data;

  public:
    PrepareAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
    odbcStatement(odbcStatement),
    odbcConnection(odbcStatement->odbcConnection),
    data(odbcStatement->data) {}

    ~PrepareAsyncWorker() {}

    void Execute() {

      SQLRETURN return_code;

      return_code = SQLPrepare
      (
        data->hstmt, // StatementHandle
        data->sql,   // StatementText
        SQL_NTS      // TextLength
      );
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error preparing the statement\0");
        return;
      }

      // front-load the work of SQLNumParams here, so we can convert
      // NAPI/JavaScript values to C values immediately in Bind
      return_code = SQLNumParams
      (
        data->hstmt,          // StatementHandle
        &data->parameterCount // ParameterCountPtr
      );
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error retrieving number of parameter markers to be bound to the statement\0");
        return;
      }

      data->parameters = new Parameter*[data->parameterCount];
      for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
        data->parameters[i] = new Parameter();
      }
    }

    void OnOK() {

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

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  this->napiParameters = Napi::Persistent(Napi::Array::New(env));

  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 0 must be a string , Argument 1 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

  if(this->data->hstmt == SQL_NULL_HANDLE) {
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

    ODBCStatement  *odbcStatement;
    ODBCConnection *odbcConnection;
    StatementData  *data;

    ~BindAsyncWorker() { }

    void Execute() {

      SQLRETURN return_code;

      return_code = ODBC::DescribeParameters(data->hstmt, data->parameters, data->parameterCount);
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error retrieving information about the parameters in the statement\0");
        return;
      }

      return_code = ODBC::BindParameters(data->hstmt, data->parameters, data->parameterCount);
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error binding parameters to the statement\0");
        return;
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCStatement::Bind(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env(); 
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() || !info[1].IsFunction() ) {
    Napi::TypeError::New(env, "Function signature is: bind(array, function)").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array napiArray = info[0].As<Napi::Array>();
  this->napiParameters = Napi::Persistent(napiArray);
  Napi::Function callback = info[1].As<Napi::Function>();

  if(this->data->hstmt == SQL_NULL_HANDLE) {
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
    ODBCStatement  *odbcStatement;
    StatementData  *data;

    void Execute() {

      SQLRETURN return_code;

      // set SQL_ATTR_QUERY_TIMEOUT
      if (data->query_options.timeout > 0) {
        return_code =
        SQLSetStmtAttr
        (
          data->hstmt,
          SQL_ATTR_QUERY_TIMEOUT,
          (SQLPOINTER) data->query_options.timeout,
          IGNORED_PARAMETER
        );

        // It is possible that SQLSetStmtAttr returns a warning with SQLSTATE
        // 01S02, indicating that the driver changed the value specified.
        // Although we never use the timeout variable again (and so we don't
        // REALLY need it to be correct in the code), its just good to have
        // the correct value if we need it.
        if (return_code == SQL_SUCCESS_WITH_INFO)
        {
          return_code =
          SQLGetStmtAttr
          (
            data->hstmt,
            SQL_ATTR_QUERY_TIMEOUT,
            (SQLPOINTER) &data->query_options.timeout,
            SQL_IS_UINTEGER,
            IGNORED_PARAMETER
          );
        }

        // Both of the SQL_ATTR_QUERY_TIMEOUT calls are combined here
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
          SetError("[odbc] Error setting the query timeout on the statement\0");
          return;
        }
      }

      return_code =
      set_fetch_size
      (
        data,
        data->query_options.fetch_size
      );
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error setting the fetch size\0");
        return;
      }

      return_code =
      SQLExecute
      (
        data->hstmt // StatementHandle
      );
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error executing the statement\0");
        return;
      }

      if (return_code != SQL_NO_DATA) {

        if (data->query_options.use_cursor)
        {
          if (data->query_options.cursor_name != NULL)
          {
            return_code =
            SQLSetCursorName
            (
              data->hstmt,
              data->query_options.cursor_name,
              data->query_options.cursor_name_length
            );

            if (!SQL_SUCCEEDED(return_code)) {
              this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
              SetError("[odbc] Error setting the cursor name on the statement\0");
              return;
            }
          }
        }

        // set_fetch_size will swallow errors in the case that the driver
        // doesn't implement SQL_ATTR_ROW_ARRAY_SIZE for SQLSetStmtAttr and
        // the fetch size was 1. If the fetch size was set by the user to a
        // value greater than 1, throw an error.
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
          SetError("[odbc] Error setting the fetch size on the statement\0");
          return;
        }

        return_code =
        prepare_for_fetch
        (
          data
        );
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
          SetError("[odbc] Error preparing for fetch\0");
          return;
        }


        if (!data->query_options.use_cursor)
        {
          bool alloc_error = false;
          return_code =
          fetch_all_and_store
          (
            data,
            true,
            &alloc_error
          );
          if (alloc_error)
          {
            SetError("[odbc] Error allocating or reallocating memory when fetching data. No ODBC error information available.\0");
            return;
          }
          if (!SQL_SUCCEEDED(return_code)) {
            this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
            SetError("[odbc] Error retrieving the result set from the statement\0");
            return;
          }
        }
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      if (data->query_options.use_cursor)
      {
        // arguments for the ODBCCursor constructor
        std::vector<napi_value> cursor_arguments =
        {
          Napi::External<StatementData>::New(env, data),
          Napi::External<ODBCConnection>::New(env, this->odbcConnection),
          this->odbcStatement->napiParameters.Value(),
          Napi::Boolean::New(env, false)
        };
  
        // create a new ODBCCursor object as a Napi::Value
        Napi::Value cursorObject = ODBCCursor::constructor.New(cursor_arguments);

        // return cursor
        std::vector<napi_value> callbackArguments =
        {
          env.Null(),
          cursorObject
        };

        Callback().Call(callbackArguments);
      }
      else
      {
        Napi::Array rows = process_data_for_napi(env, data, odbcStatement->napiParameters.Value());

        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(rows);

        Callback().Call(callbackArguments);
      }

      return;
    }

  public:
    ExecuteAsyncWorker(ODBCStatement *odbcStatement, Napi::Function& callback) : ODBCAsyncWorker(callback),
      odbcConnection(odbcStatement->odbcConnection),
      odbcStatement(odbcStatement),
      data(odbcStatement->data) {}

    ~ExecuteAsyncWorker() {}
};

Napi::Value ODBCStatement::Execute(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback;

  size_t argument_count = info.Length();
  // ensuring the passed parameters are correct
  if ((argument_count == 1 && info[0].IsFunction()) || (argument_count == 2 && info[1].IsFunction())) {
    callback = info[argument_count - 1].As<Napi::Function>();
  }

  Napi::Value error;

  // ensuring the passed parameters are correct
  if (argument_count >= 1 && info[0].IsObject()) {
    error =
    parse_query_options
    (
      env,
      info[0].As<Napi::Object>(),
      &this->data->query_options
    );
  } else {
    error =
    parse_query_options
    (
      env,
      env.Null(),
      &this->data->query_options
    );
  }

  if (!error.IsNull())
  {
    // Error when parsing the query options. Return the callback with the error
    std::vector<napi_value> callback_argument =
    {
      error
    };
    callback.Call(callback_argument);
  }

  if(this->data->hstmt == SQL_NULL_HANDLE) {
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
    StatementData *data;

    void Execute() {

      SQLRETURN return_code;

      return_code = odbcStatement->Free();
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error closing the Statement\0");
        return;
      }
    }

    void OnOK() {

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

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CloseStatementAsyncWorker *worker = new CloseStatementAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}
