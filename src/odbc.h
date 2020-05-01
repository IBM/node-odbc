/*
  Copyright (c) 2019, IBM
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

#include <algorithm>

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

#define FETCH_ARRAY 3
#define FETCH_OBJECT 4
#define SQL_DESTROY 9999

typedef struct ODBCError {
  SQLTCHAR    *state;
  SQLINTEGER  code;
  SQLTCHAR    *message;
} ODBCError;

typedef struct ConnectionOptions {
  unsigned int connectionTimeout;
  unsigned int loginTimeout;
  bool         fetchArray;
} ConnectionOptions;

typedef struct Column {
  SQLUSMALLINT  index;
  SQLTCHAR     *ColumnName = NULL;
  SQLSMALLINT   BufferLength;
  SQLSMALLINT   NameLength;
  SQLSMALLINT   DataType;
  SQLULEN       ColumnSize;
  SQLSMALLINT   DecimalDigits;
  SQLLEN        StrLen_or_IndPtr;
  SQLSMALLINT   Nullable;
  // data used when binding to the column
  SQLSMALLINT   bind_type;   // when unraveling ColumnData
  SQLLEN        buffer_size; // size of the buffer bound
} Column;

// Amalgamation of the information returned by SQLDescribeParam and
// SQLProcedureColumns as well as the information needed by SQLBindParameter
typedef struct Parameter {
  SQLSMALLINT InputOutputType; // returned by SQLProcedureColumns
  SQLSMALLINT ValueType;
  SQLSMALLINT ParameterType;
  SQLULEN     ColumnSize;
  SQLSMALLINT DecimalDigits;
  SQLPOINTER  ParameterValuePtr;
  SQLLEN      BufferLength;
  SQLLEN      StrLen_or_IndPtr;
  SQLSMALLINT Nullable;
} Parameter;

typedef struct ColumnData {
  SQLSMALLINT bind_type;
  union {
    SQLCHAR     *char_data;
    SQLWCHAR    *wchar_data;
    SQLDOUBLE    double_data;
    SQLCHAR      tinyint_data;
    SQLSMALLINT  smallint_data;
    SQLINTEGER   integer_data;
    SQLUBIGINT   ubigint_data;
  };
  SQLLEN    size;

  ~ColumnData() {
    if (bind_type == SQL_C_CHAR) {
      delete[] this->char_data;
      return;
    }
    if (bind_type == SQL_C_WCHAR) {
      delete[] this->wchar_data;
      return;
    }
  }

} ColumnData;

// QueryData
typedef struct QueryData {

  SQLHSTMT hSTMT;

  // parameters
  SQLSMALLINT parameterCount = 0; // returned by SQLNumParams
  SQLSMALLINT bindValueCount = 0; // number of values passed from JavaScript
  Parameter** parameters = NULL;

  // columns and rows
  Column                   **columns = NULL;
  SQLSMALLINT                columnCount;
  void                     **boundRow = NULL;
  std::vector<ColumnData*>   storedRows;
  SQLLEN                     rowCount;

  SQLSMALLINT                maxColumnNameLength;

  // query options
  SQLTCHAR *sql       = NULL;
  SQLTCHAR *catalog   = NULL;
  SQLTCHAR *schema    = NULL;
  SQLTCHAR *table     = NULL;
  SQLTCHAR *type      = NULL;
  SQLTCHAR *column    = NULL;
  SQLTCHAR *procedure = NULL;

  SQLRETURN sqlReturnCode;

  ~QueryData() {
    this->clear();
  }

  void deleteColumns() {
    if (this->columnCount > 0) {
      for (int i = 0; i < this->columnCount; i++) {
        delete this->columns[i]->ColumnName;
        delete this->columns[i];
      }
    }

    delete columns; columns = NULL;
    delete boundRow; boundRow = NULL;
    delete sql; sql = NULL;
    this->columnCount = 0;
    this->storedRows.clear();
  }

  void clear() {

    for (size_t h = 0; h < this->storedRows.size(); h++) {
      delete[] storedRows[h];
    };

    int numParameters = std::max<SQLSMALLINT>(this->bindValueCount, this->parameterCount);

    if (numParameters > 0) {

      Parameter* parameter;

      for (int i = 0; i < numParameters; i++) {
        if (parameter = this->parameters[i], parameter->ParameterValuePtr != NULL) {
          switch (parameter->ValueType) {
            case SQL_C_SBIGINT:
              delete (int64_t*)parameter->ParameterValuePtr;
              break;
            case SQL_C_DOUBLE:
              delete (double*)parameter->ParameterValuePtr;
              break;
            case SQL_C_BIT:
              delete (bool*)parameter->ParameterValuePtr;
              break;
            case SQL_C_TCHAR:
            default:
              delete[] (SQLTCHAR*)parameter->ParameterValuePtr;
              break;
          }
        }
        parameter->ParameterValuePtr = NULL;
        delete parameter;
      }

      delete[] this->parameters; this->parameters = NULL;
      this->bindValueCount = 0;
      this->parameterCount = 0;
    }

    if (this->columnCount > 0) {
      for (int i = 0; i < this->columnCount; i++) {
        switch (this->columns[i]->bind_type) {
          case SQL_C_CHAR:
          case SQL_C_UTINYINT:
          case SQL_C_BINARY:
            delete[] (SQLCHAR *)this->boundRow[i];
            break;
          case SQL_C_WCHAR:
            delete[] (SQLWCHAR *)this->boundRow[i];
            break;
          case SQL_C_DOUBLE:
            delete[] (SQLDOUBLE *)this->boundRow[i];
            break;
          case SQL_C_USHORT:
            delete[] (SQLUSMALLINT *)this->boundRow[i];
            break;
          case SQL_C_SLONG:
            delete[] (SQLUINTEGER *)this->boundRow[i];
            break;
          case SQL_C_UBIGINT:
            delete[] (SQLUBIGINT *)this->boundRow[i];
            break;
        }
        delete[] this->columns[i]->ColumnName;
        delete this->columns[i];
      }
    }

    delete[] columns; columns = NULL;
    delete[] boundRow; boundRow = NULL;

    delete[] this->sql; this->sql = NULL;
    delete[] this->catalog; this->catalog = NULL;
    delete[] this->schema; this->schema = NULL;
    delete[] this->table; this->table = NULL;
    delete[] this->type; this->type = NULL;
    delete[] this->column; this->column = NULL;
    delete[] this->procedure; this->procedure = NULL;
  }

} QueryData;

class ODBC {

  public:
    static uv_mutex_t g_odbcMutex;
    static SQLHENV hEnv;

    static Napi::Value Init(Napi::Env env, Napi::Object exports);

    static SQLTCHAR* NapiStringToSQLTCHAR(Napi::String string);

    static void StoreBindValues(Napi::Array *values, Parameter **parameters);

    static SQLRETURN DescribeParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount);
    static SQLRETURN  BindParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount);
    static Napi::Array ParametersToArray(Napi::Env env, QueryData *data);

    void Free();

    ~ODBC();

    static Napi::Value Connect(const Napi::CallbackInfo& info);

    #ifdef dynodbc
    static Napi::Value LoadODBCLibrary(const Napi::CallbackInfo& info);
    #endif
};

class ODBCAsyncWorker : public Napi::AsyncWorker {

  public:
    ODBCAsyncWorker(Napi::Function& callback);
    // ~ODBCAsyncWorker(); // TODO: Delete error stuff

  protected:
    ODBCError *errors;
    SQLINTEGER errorCount = 0;

    bool CheckAndHandleErrors(SQLRETURN returnCode, SQLSMALLINT handleType, SQLHANDLE handle, const char *message);
    ODBCError* GetODBCErrors(SQLSMALLINT handleType, SQLHANDLE handle);
    void OnError(const Napi::Error &e);
};

#ifdef DEBUG
#define DEBUG_TPRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) (void)0
#define DEBUG_TPRINTF(...) (void)0
#endif

#endif