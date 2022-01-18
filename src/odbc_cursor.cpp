/*
  Copyright (c) 2021 IBM

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

#include "odbc_cursor.h"

Napi::FunctionReference ODBCCursor::constructor;

Napi::Object ODBCCursor::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCCursor", {
    InstanceMethod("fetch", &ODBCCursor::Fetch),
    InstanceMethod("close", &ODBCCursor::Close),

    InstanceAccessor("noData", &ODBCCursor::MoreResultsGetter, nullptr),
  });

  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  return exports;
}


ODBCCursor::ODBCCursor(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCCursor>(info) {
  this->data = info[0].As<Napi::External<StatementData>>().Data();
  this->odbcConnection = info[1].As<Napi::External<ODBCConnection>>().Data();
  if (info.Length() > 2 && info[2].IsArray()) {
    this->napiParametersReference = Napi::Persistent(info[2].As<Napi::Array>());
  } else {
    this->napiParametersReference = Napi::Persistent(Napi::Array::New(Env()));
  }
  this->free_statement_on_close = info[3].As<Napi::Boolean>().Value();
}

SQLRETURN ODBCCursor::Free() {

  SQLRETURN return_code = SQL_SUCCESS;

  if (this->free_statement_on_close && this->data)
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

    napiParametersReference.Reset();
    delete this->data;
    this->data = NULL;
  }

  return return_code;
}

ODBCCursor::~ODBCCursor()
{
  this->Free();
}

////////////////////////////////////////////////////////////////////////////////
//   FETCH   ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class FetchAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCCursor    *cursor;
    StatementData *data;

  public:
    FetchAsyncWorker
    (
      ODBCCursor     *cursor,
      Napi::Function &callback
    ) 
    :
    ODBCAsyncWorker(callback),
    cursor(cursor),
    data(cursor->data)
    {}

    ~FetchAsyncWorker() {}

    void Execute() {
      SQLRETURN return_code;
      bool alloc_error = false;

      return_code =
      fetch_and_store
      (
        data,
        true,
        &alloc_error
      );

      if (alloc_error == true)
      {
        SetError("[odbc] Error allocating or reallocating memory when fetching data. No ODBC error information available.\0");
        return;
      }

      if (!SQL_SUCCEEDED(return_code) && return_code != SQL_NO_DATA) {
        if (return_code == SQL_INVALID_HANDLE) {
          SetError("[odbc] Error fetching results with SQLFetch: SQL_INVALID_HANDLE\0");
          return;
        } else {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        }
        SetError("[odbc] Error fetching results with SQLFetch\0");
        return;
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      Napi::Array rows =
      process_data_for_napi
      (
        env,
        data,
        cursor->napiParametersReference.Value()
      );

      std::vector<napi_value> callbackArguments
      {
        env.Null(),
        rows
      };

      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCCursor::Fetch(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  FetchAsyncWorker *worker = new FetchAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

////////////////////////////////////////////////////////////////////////////////
//   Close   ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class CursorCloseAsyncWorker : public ODBCAsyncWorker {

  private:
    ODBCCursor    *odbcCursor;
    StatementData *data;

  public:
    CursorCloseAsyncWorker
    (
      ODBCCursor     *cursor,
      Napi::Function &callback
    ) 
    :
    ODBCAsyncWorker(callback),
    odbcCursor(cursor),
    data(cursor->data)
    {}

    ~CursorCloseAsyncWorker() {}

    void Execute() {

      SQLRETURN return_code;

      return_code =
      SQLCloseCursor
      (
        data->hstmt  // StatementHandle
      );

      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
        SetError("[odbc] Error closing the cursor!\0");
        return;
      }

      if (odbcCursor->free_statement_on_close)
      {
        return_code = odbcCursor->Free();
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hstmt);
          SetError("[odbc] Error closing the Statement\0");
          return;
        }
      }
      else
      {
        if (data != NULL)
        {
          data->deleteColumns();
        }
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

Napi::Value ODBCCursor::Close(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CursorCloseAsyncWorker *worker = new CursorCloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

// Return true once SQL_NO_DATA has been returned from SQLFetch. Have to do it
// this way, as a "moreResults" would have to return true if called before the
// first call to SQLFetch, but there might actually not be any more results to
// retrieve.
Napi::Value ODBCCursor::MoreResultsGetter(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  return Napi::Boolean::New(env, data->result_set_end_reached);
}
