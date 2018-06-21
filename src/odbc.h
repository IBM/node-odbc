/*
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith <notwink@gmail.com>

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

#ifndef _SRC_ODBC_H
#define _SRC_ODBC_H

#include <napi.h>
#include <uv.h>
#include <napi.h>
#include <wchar.h>

#include <stdlib.h>
#ifdef dynodbc
#include "dynodbc.h"
#else
#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <sqlucode.h>
#endif

using namespace Napi;

#define MAX_FIELD_SIZE 1024
#define MAX_VALUE_SIZE 1048576

#ifdef UNICODE
#define ERROR_MESSAGE_BUFFER_BYTES 2048
#define ERROR_MESSAGE_BUFFER_CHARS 1024
#else
#define ERROR_MESSAGE_BUFFER_BYTES 2048
#define ERROR_MESSAGE_BUFFER_CHARS 2048
#endif

#define MODE_COLLECT_AND_CALLBACK 1
#define MODE_CALLBACK_FOR_EACH 2
#define FETCH_ARRAY 3
#define FETCH_OBJECT 4
#define SQL_DESTROY 9999


typedef struct {
  unsigned char *name;
  unsigned int len;
  SQLLEN type;
  SQLUSMALLINT index;
} Column;

typedef struct {
  SQLSMALLINT  ValueType;
  SQLSMALLINT  ParameterType;
  SQLLEN       ColumnSize;
  SQLSMALLINT  DecimalDigits;
  void        *ParameterValuePtr;
  SQLLEN       BufferLength; 
  SQLLEN       StrLen_or_IndPtr;
} Parameter;

class ODBC : public Napi::ObjectWrap<ODBC> {
  public:
    ODBC(const Napi::CallbackInfo& info);
    static Napi::FunctionReference constructor;
    static uv_mutex_t g_odbcMutex;
    
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Column* GetColumns(SQLHSTMT hStmt, short* colCount);
    static void FreeColumns(Column* columns, short* colCount);
    static Napi::Value GetColumnValue(Napi::Env env, SQLHSTMT hStmt, Column column, uint16_t* buffer, int bufferLength);
    static Napi::Value GetRecordTuple (Napi::Env env, SQLHSTMT hStmt, Column* columns, short* colCount, uint16_t* buffer, int bufferLength);
    static Napi::Value GetRecordArray (Napi::Env env, SQLHSTMT hStmt, Column* columns, short* colCount, uint16_t* buffer, int bufferLength);
    static Napi::Value CallbackSQLError(Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, Napi::FunctionReference* cb);
    static Napi::Value CallbackSQLError(Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, char* message, Napi::FunctionReference* cb);
    static Napi::Value GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle);
    static Napi::Value GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, char* message);
    static Napi::Array GetAllRecordsSync (Napi::Env env, HENV hENV, HDBC hDBC, HSTMT hSTMT, uint16_t* buffer, int bufferLength);
#ifdef dynodbc
    static Napi::Value LoadODBCLibrary(const Napi::CallbackInfo& info);
#endif
    static Parameter* GetParametersFromArray (Napi::Array *values, int *paramCount);
    
    void Free();

    ~ODBC();
    HENV m_hEnv;
    
  protected:
    //ODBC() {}

    //~ODBC();


  public:
    //static Napi::Value New(const Napi::CallbackInfo& info);

    //async methods
    Napi::Value CreateConnection(const Napi::CallbackInfo& info);
  protected:
    static void UV_CreateConnection(uv_work_t* work_req);
    static void UV_AfterCreateConnection(uv_work_t* work_req, int status);
    
    static void WatcherCallback(uv_async_t* w, int revents);
    
    //sync methods
  public:
    Napi::Value CreateConnectionSync(const Napi::CallbackInfo& info);
  protected:
    
    ODBC *self(void) { return this; }

    //HENV m_hEnv;
};

struct create_connection_work_data {
  Napi::FunctionReference* cb;
  ODBC *dbo;
  HDBC hDBC;
  int result;
};

struct open_request {
  Napi::FunctionReference cb;
  ODBC *dbo;
  int result;
  char connection[1];
};

struct close_request {
  Napi::FunctionReference cb;
  ODBC *dbo;
  int result;
};

struct query_request {
  Napi::FunctionReference cb;
  ODBC *dbo;
  HSTMT hSTMT;
  int affectedRows;
  char *sql;
  char *catalog;
  char *schema;
  char *table;
  char *type;
  char *column;
  Parameter *params;
  int  paramCount;
  int result;
};

#ifdef UNICODE
#define SQL_T(x) (L##x)
#else
#define SQL_T(x) (x)
#endif

#ifdef DEBUG
#define DEBUG_TPRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) (void)0
#define DEBUG_TPRINTF(...) (void)0
#endif

#define REQ_ARGS(N)                                                     \
  if (info.Length() < (N))                                              \
    Napi::TypeError::New(env, "Expected " #N "arguments").ThrowAsJavaScriptException(); \
    return env.Null();

//Require String Argument; Save String as Utf8
#define REQ_STR_ARG(I, VAR)                                             \
  if (info.Length() <= (I) || !info[I].IsString())                     \
    Napi::TypeError::New(env, "Argument " #I " must be a string").ThrowAsJavaScriptException(); \
    return env.Null();       \
  Napi::String VAR(env, info[I].ToString());

//Require String Argument; Save String as Wide String (UCS2)
#define REQ_WSTR_ARG(I, VAR)                                            \
  if (info.Length() <= (I) || !info[I].IsString())                     \
    Napi::TypeError::New(env, "Argument " #I " must be a string").ThrowAsJavaScriptException(); \
    return env.Null();       \
  String::Value VAR(info[I].ToString());

//Require String Argument; Save String as Object
#define REQ_STRO_ARG(I, VAR)                                            \
  if (info.Length() <= (I) || !info[I].IsString())                     \
    Napi::TypeError::New(env, "Argument " #I " must be a string").ThrowAsJavaScriptException(); \
    return env.Null();       \
  Napi::String VAR(info[I].ToString());

//Require String or Null Argument; Save String as Utf8
#define REQ_STR_OR_NULL_ARG(I, VAR)                                             \
  if ( info.Length() <= (I) || (!info[I].IsString() && !info[I].IsNull()) )   \
    Napi::TypeError::New(env, "Argument " #I " must be a string or null").ThrowAsJavaScriptException(); \
    return env.Null();       \
  Napi::String VAR(env, info[I].ToString());

//Require String or Null Argument; Save String as Wide String (UCS2)
#define REQ_WSTR_OR_NULL_ARG(I, VAR)                                              \
  if ( info.Length() <= (I) || (!info[I].IsString() && !info[I].IsNull()) )     \
    Napi::TypeError::New(env, "Argument " #I " must be a string or null").ThrowAsJavaScriptException(); \
    return env.Null();         \
  String::Value VAR(info[I].ToString());

//Require String or Null Argument; save String as String Object
#define REQ_STRO_OR_NULL_ARG(I, VAR)                                              \
  if ( info.Length() <= (I) || (!info[I].IsString() && !info[I].IsNull()) ) {   \
    Napi::TypeError::New(env, "Argument " #I " must be a string or null").ThrowAsJavaScriptException(); \
                \
    return;                                                         \
  }                                                                               \
  Napi::String VAR(info[I].ToString());

#define REQ_FUN_ARG(I, VAR)                                             \
  if (info.Length() <= (I) || !info[I].IsFunction())                   \
    Napi::TypeError::New(env, "Argument " #I " must be a function").ThrowAsJavaScriptException(); \
    return env.Null();     \
  Napi::Function VAR = info[I].As<Napi::Function>();

#define REQ_BOOL_ARG(I, VAR)                                            \
  if (info.Length() <= (I) || !info[I].IsBoolean())                    \
    Napi::TypeError::New(env, "Argument " #I " must be a boolean").ThrowAsJavaScriptException(); \
    return env.Null();      \
  Napi::Boolean VAR = (info[I].ToBoolean());

#define REQ_EXT_ARG(I, VAR)                                             \
  if (info.Length() <= (I) || !info[I].IsExternal())                   \
    Napi::TypeError::New(env, "Argument " #I " invalid").ThrowAsJavaScriptException(); \
    return env.Null();                \
  Napi::External VAR = info[I].As<Napi::External>();

#define OPT_INT_ARG(I, VAR, DEFAULT)                                    \
  int VAR;                                                              \
  if (info.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
          } else if (info[I].IsNumber()) {                                      \
    VAR = info[I].As<Napi::Number>().Int32Value();                                        \
  } else {                                                              \
    Napi::TypeError::New(env, "Argument " #I " must be an integer").ThrowAsJavaScriptException(); \
    return env.Null();     \
  }


// From node v10 NODE_DEFINE_CONSTANT
#define NODE_ODBC_DEFINE_CONSTANT(constructor, constant)       \
  (constructor).Set(Napi::String::New(env, #constant),                \
                Napi::Number::New(env, constant),                               \
                static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))

#endif
