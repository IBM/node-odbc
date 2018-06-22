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

Napi::Object ODBCResult::Init(Napi::Env env, Napi::Object exports, HENV hENV, HDBC hDBC, HSTMT hSTMT, bool canFreeHandle) {

  DEBUG_PRINTF("ODBCResult::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCResult", {
    InstanceMethod("fetchAll", &ODBCResult::FetchAll),
    InstanceMethod("fetch", &ODBCResult::Fetch),

    InstanceMethod("moreResultsSync", &ODBCResult::MoreResultsSync),
    InstanceMethod("closeSync", &ODBCResult::CloseSync),
    InstanceMethod("fetchSync", &ODBCResult::FetchSync),
    InstanceMethod("fetchAllSync", &ODBCResult::FetchAllSync),
    InstanceMethod("getColumnNamesSync", &ODBCResult::GetColumnNamesSync),
    InstanceMethod("getRowCountSync", &ODBCResult::GetRowCountSync),

    InstanceAccessor("fetchMode", &ODBCResult::FetchModeGetter, &ODBCResult::FetchModeSetter)
  });

  ODBCResult::m_hENV = hENV;
  ODBCResult::m_hDBC = hDBC;
  ODBCResult::m_hSTMT = hSTMT;
  ODBCResult::m_canFreeHandle = canFreeHandle;

  // Properties
  // MI: Not sure what to do with this
  //OPTION_FETCH_MODE.Reset(Napi::String::New(env, "fetchMode"));
  
  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCResult", constructorFunction);

  return exports;
}

ODBCResult::ODBCResult(const Napi::CallbackInfo& info)  : Napi::ObjectWrap<ODBCResult>(info) {}

ODBCResult::~ODBCResult() {
  // DEBUG_PRINTF("ODBCResult::~ODBCResult\n");
  // //DEBUG_PRINTF("ODBCResult::~ODBCResult m_hSTMT=%x\n", m_hSTMT);
  // this->Free();
}

void ODBCResult::Free() {
  DEBUG_PRINTF("ODBCResult::Free\n");
  //DEBUG_PRINTF("ODBCResult::Free m_hSTMT=%X m_canFreeHandle=%X\n", m_hSTMT, m_canFreeHandle);

  if (m_hSTMT && m_canFreeHandle) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle( SQL_HANDLE_STMT, m_hSTMT);
    
    m_hSTMT = NULL;
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  
  if (bufferLength > 0) {
    bufferLength = 0;
    free(buffer);
  }
}

