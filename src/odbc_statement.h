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

#ifndef _SRC_ODBC_STATEMENT_H
#define _SRC_ODBC_STATEMENT_H

#include <napi.h>
#include <uv.h>

class ODBCStatement : public Napi::ObjectWrap<ODBCStatement> {
  public:
   static Napi::FunctionReference constructor;
   static Napi::Object Init(Napi::Env, Napi::Object, HENV hENV, HDBC hDBC, HSTMT hSTMT);
   
   explicit ODBCStatement(const Napi::CallbackInfo& info);

   static HENV m_hENV;
   static HDBC m_hDBC;
   static HSTMT m_hSTMT;

   void Free();

   ~ODBCStatement();
   
  protected:
    //ODBCStatement() {};
     
    //~ODBCStatement();

    //constructor
public:
    Napi::Value New(const Napi::CallbackInfo& info);

    //async methods
    Napi::Value Execute(const Napi::CallbackInfo& info);
protected:
    static void UV_Execute(uv_work_t* work_req);
    static void UV_AfterExecute(uv_work_t* work_req, int status);

public:
    Napi::Value ExecuteDirect(const Napi::CallbackInfo& info);
protected:
    static void UV_ExecuteDirect(uv_work_t* work_req);
    static void UV_AfterExecuteDirect(uv_work_t* work_req, int status);

public:
    Napi::Value ExecuteNonQuery(const Napi::CallbackInfo& info);
protected:
    static void UV_ExecuteNonQuery(uv_work_t* work_req);
    static void UV_AfterExecuteNonQuery(uv_work_t* work_req, int status);
    
public:
    Napi::Value Prepare(const Napi::CallbackInfo& info);
protected:
    static void UV_Prepare(uv_work_t* work_req);
    static void UV_AfterPrepare(uv_work_t* work_req, int status);

public:
    Napi::Value Bind(const Napi::CallbackInfo& info);
protected:
    static void UV_Bind(uv_work_t* work_req);
    static void UV_AfterBind(uv_work_t* work_req, int status);
    
    //sync methods
public:
    Napi::Value CloseSync(const Napi::CallbackInfo& info);
    Napi::Value ExecuteSync(const Napi::CallbackInfo& info);
    Napi::Value ExecuteDirectSync(const Napi::CallbackInfo& info);
    Napi::Value ExecuteNonQuerySync(const Napi::CallbackInfo& info);
    Napi::Value PrepareSync(const Napi::CallbackInfo& info);
    Napi::Value BindSync(const Napi::CallbackInfo& info);
protected:

    struct Fetch_Request {
      Napi::FunctionReference* callback;
      ODBCStatement *objResult;
      SQLRETURN result;
    };
    
    ODBCStatement *self(void) { return this; }

  protected:
    Parameter *params;
    int paramCount;
    
    uint16_t *buffer;
    int bufferLength;
    Column *columns;
    short colCount;
};

struct execute_direct_work_data {
  Napi::FunctionReference* cb;
  ODBCStatement *stmt;
  int result;
  void *sql;
  int sqlLen;
};

struct execute_work_data {
  Napi::FunctionReference* cb;
  ODBCStatement *stmt;
  int result;
};

struct prepare_work_data {
  Napi::FunctionReference* cb;
  ODBCStatement *stmt;
  int result;
  void *sql;
  int sqlLen;
};

struct bind_work_data {
  Napi::FunctionReference* cb;
  ODBCStatement *stmt;
  int result;
};

#endif
