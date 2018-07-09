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
#include <node_version.h>
#include <time.h>
#include <assert.h>

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

Napi::Object ODBCResult::Init(Napi::Env env, Napi::Object exports) {

  DEBUG_PRINTF("ODBCResult::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCResult", {
    InstanceMethod("fetch", &ODBCResult::Fetch),
    InstanceMethod("fetchAll", &ODBCResult::FetchAll),
    //InstanceMethod("fetchSync", &ODBCResult::FetchSync),
    //InstanceMethod("fetchAllSync", &ODBCResult::FetchAllSync),
    InstanceMethod("moreResults", &ODBCResult::MoreResults),
    InstanceMethod("getColumnNames", &ODBCResult::GetColumnNames),
    InstanceMethod("getRowCount", &ODBCResult::GetRowCount),
    InstanceMethod("close", &ODBCResult::Close),

    InstanceAccessor("fetchMode", &ODBCResult::FetchModeGetter, &ODBCResult::FetchModeSetter)
  });

  // ODBCResult::m_hENV = hENV;
  // ODBCResult::m_hDBC = hDBC;
  // ODBCResult::m_hSTMT = hSTMT;
  // ODBCResult::m_canFreeHandle = canFreeHandle;

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

  DEBUG_PRINTF("ODBCResult::New m_hDBC=%X m_hDBC=%X m_hSTMT=%X canFreeHandle=%X\n",
   this->m_hENV,
   this->m_hDBC,
   this->m_hSTMT,
   this->m_canFreeHandle
  );

  this->columns = ODBC::GetColumns(this->m_hSTMT, &this->columnCount); // get new data
  this->boundRow = ODBC::BindColumnData(this->m_hSTMT, this->columns, &this->columnCount); // bind columns
  this->m_fetchMode = FETCH_ARRAY;
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

  ODBC::FreeColumns(this->columns, &this->columnCount);

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

  return Napi::Number::New(env, this->m_fetchMode);
}

void ODBCResult::FetchModeSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  if (value.IsNumber()) {
    this->m_fetchMode = value.As<Napi::Number>().Int32Value();
  }
}

/*
 * Fetch
 */

class FetchAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAsyncWorker(ODBCResult *odbcResultObject, FetchAsyncWorkerData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCResult::Fetch::Execute\n");

      data->result = SQLFetch(odbcResultObject->m_hSTMT);
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCResult::Fetch::OnOK\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Object row = Napi::Object::New(env);
      std::vector<napi_value> callbackArguments;
      
      // Iterate over each column, putting the data in the row object
      // Don't need to use intermediate structure in sync version
      for (int i = 0; i < odbcResultObject->columnCount; i++) {

        Napi::String columnName = Napi::String::New(env, (const char*)odbcResultObject->columns[i].name);
        Napi::Value value;

        // check for null data
        if (odbcResultObject->columns[i].dataLength == SQL_NULL_DATA) {
          value = env.Null();
        } else {
          switch(odbcResultObject->columns[i].type) {
            
            // Napi::Number
                case SQL_DECIMAL :
                case SQL_NUMERIC :
                case SQL_FLOAT :
                case SQL_REAL :
                case SQL_DOUBLE :
                case SQL_INTEGER :
                case SQL_SMALLINT :
                case SQL_BIGINT :
                  value = Napi::Number::New(env, atof((const char*)odbcResultObject->boundRow[i]));
                  break;
                // Napi::ArrayBuffer
                case SQL_BINARY :
                case SQL_VARBINARY :
                case SQL_LONGVARBINARY :
                  value = Napi::ArrayBuffer::New(env, odbcResultObject->boundRow[i], odbcResultObject->columns[i].dataLength);
                  break;
                // Napi::String (char16_t)
                case SQL_WCHAR :
                case SQL_WVARCHAR :
                case SQL_WLONGVARCHAR :
                  value = Napi::String::New(env, (const char16_t*)odbcResultObject->boundRow[i], odbcResultObject->columns[i].dataLength);
                  break;
                // Napi::String (char)
                case SQL_CHAR :
                case SQL_VARCHAR :
                case SQL_LONGVARCHAR :
                default:
                  value = env.Null();
                  value = Napi::String::New(env, (const char*)odbcResultObject->boundRow[i], odbcResultObject->columns[i].dataLength);
                  break;
          }
        }

        row.Set(columnName, value);
      }

      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(row);

      Callback().Call(callbackArguments);

      free(data);
    }

  private:
    ODBCResult *odbcResultObject;
    FetchAsyncWorkerData *data;
};