Napi::Value ODBCResult::New(const Napi::CallbackInfo& info) {
  // DEBUG_PRINTF("ODBCResult::New\n");
  // Napi::HandleScope scope(env);
  
  // REQ_EXT_ARG(0, js_henv);
  // REQ_EXT_ARG(1, js_hdbc);
  // REQ_EXT_ARG(2, js_hstmt);
  // REQ_EXT_ARG(3, js_canFreeHandle);
  
  // HENV hENV = static_cast<HENV>(js_henv->Value());
  // HDBC hDBC = static_cast<HDBC>(js_hdbc->Value());
  // HSTMT hSTMT = static_cast<HSTMT>(js_hstmt->Value());
  // bool* canFreeHandle = static_cast<bool *>(js_canFreeHandle->Value());
  
  // //create a new OBCResult object
  // ODBCResult* objODBCResult = new ODBCResult(hENV, hDBC, hSTMT, *canFreeHandle);
  
  // DEBUG_PRINTF("ODBCResult::New\n");
  // //DEBUG_PRINTF("ODBCResult::New m_hDBC=%X m_hDBC=%X m_hSTMT=%X canFreeHandle=%X\n",
  // //  objODBCResult->m_hENV,
  // //  objODBCResult->m_hDBC,
  // //  objODBCResult->m_hSTMT,
  // //  objODBCResult->m_canFreeHandle
  // //);
  
  // //free the pointer to canFreeHandle
  // delete canFreeHandle;

  // //specify the buffer length
  // objODBCResult->bufferLength = MAX_VALUE_SIZE - 1;
  
  // //initialze a buffer for this object
  // objODBCResult->buffer = (uint16_t *) malloc(objODBCResult->bufferLength + 1);
  // //TODO: make sure the malloc succeeded

  // //set the initial colCount to 0
  // objODBCResult->colCount = 0;

  // //default fetchMode to FETCH_OBJECT
  // objODBCResult->m_fetchMode = FETCH_OBJECT;
  
  // objODBCResult->Wrap(info.Holder());
  
  // return info.Holder();
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
    FetchAsyncWorker(ODBCResult *odbcResultObject, fetch_work_data *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCResult::UV_Fetch\n");

      data->result = SQLFetch(data->objResult->m_hSTMT);
    }

    void OnOK() {
      // DEBUG_PRINTF("ODBCResult::UV_AfterFetch\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);
      
      SQLRETURN ret = data->result;
      //TODO: we should probably define this on the work data so we
      //don't have to keep creating it?
      Napi::Value objError;
      bool moreWork = true;
      bool error = false;
      
      if (data->objResult->colCount == 0) {
        data->objResult->columns = ODBC::GetColumns(
          data->objResult->m_hSTMT, 
          &data->objResult->colCount);
      }
      
      //check to see if the result has no columns
      if (data->objResult->colCount == 0) {
        //this means
        moreWork = false;
      }
      //check to see if there was an error
      else if (ret == SQL_ERROR)  {
        moreWork = false;
        error = true;
        
        objError = ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->objResult->m_hSTMT, "Error in ODBCResult::UV_AfterFetch");
      }
      //check to see if we are at the end of the recordset
      else if (ret == SQL_NO_DATA) {
        moreWork = false;
      }

      std::vector<napi_value> callbackArguments;

      if (moreWork) {

        callbackArguments.push_back(env.Null());
        if (data->fetchMode == FETCH_ARRAY) {
          callbackArguments.push_back(ODBC::GetRecordArray(env,
            data->objResult->m_hSTMT,
            data->objResult->columns,
            &data->objResult->colCount,
            data->objResult->buffer,
            data->objResult->bufferLength));
        }
        else {
          callbackArguments.push_back(ODBC::GetRecordTuple(env,
            data->objResult->m_hSTMT,
            data->objResult->columns,
            &data->objResult->colCount,
            data->objResult->buffer,
            data->objResult->bufferLength));
        }
      }
      else {
        ODBC::FreeColumns(data->objResult->columns, &data->objResult->colCount);
        
        //if there was an error, pass that as arg[0] otherwise Null
        if (error) {
          callbackArguments.push_back(objError);
        }
        else {
          callbackArguments.push_back(env.Null());
        }
        
        callbackArguments.push_back(env.Null());
      }

      Callback().Call(callbackArguments);

      free(data);
      
      return;
    }

  private:
    ODBCResult *odbcResultObject;
    fetch_work_data *data;
};

Napi::Value ODBCResult::Fetch(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::Fetch\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  fetch_work_data* data = (fetch_work_data *) calloc(1, sizeof(fetch_work_data));
  
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

  return env.Undefined();
}

/*
 * FetchSync
 */

Napi::Value ODBCResult::FetchSync(const Napi::CallbackInfo& info) {
  // DEBUG_PRINTF("ODBCResult::FetchSync\n");
  // Napi::HandleScope scope(env);
  
  // ODBCResult* objResult = info.Holder().Unwrap<ODBCResult>();

  // Napi::Value objError;
  // bool moreWork = true;
  // bool error = false;
  // int fetchMode = objResult->m_fetchMode;
  
  // if (info.Length() == 1 && info[0].IsObject()) {
  //   Napi::Object obj = info[0].ToObject();
    
  //   Napi::String fetchModeKey = Napi::String::New(env, OPTION_FETCH_MODE);
  //   if (obj->Has(fetchModeKey) && obj->Get(fetchModeKey).IsNumber()) {
  //     fetchMode = obj->Get(fetchModeKey)->ToInt32()->Value();
  //   }
  // }
  
  // SQLRETURN ret = SQLFetch(objResult->m_hSTMT);

  // if (objResult->colCount == 0) {
  //   objResult->columns = ODBC::GetColumns(
  //     objResult->m_hSTMT, 
  //     &objResult->colCount);
  // }
  
  // //check to see if the result has no columns
  // if (objResult->colCount == 0) {
  //   moreWork = false;
  // }
  // //check to see if there was an error
  // else if (ret == SQL_ERROR)  {
  //   moreWork = false;
  //   error = true;
    
  //   objError = ODBC::GetSQLError(
  //     SQL_HANDLE_STMT, 
  //     objResult->m_hSTMT,
  //     (char *) "Error in ODBCResult::UV_AfterFetch");
  // }
  // //check to see if we are at the end of the recordset
  // else if (ret == SQL_NO_DATA) {
  //   moreWork = false;
  // }

  // if (moreWork) {
  //   Napi::Value data;
    
  //   if (fetchMode == FETCH_ARRAY) {
  //     data = ODBC::GetRecordArray(
  //       objResult->m_hSTMT,
  //       objResult->columns,
  //       &objResult->colCount,
  //       objResult->buffer,
  //       objResult->bufferLength);
  //   }
  //   else {
  //     data = ODBC::GetRecordTuple(
  //       objResult->m_hSTMT,
  //       objResult->columns,
  //       &objResult->colCount,
  //       objResult->buffer,
  //       objResult->bufferLength);
  //   }
    
  //   return data;
  // }
  // else {
  //   ODBC::FreeColumns(objResult->columns, &objResult->colCount);

  //   //if there was an error, pass that as arg[0] otherwise Null
  //   if (error) {
  //     Napi::Error::New(env, objError).ThrowAsJavaScriptException();

      
  //     return env.Null();
  //   }
  //   else {
  //     return env.Null();
  //   }
  // }
}

