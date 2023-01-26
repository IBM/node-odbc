/*
  Copyright (c) 2019, 2021 IBM
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

#if defined(UNICODE) && !defined(_WIN32)
#error "UNICODE builds only supported on Windows"
#endif

#ifndef _SRC_ODBC_H
#define _SRC_ODBC_H

#include <uv.h>
#include <napi.h>
#include <wchar.h>
#include <new>

#include <algorithm>

#include <stdlib.h>
#ifdef dynodbc
#include "dynodbc.h"
#else
#include <sql.h>
#include <sqlext.h>
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

#define IGNORED_PARAMETER 0

typedef struct ODBCError {
  SQLTCHAR    state[6];
  SQLINTEGER  code;
  SQLTCHAR   *message;
} ODBCError;

typedef struct GetDataExtensionsSupport {
  bool any_column;
  bool any_order;
  bool block;
  bool bound;
  bool output_params;
} GetDataExtensionsSupport;

typedef struct GetInfoResults {
  SQLSMALLINT              max_column_name_length;
  GetDataExtensionsSupport sql_get_data_supports;
  SQLUINTEGER              available_isolation_levels;
} GetInfoResults;

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
  bool          is_long_data; // set to true if data type is SQL_(W)LONG*
} Column;

typedef struct ColumnBuffer {
  SQLPOINTER  buffer;
  SQLLEN     *length_or_indicator_array;
} ColumnBuffer;

typedef struct BindData {
  SQLLEN     string_length_or_indicator;
  SQLPOINTER data;
} BindData;

// Amalgamation of the information returned by SQLDescribeParam and
// SQLProcedureColumns as well as the information needed by SQLBindParameter
typedef struct Parameter {
  SQLSMALLINT  InputOutputType; // returned by SQLProcedureColumns
  SQLSMALLINT  ValueType;
  SQLSMALLINT  ParameterType;
  SQLULEN      ColumnSize;
  SQLSMALLINT  DecimalDigits;
  SQLPOINTER   ParameterValuePtr;
  SQLLEN       BufferLength;
  SQLLEN       StrLen_or_IndPtr;
  SQLSMALLINT  Nullable;
  bool         isbigint;
} Parameter;

typedef struct ColumnData {
  SQLSMALLINT bind_type;
  bool use_free;
  union {
    SQLCHAR      *char_data;
    SQLWCHAR     *wchar_data;
    SQLDOUBLE     double_data;
    SQLCHAR       tinyint_data;
    SQLUSMALLINT  usmallint_data;
    SQLSMALLINT   smallint_data;
    SQLINTEGER    integer_data;
    SQLBIGINT     bigint_data;
  };
  SQLLEN    size;

  ~ColumnData() {
    if (bind_type == SQL_C_CHAR || bind_type == SQL_C_BINARY) {
      if (use_free) {
        free(this->char_data);
      }
      else {
        delete[] this->char_data;
      }
    }
    else if (bind_type == SQL_C_WCHAR) {
      if (use_free) {
        free(this->wchar_data);
      }
      else {
        delete[] this->wchar_data;
      }
    }
  }

} ColumnData;

#define MB_SIZE 1048576

typedef struct QueryOptions {
  bool         use_cursor                    = false;
  SQLTCHAR    *cursor_name                   = nullptr;
  SQLSMALLINT  cursor_name_length            = 0;
  SQLULEN      fetch_size                    = 1;
  SQLULEN      timeout                       = 0;
  SQLLEN       initial_long_data_buffer_size = MB_SIZE;

  // JavaScript property keys for query options
  static constexpr const char *CURSOR_PROPERTY              = "cursor";
  static constexpr const char *FETCH_SIZE_PROPERTY          = "fetchSize";
  static constexpr const char *TIMEOUT_PROPERTY             = "timeout";
  static constexpr const char *INITIAL_BUFFER_SIZE_PROPERTY = "initialBufferSize";

  void reset() {
    this->use_cursor   = false;
    this->cursor_name = nullptr;
    this->cursor_name_length = 0;
    this->fetch_size = 1;
    this->timeout = 0;
    this->initial_long_data_buffer_size = MB_SIZE;
  };

} QueryOptions;

// StatementData
typedef struct StatementData {

  SQLHENV  henv;
  SQLHDBC  hdbc;
  SQLHSTMT hstmt;

  QueryOptions query_options;

  GetDataExtensionsSupport get_data_supports;

  // parameters
  SQLSMALLINT parameterCount = 0;
  Parameter** parameters = NULL;

  // columns and rows
  bool                        simple_binding = false;
  Column                    **columns        = NULL;
  SQLSMALLINT                 column_count;
  ColumnBuffer               *bound_columns  = NULL;
  std::vector<ColumnData*>    storedRows;
  SQLLEN                      rowCount;

  SQLSMALLINT                 maxColumnNameLength;

  SQLUSMALLINT               *row_status_array;
  SQLUINTEGER                 fetch_size;
  SQLULEN                     rows_fetched;
  bool                        result_set_end_reached = false;

  bool                        fetch_array   = false;

  // query options
  SQLTCHAR *sql       = NULL;
  SQLTCHAR *catalog   = NULL;
  SQLTCHAR *schema    = NULL;
  SQLTCHAR *table     = NULL;
  SQLTCHAR *fkCatalog = NULL;
  SQLTCHAR *fkSchema  = NULL;
  SQLTCHAR *fkTable   = NULL;
  SQLTCHAR *type      = NULL;
  SQLTCHAR *column    = NULL;
  SQLTCHAR *procedure = NULL;

  ~StatementData() {
    deleteColumns();

    for (int i = 0; i < this->parameterCount; i++) {
      Parameter* parameter = this->parameters[i];
      if (parameter->ParameterValuePtr != NULL) {
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
    this->parameterCount = 0;

    delete[] sql; sql = NULL;
    delete[] this->catalog; this->catalog = NULL;
    delete[] this->schema; this->schema = NULL;
    delete[] this->table; this->table = NULL;
    delete[] this->fkCatalog; this->fkCatalog = NULL;
    delete[] this->fkSchema; this->fkSchema = NULL;
    delete[] this->fkTable; this->fkTable = NULL;
    delete[] this->type; this->type = NULL;
    delete[] this->column; this->column = NULL;
    delete[] this->procedure; this->procedure = NULL;
  }
  
  void deleteColumns() {
    for (size_t h = 0; h < this->storedRows.size(); h++) {
      delete[] storedRows[h];
    }
    this->storedRows.clear();

    for (int i = 0; i < this->column_count; i++) {
      switch (this->columns[i]->bind_type) {
        case SQL_C_CHAR:
        case SQL_C_UTINYINT:
        case SQL_C_BINARY:
          delete[] (SQLCHAR *)this->bound_columns[i].buffer;
          break;
        case SQL_C_WCHAR:
          delete[] (SQLWCHAR *)this->bound_columns[i].buffer;
          break;
        case SQL_C_DOUBLE:
          delete[] (SQLDOUBLE *)this->bound_columns[i].buffer;
          break;
        case SQL_C_USHORT:
          delete[] (SQLUSMALLINT *)this->bound_columns[i].buffer;
          break;
        case SQL_C_SLONG:
          delete[] (SQLUINTEGER *)this->bound_columns[i].buffer;
          break;
        case SQL_C_UBIGINT:
          delete[] (SQLUBIGINT *)this->bound_columns[i].buffer;
          break;
      }

      delete[] this->columns[i]->ColumnName;
      delete[] this->bound_columns[i].length_or_indicator_array;
      delete this->columns[i];
    }
    this->column_count = 0;

    delete[] row_status_array; row_status_array = NULL;
    delete[] columns; columns = NULL;
    delete[] bound_columns; bound_columns = NULL;
  }
} StatementData;

size_t strlen16(const char16_t* string);

class ODBC {

  public:
    static uv_mutex_t g_odbcMutex;
    static SQLHENV hEnv;

    static Napi::Value Init(Napi::Env env, Napi::Object exports);

    static SQLTCHAR* NapiStringToSQLTCHAR(Napi::String string);

    static void StoreBindValues(Napi::Array *values, Parameter **parameters);

    static SQLRETURN DescribeParameters(SQLHSTMT hstmt, Parameter **parameters, SQLSMALLINT parameterCount);
    static SQLRETURN  BindParameters(SQLHSTMT hstmt, Parameter **parameters, SQLSMALLINT parameterCount);
    static Napi::Array ParametersToArray(Napi::Env env, StatementData *data);

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

    bool CheckAndHandleErrors(SQLRETURN return_code, SQLSMALLINT handleType, SQLHANDLE handle, const char *message);
    ODBCError* GetODBCErrors(SQLSMALLINT handleType, SQLHANDLE handle);
    void OnError(const Napi::Error &e);
};

#endif
