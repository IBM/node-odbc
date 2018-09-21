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
*/

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

using namespace Napi;

Napi::FunctionReference ODBCResult::constructor;
Napi::String ODBCResult::OPTION_FETCH_MODE;

HENV ODBCResult::m_hENV;
HDBC ODBCResult::m_hDBC;
HSTMT ODBCResult::m_hSTMT;
bool ODBCResult::m_canFreeHandle;

int fetchMode;

Napi::Object ODBCResult::Init(Napi::Env env, Napi::Object exports) {

  DEBUG_PRINTF("ODBCResult::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCResult", {

    InstanceMethod("fetch", &ODBCResult::Fetch),
    InstanceMethod("fetchSync", &ODBCResult::FetchSync),

    InstanceMethod("fetchAll", &ODBCResult::FetchAll),
    InstanceMethod("fetchAllSync", &ODBCResult::FetchAllSync),

    //InstanceMethod("moreResults", &ODBCResult::MoreResults),
    InstanceMethod("moreResultsSync", &ODBCResult::MoreResultsSync),

    //InstanceMethod("getColumnNames", &ODBCResult::GetColumnNames),
    InstanceMethod("getColumnNamesSync", &ODBCResult::GetColumnNamesSync),

    //InstanceMethod("getRowCount", &ODBCResult::GetRowCount),
    InstanceMethod("getRowCountSync", &ODBCResult::GetRowCountSync),

    InstanceMethod("close", &ODBCResult::Close),
    InstanceMethod("closeSync", &ODBCResult::CloseSync),

    InstanceAccessor("fetchMode", &ODBCResult::FetchModeGetter, &ODBCResult::FetchModeSetter)
  });
  
  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCResult", constructorFunction);

  return exports;
}

ODBCResult::ODBCResult(const Napi::CallbackInfo& info)  : Napi::ObjectWrap<ODBCResult>(info) {

  this->data = info[4].As<Napi::External<QueryData>>().Data();
  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  this->m_hSTMT = data->hSTMT;
  this->m_canFreeHandle = info[3].As<Napi::Boolean>().Value();

  // default fetchMode to FETCH_OBJECT
  this->fetchMode = FETCH_OBJECT;

  DEBUG_PRINTF("ODBCResult::New m_hDBC=%X m_hDBC=%X m_hSTMT=%X canFreeHandle=%X\n",
   this->m_hENV,
   this->m_hDBC,
   this->m_hSTMT,
   this->m_canFreeHandle
  );
}

ODBCResult::~ODBCResult() {
  DEBUG_PRINTF("ODBCResult::~ODBCResult\n");
  DEBUG_PRINTF("ODBCResult::~ODBCResult m_hSTMT=%x\n", m_hSTMT);
  this->Free();
}

SQLRETURN ODBCResult::Free() {
  DEBUG_PRINTF("ODBCResult::Free\n");
  DEBUG_PRINTF("ODBCResult::Free m_hSTMT=%X m_canFreeHandle=%X\n", m_hSTMT, m_canFreeHandle);

  SQLRETURN sqlReturnCode = SQL_SUCCESS;

  ODBC::FreeColumns(this->data->columns, &this->data->columnCount);

  if (this->m_hSTMT && this->m_canFreeHandle) {

    uv_mutex_lock(&ODBC::g_odbcMutex);
    sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, this->m_hSTMT);
    this->m_hSTMT = NULL;
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  return sqlReturnCode;
}

Napi::Value ODBCResult::FetchModeGetter(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Number::New(env, this->fetchMode);
}

void ODBCResult::FetchModeSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  printf("Setting fetch mode!\n");
  if (value.IsNumber()) {
    this->fetchMode = value.As<Napi::Number>().Int32Value();
  }
}

/******************************************************************************
 ********************************** FETCH *************************************
 *****************************************************************************/