/*
 * FetchAll
 */

class FetchAllAsyncWorker : public Napi::AsyncWorker {

  public:
    FetchAllAsyncWorker(ODBCResult *odbcResultObject, fetch_work_data *data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcResultObject(odbcResultObject),
      data(data) {}

    ~FetchAllAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCResult::UV_FetchAll\n");

      data->result = SQLFetch(data->objResult->m_hSTMT);
    }

    void OnOK() {

    DEBUG_PRINTF("ODBCResult::UV_AfterFetchAll\n");

    Napi::Env env = Env();
    Napi::HandleScope scope(env);
    
    bool doMoreWork = true;
    
    if (odbcResultObject->colCount == 0) {
      odbcResultObject->columns = ODBC::GetColumns(odbcResultObject->m_hSTMT, &odbcResultObject->colCount);
    }
    
    //check to see if the result set has columns
    if (odbcResultObject->colCount == 0) {
      //this most likely means that the query was something like
      //'insert into ....'
      doMoreWork = false;
    }
    //check to see if there was an error
    else if (data->result == SQL_ERROR)  {
      data->errorCount++;

      //NanAssignPersistent(data->objError, ODBC::GetSQLError(
      data->objError = ODBC::GetSQLError(env,
          SQL_HANDLE_STMT, 
          odbcResultObject->m_hSTMT,
          (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll"
      );
      
      doMoreWork = false;
    }
    //check to see if we are at the end of the recordset
    else if (data->result == SQL_NO_DATA) {
      doMoreWork = false;
    }
    else {
      Napi::Array rows = (data->rows).Value();
      if (data->fetchMode == FETCH_ARRAY) {
        rows.Set(
          Napi::Number::New(env, data->count), 
          ODBC::GetRecordArray(env,
            odbcResultObject->m_hSTMT,
            odbcResultObject->columns,
            &odbcResultObject->colCount,
            odbcResultObject->buffer,
            odbcResultObject->bufferLength)
        );
      }
      else {
        rows.Set(
          Napi::Number::New(env, data->count), 
          ODBC::GetRecordTuple(env,
            odbcResultObject->m_hSTMT,
            odbcResultObject->columns,
            &odbcResultObject->colCount,
            odbcResultObject->buffer,
            odbcResultObject->bufferLength)
        );
      }
      data->count++;
    }
    
    if (doMoreWork) {
      // FetchAllAsyncWorker *worker = new FetchAllAsyncWorker(odbcResultObject, data, Callback());
    }
    else {
      ODBC::FreeColumns(odbcResultObject->columns, &odbcResultObject->colCount);
      
      std::vector<napi_value> callbackArguments;
      
      if (data->errorCount > 0) {
        callbackArguments.push_back(data->objError);
      }
      else {
        callbackArguments.push_back(env.Null());
      }
      
      Callback().Call(callbackArguments);

      free(data);
      
      return;
    }
  }

  private:
    ODBCResult *odbcResultObject;
    fetch_work_data *data;
};

Napi::Value ODBCResult::FetchAll(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::FetchAll\n");

  Napi::Env env = Env();
  Napi::HandleScope scope(env);
  
  fetch_work_data* data = (fetch_work_data *) calloc(1, sizeof(fetch_work_data));
  
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
  
  data->rows.Reset();
  data->errorCount = 0;
  data->count = 0;

  FetchAllAsyncWorker *worker = new FetchAllAsyncWorker(this, data, callback);

  return env.Undefined();

  DEBUG_PRINTF("ODBCResult::Fetch\n");
}

/*
 * FetchAllSync
 */

Napi::Value ODBCResult::FetchAllSync(const Napi::CallbackInfo& info) {
  // DEBUG_PRINTF("ODBCResult::FetchAllSync\n");
  // Napi::HandleScope scope(env);
  
  // ODBCResult* self = info.Holder().Unwrap<ODBCResult>();
  
  // Napi::Value objError = Napi::Object::New(env);
  
  // SQLRETURN ret;
  // int count = 0;
  // int errorCount = 0;
  // int fetchMode = self->m_fetchMode;

  // if (info.Length() == 1 && info[0].IsObject()) {
  //   Napi::Object obj = info[0].ToObject();
    
  //   Napi::String fetchModeKey = Napi::String::New(env, OPTION_FETCH_MODE);
  //   if (obj->Has(fetchModeKey) && obj->Get(fetchModeKey).IsNumber()) {
  //     fetchMode = obj->Get(fetchModeKey)->ToInt32()->Value();
  //   }
  // }
  
  // if (self->colCount == 0) {
  //   self->columns = ODBC::GetColumns(self->m_hSTMT, &self->colCount);
  // }
  
  // Napi::Array rows = Napi::Array::New(env);
  
  // //Only loop through the recordset if there are columns
  // if (self->colCount > 0) {
  //   //loop through all records
  //   while (true) {
  //     ret = SQLFetch(self->m_hSTMT);
      
  //     //check to see if there was an error
  //     if (ret == SQL_ERROR)  {
  //       errorCount++;
        
  //       objError = ODBC::GetSQLError(
  //         SQL_HANDLE_STMT, 
  //         self->m_hSTMT,
  //         (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll; probably"
  //           " your query did not have a result set."
  //       );
        
  //       break;
  //     }
      
  //     //check to see if we are at the end of the recordset
  //     if (ret == SQL_NO_DATA) {
  //       ODBC::FreeColumns(self->columns, &self->colCount);
        
  //       break;
  //     }

  //     if (fetchMode == FETCH_ARRAY) {
  //       rows.Set(
  //         Napi::New(env, count), 
  //         ODBC::GetRecordArray(
  //           self->m_hSTMT,
  //           self->columns,
  //           &self->colCount,
  //           self->buffer,
  //           self->bufferLength)
  //       );
  //     }
  //     else {
  //       rows.Set(
  //         Napi::New(env, count), 
  //         ODBC::GetRecordTuple(
  //           self->m_hSTMT,
  //           self->columns,
  //           &self->colCount,
  //           self->buffer,
  //           self->bufferLength)
  //       );
  //     }
  //     count++;
  //   }
  // }
  // else {
  //   ODBC::FreeColumns(self->columns, &self->colCount);
  // }
  
  // //throw the error object if there were errors
  // if (errorCount > 0) {
  //   Napi::Error::New(env, objError).ThrowAsJavaScriptException();

  // }
  
  // return rows;
}

/*
 * CloseSync
 * 
 */

Napi::Value ODBCResult::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  
  DEBUG_PRINTF("ODBCResult::CloseSync closeOption=%i m_canFreeHandle=%i\n", closeOption, this->m_canFreeHandle);
  
