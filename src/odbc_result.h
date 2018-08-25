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

#ifndef _SRC_ODBC_RESULT_H
#define _SRC_ODBC_RESULT_H

#include <napi.h>
#include <uv.h>

class ODBCResult : public Napi::ObjectWrap<ODBCResult> {

  friend class FetchAsyncWorker;
  friend class FetchAllAsyncWorker;
  friend class CreateConnectionAsyncWorker;
  friend class CloseAsyncWorker;

  public:
    static Napi::String OPTION_FETCH_MODE;
    static Napi::FunctionReference constructor;
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    
    SQLRETURN Free();

    explicit ODBCResult(const Napi::CallbackInfo& info);

    ~ODBCResult();

    QueryData *data;

    static HENV m_hENV;
    static HDBC m_hDBC;
    static HSTMT m_hSTMT;
    static bool m_canFreeHandle;

    int fetchMode;

    explicit ODBCResult(const Napi::CallbackInfo& info);
    ~ODBCResult();

    Napi::Value Fetch(const Napi::CallbackInfo& info);
    Napi::Value FetchSync(const Napi::CallbackInfo& info);

    Napi::Value FetchAll(const Napi::CallbackInfo& info);
    Napi::Value FetchAllSync(const Napi::CallbackInfo& info);

    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value CloseSync(const Napi::CallbackInfo& info);

    Napi::Value MoreResultsSync(const Napi::CallbackInfo& info);
    Napi::Value GetColumnNamesSync(const Napi::CallbackInfo& info);
    Napi::Value GetRowCountSync(const Napi::CallbackInfo& info);
    
    //property getter/setters
    Napi::Value FetchModeGetter(const Napi::CallbackInfo& info);
    void FetchModeSetter(const Napi::CallbackInfo& info, const Napi::Value& value);
};

#endif
