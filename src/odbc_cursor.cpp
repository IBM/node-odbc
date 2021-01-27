/*
  Copyright (c) 2021, IBM
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

#include "odbc_cursor.h"

Napi::FunctionReference ODBCCursor::constructor;

Napi::Object ODBCCursor::Init(Napi::Env env, Napi::Object exports) {
  DEBUG_PRINTF("ODBCCursor::Init\n");

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
  // this->napiParameters = info[1].As<Napi::Array>();
}

ODBCCursor::~ODBCCursor() {
  this->Free();
  delete data;
  data = NULL;
}

SQLRETURN ODBCCursor::Free() {
  DEBUG_PRINTF("ODBCCursor::Free\n");

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
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::FetchAsyncWorker::Execute()\n", data->henv, data->hdbc, data->hSTMT);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::FetchAsyncWorker::Execute(): Running SQLFetch(StatementHandle = %p)\n", data->henv, data->hdbc, data->hSTMT);

      SQLRETURN return_code;

      return_code =
      fetch_and_store
      (
        data
      );

      if (!SQL_SUCCEEDED(return_code) && return_code != SQL_NO_DATA) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::FetchAsyncWorker::Execute(): SQLFetch returned code %d\n", data->henv, data->hdbc, data->hSTMT, return_code);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error fetching results with SQLFetch\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::FetchAsyncWorker::Execute(): SQLFetch succeeded: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->return_code);
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::FetchAsyncWorker::OnOk()\n", data->henv, data->hdbc, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // TODO: Get the actual parameters
      Napi::Array empty = Napi::Array::New(env);
      Napi::Array rows = process_data_for_napi(env, data, empty);

      // Have to clear out the data in the storedRow, so that they aren't
      // lingering the next time fetch is called.
      for (size_t h = 0; h < data->storedRows.size(); h++) {
        delete[] data->storedRows[h];
      };
      data->storedRows.clear();

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(rows);
      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCCursor::Fetch(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::Fetch()\n", data->henv, data->hdbc, data->hSTMT);

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
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::CursorCloseAsyncWorker::Execute()\n", data->henv, data->hdbc, data->hSTMT);

      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::CursorCloseAsyncWorker::Execute(): Running SQLCloseCursor(StatementHandle = %p)\n", data->henv, data->hdbc, data->hSTMT);

      data->sqlReturnCode =
      SQLCloseCursor
      (
        data->hSTMT  // StatementHandle
      );

      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::CursorCloseAsyncWorker::Execute(): SQLCloseCursor returned code %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error fetching results with SQLFetch\0");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::CursorCloseAsyncWorker::Execute(): SQLCloseCursor succeeded: SQLRETURN = %d\n", data->henv, data->hdbc, data->hSTMT, data->sqlReturnCode);

      data->sqlReturnCode = odbcCursor->Free();
      if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][[SQLHDBC: %p] ODBCCursor::CursorCloseAsyncWorker::Execute(): SQLFreeHandle returned %d\n", data->henv, data->hdbc, data->sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_STMT, data->hSTMT);
        SetError("[odbc] Error closing the Statement\0");
        return;
      }

    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::CursorCloseAsyncWorker::OnOk()\n", data->henv, data->hdbc, data->hSTMT);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }
};

Napi::Value ODBCCursor::Close(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p][SQLHSTMT: %p] ODBCCursor::Close()\n", odbcConnection->hENV, odbcConnection->hDBC, data->hSTMT);

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