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

static const std::string NAME = "name";
static const std::string DATA_TYPE = "dataType";
static const std::string COUNT = "count";
static const std::string COLUMNS = "columns";

typedef struct Column {
  SQLUSMALLINT  index;
  SQLTCHAR      *name;
  SQLSMALLINT   nameSize;
  SQLSMALLINT   type;
  SQLULEN       precision;
  SQLSMALLINT   scale;
  SQLSMALLINT   nullable;
  SQLLEN        dataLength;
} Column;

typedef struct Parameter {
  SQLSMALLINT  InputOutputType;
  SQLSMALLINT  ValueType;
  SQLSMALLINT  ParameterType;
  SQLLEN       ColumnSize;
  SQLSMALLINT  DecimalDigits;
  void        *ParameterValuePtr;
  SQLLEN       BufferLength; 
  SQLLEN       StrLen_or_IndPtr;
} Parameter;

typedef struct ColumnData {
  SQLTCHAR *data;
  int       size;
} ColumnData;

// QueryData
typedef struct QueryData {

  HSTMT hSTMT;

  int fetchMode;

  Napi::Value objError;
  
  // parameters
  Parameter *params;
  int paramCount = 0;
  int completionType;

  // columns and rows
  Column                    *columns;
  SQLSMALLINT                columnCount;
  SQLTCHAR                 **boundRow;
  std::vector<ColumnData*>   storedRows;
  SQLLEN                     rowCount;

  // query options
  bool useCursor = false;
  int fetchCount = 0;

  SQLTCHAR *sql     = NULL;
  SQLTCHAR *catalog = NULL;
  SQLTCHAR *schema  = NULL;
  SQLTCHAR *table   = NULL;
  SQLTCHAR *type    = NULL;
  SQLTCHAR *column  = NULL;

  SQLRETURN sqlReturnCode;

  ~QueryData() {

    if (this->paramCount) {
      Parameter prm;
      // free parameters
      for (int i = 0; i < this->paramCount; i++) {
        if (prm = this->params[i], prm.ParameterValuePtr != NULL) {
          switch (prm.ValueType) {
            case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break; 
            case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
            case SQL_C_LONG:    delete (int64_t *)prm.ParameterValuePtr; break;
            case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
            case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
          }
        }
      }
      
      free(this->params);
    }

    delete columns;
    delete boundRow;

    free(this->sql);
    free(this->catalog);
    free(this->schema);
    free(this->table);
    free(this->type);
    free(this->column);
  }

} QueryData;

class ODBC {

  public:
    static uv_mutex_t g_odbcMutex;
    static SQLHENV hEnv;

    static Napi::Value Init(Napi::Env env, Napi::Object exports);

    static std::string GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle);
    static std::string GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle, const char* message);

    static SQLTCHAR* NapiStringToSQLTCHAR(Napi::String string);

    static void RetrieveData(QueryData *data);
    static void BindColumns(QueryData *data);
    static void FetchAll(QueryData *data);

    static Parameter*  GetParametersFromArray(Napi::Array *values, int *paramCount);
    static void        DetermineParameterType(Napi::Value value, Parameter *param);
    static void        BindParameters(QueryData *data);

    static Napi::Array ProcessDataForNapi(Napi::Env env, QueryData *data);

    void Free();

    ~ODBC();
    
    static Napi::Value Connect(const Napi::CallbackInfo& info);
    static Napi::Value ConnectSync(const Napi::CallbackInfo& info);

    #ifdef dynodbc
    static Napi::Value LoadODBCLibrary(const Napi::CallbackInfo& info);
    #endif
};

#ifdef DEBUG
#define DEBUG_TPRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) (void)0
#define DEBUG_TPRINTF(...) (void)0
#endif

#endif