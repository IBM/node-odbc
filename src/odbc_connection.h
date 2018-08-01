/*
  Copyright (c) 2013, Dan VerWeire<dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith<notwink@gmail.com>

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

#ifndef _SRC_ODBC_CONNECTION_H
#define _SRC_ODBC_CONNECTION_H

#include <napi.h>
#include <uv.h>

class ODBCConnection : public Napi::ObjectWrap<ODBCConnection> {

  friend class OpenAsyncWorker;
  friend class CloseAsyncWorker;
  friend class CreateStatementAsyncWorker;
  friend class QueryAsyncWorker;
  friend class BeginTransactionAsyncWorker;
  friend class EndTransactionAsyncWorker;
  friend class TablesAsyncWorker;
  friend class ColumnsAsyncWorker;

  public:
   ODBCConnection(const Napi::CallbackInfo& info);
   static Napi::String OPTION_SQL;
   static Napi::String OPTION_PARAMS;
   static Napi::String OPTION_NORESULTS;
   static Napi::FunctionReference constructor;
   
   static Napi::Object Init(Napi::Env env, Napi::Object exports);
   
   ~ODBCConnection();

   void Free(SQLRETURN *sqlRetrunCode);
   
  protected:
    //ODBCConnection() {};
    //~ODBCConnection();

public:
    //constructor
    static Napi::Value New(const Napi::CallbackInfo& info);

    // functions exposed by N-API to JavaScript
    Napi::Value Open(const Napi::CallbackInfo& info);
    Napi::Value OpenSync(const Napi::CallbackInfo& info);

    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value CloseSync(const Napi::CallbackInfo& info);

    Napi::Value CreateStatement(const Napi::CallbackInfo& info);
    Napi::Value CreateStatementSync(const Napi::CallbackInfo& info);

    Napi::Value Query(const Napi::CallbackInfo& info);
    Napi::Value QuerySync(const Napi::CallbackInfo& info);

    Napi::Value BeginTransaction(const Napi::CallbackInfo& info);
    Napi::Value BeginTransactionSync(const Napi::CallbackInfo& info);

    Napi::Value EndTransaction(const Napi::CallbackInfo& info);
    Napi::Value EndTransactionSync(const Napi::CallbackInfo& info);

    Napi::Value Columns(const Napi::CallbackInfo& info);
    Napi::Value ColumnsSync(const Napi::CallbackInfo& info);

    Napi::Value Tables(const Napi::CallbackInfo& info);
    Napi::Value TablesSync(const Napi::CallbackInfo& info);

    Napi::Value GetInfo(const Napi::CallbackInfo& info);
    Napi::Value GetInfoSync(const Napi::CallbackInfo& info);

    //Property Getter/Setterss
    Napi::Value ConnectedGetter(const Napi::CallbackInfo& info);
    void ConnectedSetter(const Napi::CallbackInfo& info, const Napi::Value &value);
    Napi::Value ConnectTimeoutGetter(const Napi::CallbackInfo& info);
    void ConnectTimeoutSetter(const Napi::CallbackInfo& info, const Napi::Value &value);
    Napi::Value LoginTimeoutGetter(const Napi::CallbackInfo& info);
    void LoginTimeoutSetter(const Napi::CallbackInfo& info, const Napi::Value &value);

protected:

    // struct Fetch_Request {
    //   Napi::FunctionReference* callback;
    //   ODBCConnection *objResult;
    //   SQLRETURN result;
    // };
    
    // //ODBCConnection *self(void) { return this; }

  protected:
    SQLHENV m_hENV;
    SQLHDBC m_hDBC;
    SQLUSMALLINT canHaveMoreResults;
    bool connected;
    int statements;
    SQLUINTEGER connectTimeout;
    SQLUINTEGER loginTimeout;
};

// struct query_work_data {
    
//   Napi::FunctionReference* cb;
//   ODBCConnection *conn;
//   HSTMT hSTMT;
  
//   Parameter *params;
//   int paramCount;
//   int completionType;
//   bool noResultObject;
  
//   void *sql;
//   void *catalog;
//   void *schema;
//   void *table;
//   void *type;
//   void *column;
  
//   int sqlLen;
//   int sqlSize;
  
//   int result;
// };

#endif