Napi::Value ODBCResult::Fetch(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::Fetch\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  FetchAsyncWorkerData *data = new FetchAsyncWorkerData;
  
  Napi::Function callback;
   
  //set the fetch mode to the default of this instance
  data->fetchMode = this->m_fetchMode;
  
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
    Napi::TypeError::New(env, "ODBCResult::Fetch(): 1 or 2 arguments are required. The last argument must be a callback function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  FetchAsyncWorker *worker = new FetchAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * FetchAll
 */

class FetchAllAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAllAsyncWorker(ODBCResult *odbcResultObject, FetchAsyncWorkerData *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAllAsyncWorker() {}

    void Execute() {
      
      printf("FetchingAll: Col Count is %d", odbcResultObject->columnCount);

      //Only loop through the recordset if there are columns
      if (odbcResultObject->columnCount > 0) {

        // continue call SQLFetch, with results going in the boundRow array
        while(SQL_SUCCEEDED(SQLFetch(odbcResultObject->m_hSTMT))) {

          ColumnData *row = new ColumnData[odbcResultObject->columnCount];

          // Iterate over each column, putting the data in the row object
          // Don't need to use intermediate structure in sync version
          for (int i = 0; i < odbcResultObject->columnCount; i++) {

            row[i].size = odbcResultObject->columns[i].dataLength;
            row[i].data = new SQLCHAR[row[i].size];

            // check for null data
            if (row[i].size == SQL_NULL_DATA) {
              row[i].data = NULL;
              printf("\nDANGER");
            } else {
              switch(odbcResultObject->columns[i].type) {

                // TODO: Need to actually check the type of the column
                default:
                  memcpy(row[i].data, odbcResultObject->boundRow[i], row[i].size);
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
      if (odbcResultObject->columnCount == 0) {
        //this most likely means that the query was something like
        //'insert into ....'

        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(env.Undefined());

        Callback().Call(callbackArguments);

      }
      //check to see if there was an error
      else if (data->result == SQL_ERROR)  {

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

          for (int j = 0; j < odbcResultObject->columnCount; j++) {

            Napi::Value value;

            // check for null data
            if (storedRow[j].size == SQL_NULL_DATA) {
              //value = env.Null();
            } else {
              switch(odbcResultObject->columns[j].type) {
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

              row.Set(Napi::String::New(env, (const char*)odbcResultObject->columns[j].name), value);

            }

            delete storedRow[j].data;
            delete storedRow;
          }

          rows.Set(i, row);
        }

        Napi::Array columnNames = Napi::Array::New(env);
  
        for (int i = 0; i < odbcResultObject->columnCount; i++) {
          
          printf("\nGETTING A COL NAME");
          #ifdef UNICODE
            columnNames.Set(Napi::Number::New(env, i),
                            Napi::String::New(env, (char16_t*) odbcResultObject->columns[i].name));
          #else
            columnNames.Set(Napi::Number::New(env, i),
                            Napi::String::New(env, (char*)odbcResultObject->columns[i].name));
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
    FetchAsyncWorkerData *data;
    std::vector<ColumnData*> storedRows;
};

Napi::Value ODBCResult::FetchAll(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::FetchAll\n");

  Napi::Env env = Env();
  Napi::HandleScope scope(env);

  FetchAsyncWorkerData *data = new FetchAsyncWorkerData;
  
  Napi::Function callback;
  
  data->fetchMode = this->m_fetchMode;
  
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
  data->count = 0;

  FetchAllAsyncWorker *worker = new FetchAllAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 *  ODBCResult::MoreResults
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
Napi::Value ODBCResult::MoreResults(const Napi::CallbackInfo& info) {
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

/*
 *  ODBCResult::GetColumnNames
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
Napi::Value ODBCResult::GetColumnNames(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::GetColumnNamesSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Array columnNames = Napi::Array::New(env);
  
  for (int i = 0; i < this->columnCount; i++) {
    #ifdef UNICODE
      columnNames.Set(Napi::Number::New(env, i),
                      Napi::String::New(env, (char16_t*) this->columns[i].name));
    #else
      columnNames.Set(Napi::Number::New(env, i),
                      Napi::String::New(env, (char*)this->columns[i].name));
    #endif

  }
    
  return columnNames;
}

/*
 *  ODBCResult::GetRowCount
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
Napi::Value ODBCResult::GetRowCount(const Napi::CallbackInfo& info) {

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

  DEBUG_PRINTF("ODBCResult::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  OPT_INT_ARG(0, closeOption, SQL_DESTROY);

  SQLRETURN sqlReturnCode;
  
  DEBUG_PRINTF("ODBCResult::CloseSync closeOption=%i m_canFreeHandle=%i\n", closeOption, this->m_canFreeHandle);
  
  if (closeOption == SQL_DESTROY && this->m_canFreeHandle) {
    sqlReturnCode = this->Free();
  }
  else if (closeOption == SQL_DESTROY && !this->m_canFreeHandle) {
    //We technically can't free the handle so, we'll SQL_CLOSE
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    sqlReturnCode = SQLFreeStmt(this->m_hSTMT, SQL_CLOSE);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  else {
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