  if (closeOption == SQL_DESTROY && this->m_canFreeHandle) {
    this->Free();
  }
  else if (closeOption == SQL_DESTROY && !this->m_canFreeHandle) {
    //We technically can't free the handle so, we'll SQL_CLOSE
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeStmt(this->m_hSTMT, SQL_CLOSE);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  else {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeStmt(this->m_hSTMT, closeOption);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  
  return Napi::Boolean::New(env, true);
}

Napi::Value ODBCResult::MoreResultsSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::MoreResultsSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  
  SQLRETURN ret = SQLMoreResults(this->m_hSTMT);

  if (ret == SQL_ERROR) {
    Napi::Value objError = ODBC::GetSQLError(env,
    	SQL_HANDLE_STMT, 
	    this->m_hSTMT, 
	    (char *)"[node-odbc] Error in ODBCResult::MoreResultsSync"
    );

    Napi::Error(env, objError).ThrowAsJavaScriptException();
  }

  return Napi::Boolean::New(env, SQL_SUCCEEDED(ret) || ret == SQL_ERROR ? true : false);
}

/*
 * GetColumnNamesSync
 */

Napi::Value ODBCResult::GetColumnNamesSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCResult::GetColumnNamesSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Array cols = Napi::Array::New(env);
  
  if (this->colCount == 0) {
    this->columns = ODBC::GetColumns(this->m_hSTMT, &this->colCount);
  }
  
  for (int i = 0; i < this->colCount; i++) {
#ifdef UNICODE
    cols.Set(Napi::Number::New(env, i),
              Napi::String::New(env, (char16_t*) this->columns[i].name));
#else
    cols.Set(Napi::Number::New(env, i),
              Napi::String::New(env, (char *) this->columns[i].name));
#endif

  }
    
  return cols;
}

/*
 * GetRowCountSync
 */

Napi::Value ODBCResult::GetRowCountSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCResult::GetRowCountSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLLEN rowCount = 0;

  SQLRETURN ret = SQLRowCount(this->m_hSTMT, &rowCount);

  if (!SQL_SUCCEEDED(ret)) {
    rowCount = 0;
  }

  return Napi::Number::New(env, rowCount);
}