// FetchAsyncWorker, used by Fetch function (see below)
class FetchAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAsyncWorker(ODBCResult *odbcResultObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAsyncWorker() {
      //delete data; 

    }

    void Execute() {

      DEBUG_PRINTF("\nODBCCursor::FetchAsyncWorker::Execute");

      DEBUG_PRINTF("ODBCCursor::FetchAll : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);
      
      //Only loop through the recordset if there are columns
      if (data->columnCount > 0) {
        ODBC::Fetch(data);
      }

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        SetError("error");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCConnection::FetchAsyncWorker::OnOk : data->sqlReturnCode=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, data->fetchMode);

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

    void OnError(Error &error) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCResult::FetchAsyncWorker"));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCResult *odbcResultObject;
    QueryData *data;
};

/*
 *  ODBCResult::Fetch (Async)
 *    Description: Fetches the next result row from the statement that
 *                 produced this ODBCResult.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetch() function takes one or two arguments.
 * 
 *        info[0]: Number: [OPTIONAL] the fetchMode (return row as array or
 *                         object)
 *        info[1]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a problem getting results,
 *                     or null if operation was successful.
 *              result: The array of rows
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback).
 */
Napi::Value ODBCResult::Fetch(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();

  Napi::Function callback;

  if (info.Length() == 1 && info[0].IsFunction()) {
    callback = info[0].As<Napi::Function>();
  } else if (info.Length() == 2 && info[0].IsObject() && info[1].IsFunction()) {
    callback = info[1].As<Napi::Function>();

    Napi::Object obj = info[0].ToObject();
    Napi::String fetchModeKey = OPTION_FETCH_MODE;

    if (obj.Has(fetchModeKey) && obj.Get(fetchModeKey).IsNumber()) {
      this->data->fetchMode = obj.Get(fetchModeKey).ToNumber().Int32Value();
    }
  }

  FetchAsyncWorker *worker = new FetchAsyncWorker(this, this->data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCResult::FetchSync
 *    Description: Fetches the next result row from the statement that
 *                 produced this ODBCResult.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetchSync() function takes one optional argument.
 * 
 *        info[0]: Number: the fetchMode (return rows as arrays or objects)
 * 
 *    Return:
 *      Napi::Value:
 *        An Array or Object that is the returned row.
 */
Napi::Value ODBCResult::FetchSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("\nODBCResult::FetchSync");


  Napi::Env env = info.Env();

  if (info.Length() == 1 && info[0].IsObject()) {
    Napi::Object obj = info[0].ToObject();
    Napi::String fetchModeKey = OPTION_FETCH_MODE;

    if (obj.Has(fetchModeKey) && obj.Get(fetchModeKey).IsNumber()) {
      this->fetchMode = obj.Get(fetchModeKey).ToNumber().Int32Value();
    }
  }
      
  //Only loop through the recordset if there are columns
  if (data->columnCount > 0) {
    ODBC::Fetch(data);
  }

  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *)"[node-odbc] Error in ODBCResult::FetcgSync")).ThrowAsJavaScriptException();
    return env.Null();
  }

  return ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, this->fetchMode);
}

/******************************************************************************
 ******************************** FETCH ALL ***********************************
 *****************************************************************************/

// FetchAllAsyncWorker, used by FetchAll function (see below)
class FetchAllAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAllAsyncWorker(ODBCResult *odbcResultObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAllAsyncWorker() {}

    void Execute() {

      //Only loop through the recordset if there are columns
      if (data->columnCount > 0) {
        ODBC::FetchAll(data);
      }

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("Was an error in fetching");
        SetError("ERROR");
      }
    }

    void OnOK() {

      printf("FetchAllAsyncWorker::OnOk\n");

      DEBUG_PRINTF("ODBCResult::UV_AfterFetchAll\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      
      Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, odbcResultObject->fetchMode);
          
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);

      Callback().Call(callbackArguments);
    }

    void OnError(const Error &e) {

      printf("THERE WAS AN ERROR IN FETCH ALL!!");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT);

      callbackArguments.push_back(error);
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCResult *odbcResultObject;
    QueryData *data;
};

/*
 *  ODBCResult::FetchAll (Async)
 *    Description: Fetches all of the (remaining) result rows from the
 *                 statment that produced this ODBCResult.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetchAll() function takes one or two arguments.
 * 
 *        info[0]: Number: [OPTIONAL] the fetchMode (return rows as arrays or
 *                         objects)
 *        info[1]: Function: callback function:
 *            function(error, result)
 *              error: An error object if there was a problem getting results,
 *                     or null if operation was successful.
 *              result: The array of rows
 * 
 *    Return:
 *      Napi::Value:
 *        Undefined (results returned in callback).
 */
