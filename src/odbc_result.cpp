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

#include <string.h>
#include <napi.h>
#include <uv.h>
#include <time.h>

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

  // Properties
  // MI: Not sure what to do with this
  //OPTION_FETCH_MODE.Reset(Napi::String::New(env, "fetchMode"));
  
  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCResult", constructorFunction);

  return exports;
}

ODBCResult::ODBCResult(const Napi::CallbackInfo& info)  : Napi::ObjectWrap<ODBCResult>(info) {

  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  this->m_hSTMT = *(info[2].As<Napi::External<SQLHSTMT>>().Data());
  this->m_canFreeHandle = info[3].As<Napi::Boolean>().Value();
  
  this->data = info[4].As<Napi::External<QueryData>>().Data();

  // default fetchMode to FETCH_OBJECT
  this->fetchMode = FETCH_OBJECT;

  DEBUG_PRINTF("ODBCResult::New m_hDBC=%X m_hDBC=%X m_hSTMT=%X canFreeHandle=%X\n",
   this->m_hENV,
   this->m_hDBC,
   this->m_hSTMT,
   this->m_canFreeHandle
  );

  // this->columns = ODBC::GetColumns(this->m_hSTMT, &this->columnCount); // get new data
  // this->boundRow = ODBC::BindColumnData(this->m_hSTMT, this->columns, &this->columnCount); // bind columns
  // this->m_fetchMode = FETCH_ARRAY;
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

  return Napi::Number::New(env, this->data->fetchMode);
}

void ODBCResult::FetchModeSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (value.IsNumber()) {
    this->data->fetchMode = value.As<Napi::Number>().Int32Value();
  }
}

/******************************************************************************
 ********************************** FETCH *************************************
 *****************************************************************************/

class FetchAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAsyncWorker(QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
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

      Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, false);
      Napi::Object fields = ODBC::GetNapiColumns(env, data->columns, data->columnCount);
      Napi::Array parameters = ODBC::GetNapiParameters(env, data->params, data->paramCount);

      // the result object returned
      Napi::Object result = Napi::Object::New(env);
      result.Set(Napi::String::New(env, "rows"), rows);
      result.Set(Napi::String::New(env, "fields"), fields);
      result.Set(Napi::String::New(env, "parameters"), parameters);

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(result);

      Callback().Call(callbackArguments);
    }

    void OnError(Error &error) {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      //callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] Error in ODBCResult::FetchAsyncWorker"));
      callbackArguments.push_back(env.Null());

      Callback().Call(callbackArguments);
    }

  private:
    QueryData *data;
};

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

  FetchAsyncWorker *worker = new FetchAsyncWorker(this->data, callback);
  worker->Queue();

  return env.Undefined();
}

/* TODO: FetchSync */
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
    // TODO: Error
  }

  return ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, this->fetchMode);
}

/******************************************************************************
 ******************************** FETCH ALL ***********************************
 *****************************************************************************/

/*
 * FetchAll
 */
class FetchAllAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAllAsyncWorker(ODBCResult *odbcResultObject, QueryData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAllAsyncWorker() {}

    void Execute() {
      
      printf("FetchingAll: Col Count is %d", data->columnCount);

      //Only loop through the recordset if there are columns
      if (data->columnCount > 0) {

        // continue call SQLFetch, with results going in the boundRow array
        while(SQL_SUCCEEDED(SQLFetch(odbcResultObject->m_hSTMT))) {

          ColumnData *row = new ColumnData[data->columnCount];

          // Iterate over each column, putting the data in the row object
          // Don't need to use intermediate structure in sync version
          for (int i = 0; i < data->columnCount; i++) {

            row[i].size = data->columns[i].dataLength;
            row[i].data = new SQLCHAR[row[i].size];

            // check for null data
            if (row[i].size == SQL_NULL_DATA) {
              row[i].data = NULL;
              printf("\nDANGER");
            } else {
              switch(data->columns[i].type) {

                // TODO: Need to actually check the type of the column
                default:
                  memcpy(row[i].data, data->boundRow[i], row[i].size);
              }
            }
          }

          storedRows.push_back(row);
        }
      }
      printf("\nStored Rows %d", (int)storedRows.size());
    }

    void OnOK() {

      // COMMENT FROM HERE

      DEBUG_PRINTF("ODBCResult::UV_AfterFetchAll\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;

      //check to see if the result set has columns
      if (data->columnCount == 0) {
        //this most likely means that the query was something like
        //'insert into ....'

        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(env.Undefined());

        Callback().Call(callbackArguments);

      }
      //check to see if there was an error
      else if (data->sqlReturnCode == SQL_ERROR)  {

        data->errorCount++;

        data->objError = ODBC::GetSQLError(env,
            SQL_HANDLE_STMT, 
            odbcResultObject->m_hSTMT,
            (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll"
        );
      }
      else {

        Napi::Array rows = Napi::Array::New(env);
    
        for (unsigned int i = 0; i < storedRows.size(); i++) {

          Napi::Object row = Napi::Object::New(env);
          ColumnData *storedRow = storedRows[i];

          // Iterate over each column, putting the data in the row object
          // Don't need to use intermediate structure in sync version

          for (int j = 0; j < data->columnCount; j++) {

            Napi::Value value;

            // check for null data
            if (storedRow[j].size == SQL_NULL_DATA) {
              //value = env.Null();
            } else {
              switch(data->columns[j].type) {
                // Napi::Number
                case SQL_DECIMAL :
                case SQL_NUMERIC :
                case SQL_FLOAT :
                case SQL_REAL :
                case SQL_DOUBLE :
                case SQL_INTEGER :
                case SQL_SMALLINT :
                case SQL_BIGINT :
                  value = Napi::Number::New(env, atof((const char*)storedRow[j].data));
                  break;
                // Napi::ArrayBuffer
                case SQL_BINARY :
                case SQL_VARBINARY :
                case SQL_LONGVARBINARY :
                  value = Napi::ArrayBuffer::New(env, storedRow[j].data, storedRow[j].size);
                  break;
                // Napi::String (char16_t)
                case SQL_WCHAR :
                case SQL_WVARCHAR :
                case SQL_WLONGVARCHAR :
                  value = Napi::String::New(env, (const char16_t*)storedRow[j].data, storedRow[j].size);
                  break;
                // Napi::String (char)
                case SQL_CHAR :
                case SQL_VARCHAR :
                case SQL_LONGVARCHAR :
                default:
                  value = env.Null();
                  value = Napi::String::New(env, (const char*)storedRow[j].data, storedRow[j].size);
                  break;
              }

              row.Set(Napi::String::New(env, (const char*)data->columns[j].name), value);

            }

            delete storedRow[j].data;
            delete storedRow;
          }

          rows.Set(i, row);
        }

        Napi::Object columnNames = Napi::Object::New(env);
  
        for (int i = 0; i < data->columnCount; i++) {

          #ifdef UNICODE
            columnNames.Set(Napi::String::New(env, (char16_t*) data->columns[i].name),
                            Napi::Number::New(env, data->columns[i].type));
          #else
            columnNames.Set(Napi::String::New(env, (char*) data->columns[i].name),
                            Napi::Number::New(env, data->columns[i].type));
          #endif

        }

        rows.Set(Napi::String::New(env, "rowCount"), rows.Length());
        rows.Set(Napi::String::New(env, "fields"), columnNames);
            
        std::vector<napi_value> callbackArguments;

        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(rows);

        Callback().Call(callbackArguments);
      }

      delete data;
    }

  private:
    ODBCResult *odbcResultObject;
    QueryData *data;
    std::vector<ColumnData*> storedRows;
};

Napi::Value ODBCResult::FetchAll(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::FetchAll\n");

  Napi::Env env = Env();
  Napi::HandleScope scope(env);

  QueryData *data = new QueryData;
  
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
  
  data->errorCount = 0;
  // data->count = 0;

  FetchAllAsyncWorker *worker = new FetchAllAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/* TODO: FetchAllSync */
Napi::Value ODBCResult::FetchAllSync(const Napi::CallbackInfo& info) {

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

  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    // TODO: Error
  }

  Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, false);
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
 *    Return:
 *      Napi::Value:
 *        A Boolean value that is true if there are more results.
 */
Napi::Value ODBCResult::MoreResultsSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::MoreResultsSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  SQLRETURN sqlReturnCode = SQLMoreResults(this->m_hSTMT);

  if (sqlReturnCode == SQL_ERROR) {
    Napi::Value objError = ODBC::GetSQLError(env,
    	SQL_HANDLE_STMT, 
	    this->m_hSTMT, 
	    (char *)"[node-odbc] Error in ODBCResult::MoreResultsSync"
    );

    Napi::Error(env, objError).ThrowAsJavaScriptException();
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
 *    Return:
 *      Napi::Value:
 *        A Number that is set to the number of affected rows
 */
Napi::Value ODBCResult::GetRowCountSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::GetRowCountSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLLEN rowCount = 0;

  SQLRETURN sqlReturnCode = SQLRowCount(this->m_hSTMT, &rowCount);

  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    rowCount = 0;
  }

  return Napi::Number::New(env, rowCount);
}

/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

/*
 *  ODBCResult::CloseSync
 *    Description: Closes the statement and potentially the database connection
 *                 depending on the permissions given to this object and the
 *                 parameters passed in.
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        getRowCount() function takes an optional integer argument the
 *        specifies the option passed to SQLFreeStmt.
 *    Return:
 *      Napi::Value:
 *        A Boolean that is true if the connection was correctly closed.
 */
Napi::Value ODBCResult::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  OPT_INT_ARG(0, closeOption, SQL_DESTROY);

  SQLRETURN sqlReturnCode;
  
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
    Napi::Value objError = ODBC::GetSQLError(env,
      SQL_HANDLE_STMT, 
	    this->m_hSTMT, 
	    (char *)"[node-odbc] Error in ODBCResult::MoreResultsSync"
    );

    Napi::Error(env, objError).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  
  return Napi::Boolean::New(env, true);
}

/*
 * Close
 * 
 */
class CloseAsyncWorker : public Napi::AsyncWorker {

  public:
    CloseAsyncWorker(ODBCResult *odbcResultObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCResult::CloseAsyncWorker::Execute\n");
      
      //TODO: check to see if there are any open statements
      //on this connection
      
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

      callbackArguments.push_back(Napi::String::New(env, e.Message()));

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
 *    Parameters:
 *      const Napi::CallbackInfo& info:
 *        The information passed by Napi from the JavaScript call, including
 *        arguments from the JavaScript function. In JavaScript, the
 *        getRowCount() function takes an optional integer argument the
 *        specifies the option passed to SQLFreeStmt.
 *    Return:
 *      Napi::Value:
 *        A Boolean that is true if the connection was correctly closed.
 */
Napi::Value ODBCResult::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::CloseAsync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  REQ_FUN_ARG(1, callback);

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}