Napi::Value ODBCResult::FetchAll(const Napi::CallbackInfo& info) {

  printf("FetchAll\n");
  DEBUG_PRINTF("ODBCResult::FetchAll\n");

  Napi::Env env = Env();
  Napi::HandleScope scope(env);
  
  Napi::Function callback;
  
  if (info.Length() == 1 && info[0].IsFunction()) {
    callback = info[0].As<Napi::Function>();
  }
  else if (info.Length() == 2 && info[0].IsObject() && info[1].IsFunction()) {
    callback = info[1].As<Napi::Function>();  
    
    Napi::Object obj = info[0].ToObject();

    Napi::String fetchModeKey = Napi::String::New(env, OPTION_FETCH_MODE.Utf8Value());
    if (obj.Has(fetchModeKey) && obj.Get(fetchModeKey).IsNumber()) {
      data->fetchMode = obj.Get(fetchModeKey).As<Napi::Number>().Int32Value();

    }
  }
  else {
    Napi::TypeError::New(env, "ODBCResult::FetchAll(): 1 or 2 arguments are required. The last argument must be a callback function.").ThrowAsJavaScriptException();
  }

  FetchAllAsyncWorker *worker = new FetchAllAsyncWorker(this, this->data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCResult::FetchAllSync
 *    Description: Fetches all of the (remaining) result rows from the
 *                 statment that produced this ODBCResult.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        fetchAllSync() function takes one optional argument.
 * 
 *        info[0]: Number: the fetchMode (return rows as arrays or objects)
 * 
 *    Return:
 *      Napi::Value:
 *        An Array of Arrays or Objects that are the returned rows.
 */
Napi::Value ODBCResult::FetchAllSync(const Napi::CallbackInfo& info) {

  printf("FetchAllSync\n");

  DEBUG_PRINTF("\nODBCResult::FetchSync");

  Napi::Env env = info.Env();

  if (info.Length() == 1 && info[0].IsObject()) {
    Napi::Object obj = info[0].ToObject();
    Napi::String fetchModeKey = OPTION_FETCH_MODE;

    if (obj.Has(fetchModeKey) && obj.Get(fetchModeKey).IsNumber()) {
      this->data->fetchMode = obj.Get(fetchModeKey).ToNumber().Int32Value();
    }
  }
      
  //Only loop through the recordset if there are columns
  if (data->columnCount > 0) {
    ODBC::FetchAll(data);
  }

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    return ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, false);
  } else {
    Napi::Error(env, ODBC::GetSQLError(env,SQL_HANDLE_STMT, data->hSTMT, (char *)"[node-odbc] Error in ODBCResult::FetchAllSync")).ThrowAsJavaScriptException();
    return env.Null();
  }
}

/******************************************************************************
 ****************************** MORE RESULTS **********************************
 *****************************************************************************/

/*
 *  ODBCResult::MoreResultsSync
 *    Description: Checks to see if there are more results that can be fetched
 *                 from the statement that genereated this ODBCResult object.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        moreResults() function takes no arguments.
 * 
 *    Return:
 *      Napi::Value:
 *        A Boolean value that is true if there are more results.
 */
Napi::Value ODBCResult::MoreResultsSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::MoreResultsSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  SQLRETURN sqlReturnCode = SQLMoreResults(data->hSTMT);

  if (sqlReturnCode == SQL_ERROR) {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *)"[node-odbc] Error in ODBCResult::MoreResultsSync")).ThrowAsJavaScriptException();
  }

  return Napi::Boolean::New(env, SQL_SUCCEEDED(sqlReturnCode) ? true : false);
}

/******************************************************************************
 **************************** GET COLUMN NAMES ********************************
 *****************************************************************************/

/*
 *  ODBCResult::GetColumnNamesSync
 *    Description: Returns an array containing all of the column names of the
 *                 executed statement.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        getColumnNames() function takes no arguments.
 * 
 *    Return:
 *      Napi::Value:
 *        An Array that contains the names of the columns from the result.
 */
Napi::Value ODBCResult::GetColumnNamesSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::GetColumnNamesSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Array columnNames = Napi::Array::New(env);
  
  for (int i = 0; i < this->data->columnCount; i++) {

    #ifdef UNICODE
      columnNames.Set(Napi::Number::New(env, i),
                      Napi::String::New(env, (char16_t*) this->data->columns[i].name));
    #else
      columnNames.Set(Napi::Number::New(env, i),
                      Napi::String::New(env, (char*)this->data->columns[i].name));
    #endif
  }

    
  return columnNames;
}

/******************************************************************************
 ***************************** GET ROW COUNT **********************************
 *****************************************************************************/

/*
 *  ODBCResult::GetRowCountSync
 *    Description: Returns the number of rows affected by a INSERT, DELETE, or
 *                 UPDATE operation. Specific drivers may define behavior to
 *                 return number of rows affected by SELECT.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        getRowCount() function takes no arguments.
 * 
 *    Return:
 *      Napi::Value:
 *        A Number that is set to the number of affected rows
 */
Napi::Value ODBCResult::GetRowCountSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::GetRowCountSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLLEN rowCount = 0;

  SQLRETURN sqlReturnCode = SQLRowCount(data->hSTMT, &rowCount);

  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    rowCount = 0;
  }

  return Napi::Number::New(env, rowCount);
}

/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

// CloseAsyncWorker, used by Close function (see below)
class CloseAsyncWorker : public Napi::AsyncWorker {

  public:
    CloseAsyncWorker(ODBCResult *odbcResultObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCResult::CloseAsyncWorker::Execute\n");
      
      if (odbcResultObject->m_hDBC) {

        uv_mutex_lock(&ODBC::g_odbcMutex);

        sqlReturnCode = SQLDisconnect(odbcResultObject->m_hDBC);

        if (SQL_SUCCEEDED(sqlReturnCode)) {

          sqlReturnCode = SQLFreeHandle(SQL_HANDLE_DBC, odbcResultObject->m_hDBC);

          if (SQL_SUCCEEDED(sqlReturnCode)) {
            odbcResultObject->m_hDBC = NULL;
          } else {
            SetError("ERROR");
          }
        } else {
          SetError("ERROR");
        }
        
        uv_mutex_unlock(&ODBC::g_odbcMutex);
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCResult::CloseAsyncWorker::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      // errors are handled in OnError
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

    void OnError(Error& e) {

      DEBUG_PRINTF("ODBCResult::CloseAsyncWorker::OnError\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, odbcResultObject->m_hSTMT, (char *) "[node-odbc] Error in ODBCResult::CloseAsyncWorker Execute"));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    ODBCResult *odbcResultObject;
    SQLRETURN sqlReturnCode;
};

/*
 *  ODBCResult::Close
 *    Description: Closes the statement and potentially the database connection
 *                 depending on the permissions given to this object and the
 *                 parameters passed in.
 * 
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        getRowCount() function takes an optional integer argument the
 *        specifies the option passed to SQLFreeStmt.
 * 
 *    Return:
 *      Napi::Value:
 *        A Boolean that is true if the connection was correctly closed.
 */
Napi::Value ODBCResult::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::CloseAsync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  int closeOption = SQL_DESTROY;
  Napi::Function callback;

  if (info.Length() == 1) {
    callback = info[0].As<Napi::Function>();
  } else if (info.Length() == 2) {
    closeOption = info[0].As<Napi::Number>().Int32Value();
    callback = info[1].As<Napi::Function>();
  } else {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, this->m_hSTMT, (char *)"[node-odbc] Error in ODBCResult::Close")).ThrowAsJavaScriptException();
    return env.Null();
  }

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCResult::CloseSync
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
Napi::Value ODBCResult::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode;
  int closeOption = SQL_DESTROY;
  
  if (info.Length() == 1 && info[0].IsNumber()) {
    closeOption = info[0].As<Napi::Number>().Int32Value();
  }
  
  DEBUG_PRINTF("ODBCResult::CloseSync closeOption=%i m_canFreeHandle=%i\n", closeOption, this->m_canFreeHandle);
  
  if (closeOption == SQL_DESTROY && this->m_canFreeHandle) {
    sqlReturnCode = this->Free();
  } else if (closeOption == SQL_DESTROY && !this->m_canFreeHandle) {
    // We technically can't free the handle so, we'll SQL_CLOSE
    uv_mutex_lock(&ODBC::g_odbcMutex);
    sqlReturnCode = SQLFreeStmt(this->m_hSTMT, SQL_CLOSE);
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  } else {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    sqlReturnCode = SQLFreeStmt(this->m_hSTMT, closeOption);
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  if (sqlReturnCode == SQL_ERROR) {
    Napi::Error(env, ODBC::GetSQLError(env, SQL_HANDLE_STMT, this->m_hSTMT, (char *)"[node-odbc] Error in ODBCResult::CloseSync")).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  
  return Napi::Boolean::New(env, true);
}