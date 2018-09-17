/*
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
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

#include <napi.h>
#include <time.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"
#include <iostream>

#ifdef dynodbc
#include "dynodbc.h"
#endif

using namespace Napi;

uv_mutex_t ODBC::g_odbcMutex;

Napi::FunctionReference ODBC::constructor;

Napi::Object ODBC::Init(Napi::Env env, Napi::Object exports) {
  DEBUG_PRINTF("ODBC::Init\n");
  Napi::HandleScope scope(env);
  
  Napi::Function constructorFunction = DefineClass(env, "ODBC", {

    InstanceMethod("createConnection", &ODBC::CreateConnection),
    InstanceMethod("createConnectionSync", &ODBC::CreateConnectionSync),

    // instance values [THESE WERE 'constant_attributes' in NAN, is there an equivalent here?]
    StaticValue("SQL_CLOSE", Napi::Number::New(env, SQL_CLOSE)),
    StaticValue("SQL_DROP", Napi::Number::New(env, SQL_DROP)),
    StaticValue("SQL_UNBIND", Napi::Number::New(env, SQL_UNBIND)),
    StaticValue("SQL_RESET_PARAMS", Napi::Number::New(env, SQL_RESET_PARAMS)),
    StaticValue("SQL_DESTROY", Napi::Number::New(env, SQL_DESTROY)),
    StaticValue("FETCH_ARRAY", Napi::Number::New(env, FETCH_ARRAY)),
    StaticValue("SQL_USER_NAME", Napi::Number::New(env, SQL_USER_NAME))
    // NODE_ODBC_DEFINE_CONSTANT(constructor, FETCH_OBJECT); // TODO: MI: This was in here too... what does this MACRO really do?
  });

  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBC", constructorFunction);
  
  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);

  return exports;
}

ODBC::ODBC(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBC>(info) {
  DEBUG_PRINTF("ODBC::New\n");

  Napi::Env env = info.Env();

  this->m_hEnv = NULL;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  // Initialize the Environment handle
  int ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);

  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(ret)) {

    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
    Error(env, ODBC::GetSQLError(env, SQL_HANDLE_ENV, this->m_hEnv)).ThrowAsJavaScriptException();
    return;
  }
  
  // Use ODBC 3.x behavior
  SQLSetEnvAttr(this->m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
}

ODBC::~ODBC() {
  DEBUG_PRINTF("ODBC::~ODBC\n");
  this->Free();
}

void ODBC::Free() {
  DEBUG_PRINTF("ODBC::Free\n");
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  if (m_hEnv) {
    SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
    m_hEnv = NULL;      
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);
}

void ODBC::FetchAll(QueryData *data) {

  // continue call SQLFetch, with results going in the boundRow array
  while(SQL_SUCCEEDED(SQLFetch(data->hSTMT))) {

    ColumnData *row = new ColumnData[data->columnCount];

    // Iterate over each column, putting the data in the row object
    // Don't need to use intermediate structure in sync version
    for (int i = 0; i < data->columnCount; i++) {

      row[i].size = data->columns[i].dataLength;
      if (row[i].size == SQL_NULL_DATA) {
        row[i].data = NULL;
      } else {
        // HELP: Is it ok just to copy the memory for any type?
        row[i].data = new SQLCHAR[row[i].size];
        memcpy(row[i].data, data->boundRow[i], row[i].size);
      }
    }

    data->storedRows.push_back(row);
  }
}

void ODBC::Fetch(QueryData *data) {
  

  if (SQL_SUCCEEDED(SQLFetch(data->hSTMT))) {

    ColumnData *row = new ColumnData[data->columnCount];

    // Iterate over each column, putting the data in the row object
    // Don't need to use intermediate structure in sync version
    for (int i = 0; i < data->columnCount; i++) {

      printf("\nIn Fetch, the data->columns[%d] data length is %d", i, data->columns[i].dataLength);

      row[i].size = data->columns[i].dataLength;
      if (row[i].size == SQL_NULL_DATA) {
        row[i].data = NULL;
      } else {
        row[i].data = new SQLCHAR[row[i].size];

        switch(data->columns[i].type) {

          // TODO: Need to node-actually check the type of the column
          default:
            memcpy(row[i].data, data->boundRow[i], row[i].size);
        }
      }
    }

    data->storedRows.push_back(row);
  }
}

Napi::Array ODBC::GetNapiRowData(Napi::Env env, std::vector<ColumnData*> *storedRows, Column *columns, int columnCount, int fetchMode) {

  //Napi::HandleScope scope(env);
  Napi::Array rows = Napi::Array::New(env);

  for (unsigned int i = 0; i < storedRows->size(); i++) {

    // Arrays are a subclass of Objects
    Napi::Object row;

    if (fetchMode == FETCH_ARRAY) {
      row = Napi::Array::New(env);
    } else {
      row = Napi::Object::New(env);
    }

    ColumnData *storedRow = (*storedRows)[i];

    // Iterate over each column, putting the data in the row object
    // Don't need to use intermediate structure in sync version
    for (int j = 0; j < columnCount; j++) {

      Napi::Value value;

      // check for null data
      if (storedRow[j].size == SQL_NULL_DATA) {
        //value = env.Null();
      } else {
        switch(columns[j].type) {
          // Napi::Number
          case SQL_DECIMAL :
          case SQL_NUMERIC :
          case SQL_FLOAT :
          case SQL_REAL :
          case SQL_DOUBLE :
            value = Napi::Number::New(env, *(double*)storedRow[j].data);
            break;
            break;
          case SQL_INTEGER :
          case SQL_SMALLINT :
          case SQL_BIGINT :
            value = Napi::Number::New(env, *(int64_t*)storedRow[j].data);
            break;
          // Napi::ArrayBuffer
          case SQL_BINARY :
          case SQL_VARBINARY :
          case SQL_LONGVARBINARY :
            value = Napi::ArrayBuffer::New(env, storedRow[j].data, storedRow[j].size);
            break;
          // Napi::String (char16_t)
          case SQL_WCHAR :
          case SQL_WVARCHAR :
          case SQL_WLONGVARCHAR :
            value = Napi::String::New(env, (const char16_t*)storedRow[j].data, storedRow[j].size);
            break;
          // Napi::String (char)
          case SQL_CHAR :
          case SQL_VARCHAR :
          case SQL_LONGVARCHAR :
          default:
            value = Napi::String::New(env, (const char*)storedRow[j].data, storedRow[j].size);
            break;
        }

        if (fetchMode == FETCH_ARRAY) {
          row.Set(j, value);
        } else {
          row.Set(Napi::String::New(env, (const char*)columns[j].name), value);
        }
      }

      delete storedRow[j].data;
      //delete storedRow;
    }
    rows.Set(i, row);
  }

  storedRows->clear();

  return rows;
}

// MI: GetNapiColumns is an unused method that I used for an extension where result could also just return column names

// Napi::Object ODBC::GetNapiColumns(Napi::Env env, Column *columns, int columnCount) {

//   Napi::Object fields = Napi::Object::New(env);
    
//   for (int i = 0; i < columnCount; i++) {

//     #ifdef UNICODE
//       fields.Set(Napi::String::New(env, (char16_t*) columns[i].name),
//                       Napi::Number::New(env, columns[i].type));
//     #else
//       fields.Set(Napi::String::New(env, (char*) columns[i].name),
//                       Napi::Number::New(env, columns[i].type));
//     #endif

//   }

//   return fields;
// }

/******************************************************************************
 ****************************** BINDING COLUMNS *******************************
 *****************************************************************************/

Column* ODBC::GetColumns(SQLHSTMT hStmt, SQLSMALLINT *colCount) {

  SQLRETURN sqlReturnCode;

  //get the number of columns in the result set
  sqlReturnCode = SQLNumResultCols(hStmt, colCount);
  
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    *colCount = 0;
    return new Column[0];
  }
  
  Column *columns = new Column[*colCount];

  for (int i = 0; i < *colCount; i++) {

    columns[i].index = i + 1; // Column number of result data, starting at 1

#ifdef UNICODE
    columns[i].name = new SQLWCHAR[SQL_MAX_COLUMN_NAME_LEN];
    sqlReturnCode = SQLDescribeColW(
      hStmt,                    // StatementHandle
      columns[i].index,         // ColumnNumber
      columns[i].name,          // ColumnName
      SQL_MAX_COLUMN_NAME_LEN,  // BufferLength,  
      &(columns[i].nameSize),   // NameLengthPtr,
      &(columns[i].type),       // DataTypePtr
      &(columns[i].precision),  // ColumnSizePtr,
      &(columns[i].scale),      // DecimalDigitsPtr,
      &(columns[i].nullable)    // NullablePtr
    );
#else
    columns[i].name = new SQLCHAR[SQL_MAX_COLUMN_NAME_LEN];
    sqlReturnCode = SQLDescribeCol(
      hStmt,                    // StatementHandle
      columns[i].index,         // ColumnNumber
      columns[i].name,          // ColumnName
      SQL_MAX_COLUMN_NAME_LEN,  // BufferLength,  
     &columns[i].nameSize,      // NameLengthPtr,
     &columns[i].type,          // DataTypePtr
     &columns[i].precision,     // ColumnSizePtr,
     &columns[i].scale,         // DecimalDigitsPtr,
     &columns[i].nullable       // NullablePtr
    );
#endif
  }


  if (SQL_SUCCEEDED(sqlReturnCode)) {
    return columns;
  } else {
    // TODO: Throw a Javascript error here
  }

  return columns;
}

/*
 *  ODBC::BindColumnData
 *    Description: After a query is executed, this function binds the result
 *                 columns to buffers. The type and size of these buffers
 *                 depends on the column type, which is obtained from
 *                 ODBC::GetColumns.
 * 
 *    Parameters:
 *      HSTMT hSTMT:
 *        The statement handle passed around during for this SQL statement.
 * 
 *      Column *columns:
 *        The columns that were obtained from ODBC::GetColumns. Contains a
 *        pointer to data length that is updated with every SQLFetch to know
 *        how much of the buffer is real data.
 * 
 *      SQLSMALLINT *columnCount:
 *        The number of columns in the result set.
 * 
 *    Return:
 *      SQLCHAR**:
 *        The data from each column in the row. Each column is represented by
 *        a SQLCHAR*, and then the SQLCHAR** has pointers to each column.
 */
SQLCHAR** ODBC::BindColumnData(HSTMT hSTMT, Column *columns, SQLSMALLINT *columnCount) {

  SQLCHAR **rowData = new SQLCHAR*[*columnCount];
  SQLLEN maxColumnLength;
  SQLSMALLINT targetType;

  SQLRETURN sqlReturnCode;

  for (int i = 0; i < *columnCount; i++)
  {
    // TODO: These are taken from idb-connector. Make sure we handle all ODBC cases
    switch(columns[i].type) {

      case SQL_DECIMAL :
      case SQL_NUMERIC :

        maxColumnLength = columns[i].precision;
        targetType = SQL_C_CHAR;
        break;

      case SQL_INTEGER :

        maxColumnLength = columns[i].precision;
        targetType = SQL_C_LONG;
        break;

      case SQL_BIGINT :

       maxColumnLength = columns[i].precision;
       targetType = SQL_C_SBIGINT;
       break;

      case SQL_BINARY :
      case SQL_VARBINARY :
      case SQL_LONGVARBINARY :

        maxColumnLength = columns[i].precision;
        targetType = SQL_C_BINARY;
        break;

      case SQL_WCHAR :
      case SQL_WVARCHAR :

        maxColumnLength = (columns[i].precision << 2) + 1;
        targetType = SQL_C_CHAR;
        break;

      default:
      
        maxColumnLength = columns[i].precision + 1;
        targetType = SQL_C_CHAR;
        break;
    }

    rowData[i] = new SQLCHAR[maxColumnLength];

    sqlReturnCode = SQLBindCol(
      hSTMT,                    // StatementHandle
      i + 1,                    // ColumnNumber
      targetType,               // TargetType
      rowData[i],               // TargetValuePtr
      maxColumnLength,          // BufferLength
      &(columns[i].dataLength)  // StrLen_or_Ind
    );

    if (SQL_SUCCEEDED(sqlReturnCode)) {
      continue;
    } else {
      // TODO: Throw error here
      return nullptr;
    }
  }

  return rowData;
}

/*
 * FreeColumns
 */

void ODBC::FreeColumns(Column *columns, SQLSMALLINT *colCount) {
  for(int i = 0; i < *colCount; i++) {
      delete [] columns[i].name;
  }

  delete [] columns;
  
  *colCount = 0;
}

/******************************************************************************
 **************************** BINDING PARAMETERS ******************************
 *****************************************************************************/

/*
 * GetParametersFromArray
 */
Parameter* ODBC::GetParametersFromArray(Napi::Array *values, int *paramCount) {

  DEBUG_PRINTF("ODBC::GetParametersFromArray\n");

  *paramCount = values->Length();
  
  Parameter* params = NULL;
  
  if (*paramCount > 0) {
    params = new Parameter[*paramCount];
  }
  
  for (int i = 0; i < *paramCount; i++) {

    Napi::Value value;
    Napi::Number ioType;

    Napi::Value param = values->Get(i);

    // these are the default values, overwritten in some cases
    params[i].ColumnSize       = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;
    params[i].BufferLength     = 0;
    params[i].DecimalDigits    = 0;

    value = param;
    params[i].InputOutputType = SQL_PARAM_INPUT;

    ODBC::DetermineParameterType(value, &params[i]);
  } 
  
  return params;
}

// This is for a future extension to explicitly set parameter types

// void ODBC::SetParameterType(Napi::Value value, SQLSMALLINT parameterType, Parameter *param) {

//   printf("\nSetParameterType");
//   printf("\nSizeof(DATE_STRUCT) is %d", sizeof(DATE_STRUCT));
//   printf("\nSizeof(SQL_TIME_STRUCT) is %d", sizeof(SQL_TIME_STRUCT));
//   printf("\nSizeof(SQL_TIMESTAMP_STRUCT) is %d", sizeof(SQL_TIMESTAMP_STRUCT));

//   param->ParameterType = parameterType;

//   // Default values, overwritten as needed
//   param->ColumnSize       = 0;
//   param->StrLen_or_IndPtr = SQL_NULL_DATA;
//   param->BufferLength     = 0;
//   param->DecimalDigits    = 0;

//   switch (parameterType) {

//     case SQL_CHAR:
//     case SQL_VARCHAR:
//     case SQL_LONGVARCHAR: {
      
//       std::string string       = value.ToString().Utf8Value(); 
//       char *str                = strdup(string.c_str());
//       param->ValueType         = SQL_C_CHAR;
//       param->ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
//       param->BufferLength      = string.size() + 1;
//       param->ParameterValuePtr = str;
//       param->StrLen_or_IndPtr  = SQL_NTS; //param->BufferLength;
//       break;
//     }

//     case SQL_WCHAR:
//     case SQL_WVARCHAR:
//     case SQL_WLONGVARCHAR: {

//       // HELP: Do I need to have a poitner to a char16_t here?
//       std::string string       = value.ToString().Utf8Value(); 
//       char *str                = strdup(string.c_str());
//       // std::u16string string    = value.ToString().Utf16Value(); 
//       // char16_t *str            = string.pointer;
//       param->ValueType         = SQL_C_WCHAR;
//       param->ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
//       param->BufferLength      = (string.size() * sizeof(SQLWCHAR)) + sizeof(SQLWCHAR);
//       param->ParameterValuePtr = str;
//       param->StrLen_or_IndPtr  = SQL_NTS; //param->BufferLength;
//       break;
//     }
    
//     case SQL_DECIMAL:
//     case SQL_NUMERIC: {

//       Napi::String string = value.ToString();
//       SQLCHAR *number = new SQLCHAR(*(const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(string.Utf8Value().c_str()))));

//       param->BufferLength = string.Utf8Value().length() + 1;
//       param->ValueType = SQL_C_CHAR;
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr = SQL_NTS;
//     }

//     case SQL_SMALLINT: {
//       SQLSMALLINT *number = new SQLSMALLINT(value.As<Napi::Number>().Int32Value());
//       param->ValueType = SQL_C_SHORT;
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr = 0;
//       break;
//     }

//     case SQL_INTEGER: {
//       SQLINTEGER *number = new SQLINTEGER(value.As<Napi::Number>().Int32Value());
//       param->ValueType         = SQL_C_SLONG;
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr  = 0;
//       param->ColumnSize        = sizeof(SQLINTEGER);
//       break;
//     }

//     case SQL_BIGINT: {
//       SQLBIGINT *number = new SQLBIGINT(value.As<Napi::Number>().Int64Value());
//       param->ValueType = SQL_C_SBIGINT;
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr = 0;
//       break;
//     }

//     case SQL_REAL:
//     case SQL_FLOAT:
//     case SQL_DOUBLE: {
//       SQLDOUBLE *number = new SQLDOUBLE(value.As<Napi::Number>().DoubleValue());
//       param->ValueType         = SQL_C_DOUBLE;
//       param->BufferLength      = sizeof(SQLDOUBLE);
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr  = param->BufferLength;
//       param->DecimalDigits     = 7;
//       param->ColumnSize        = sizeof(SQLDOUBLE);
//       break;
//     }

//     case SQL_BIT: {
//       SQLCHAR *number = new SQLCHAR(value.As<Napi::Number>().Int32Value());
//       param->ValueType         = SQL_C_BIT;
//       param->BufferLength      = sizeof(SQLCHAR);
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr  = param->BufferLength;
//       break;
//     }

//     case SQL_TINYINT: {
//       SQLCHAR *number = new SQLCHAR(value.As<Napi::Number>().Int32Value());
//       param->ValueType         = SQL_C_STINYINT;
//       param->BufferLength      = sizeof(SQLCHAR);
//       param->ParameterValuePtr = number;
//       param->StrLen_or_IndPtr  = param->BufferLength;
//       break;
//     }

//     case SQL_BINARY:
//     case SQL_VARBINARY:
//     case SQL_LONGVARBINARY: {
//       // Napi::ArrayBuffer data = new ArrayBuffer(Napi::Env::Global(), value);
//       // void *bufferData = data.Data();

//       std::string string       = value.ToString().Utf8Value(); 
//       char *str                = strdup(string.c_str());

//       param->ValueType         = SQL_C_BINARY;
//       param->ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
//       param->BufferLength      = string.size() + 1;
//       param->ParameterValuePtr = str;
//       param->StrLen_or_IndPtr  = SQL_NTS; //param->BufferLength;
//       break;
//     }

//     case SQL_TYPE_DATE: {

//       DATE_STRUCT *date = new DATE_STRUCT;

//       param->ValueType         = SQL_C_TYPE_DATE;
//       param->BufferLength      = sizeof(DATE_STRUCT);
//       param->ParameterValuePtr = date;
//       param->StrLen_or_IndPtr  = SQL_NTS;
//       break;
//     }

//     case SQL_TYPE_TIME: {

//       SQL_TIME_STRUCT *time = new SQL_TIME_STRUCT;

//       param->ValueType         = SQL_C_TYPE_TIME;
//       param->BufferLength      = sizeof(SQL_TIME_STRUCT);
//       param->ParameterValuePtr = time;
//       param->StrLen_or_IndPtr  = SQL_NTS;
//       break;   
//     }

//     case SQL_TYPE_TIMESTAMP: {

//       SQL_TIMESTAMP_STRUCT *timestamp = new SQL_TIMESTAMP_STRUCT;

//       param->ValueType         = SQL_C_TYPE_TIMESTAMP;
//       param->BufferLength      = sizeof(SQL_TIMESTAMP_STRUCT);
//       param->ParameterValuePtr = timestamp;
//       param->StrLen_or_IndPtr  = SQL_NTS;
//       break;      
//     }

//     default:
//       break;
//   }
// }

void ODBC::DetermineParameterType(Napi::Value value, Parameter *param) {

  if (value.IsNull()) {

      param->ValueType = SQL_C_DEFAULT;
      param->ParameterType   = SQL_VARCHAR;
      param->StrLen_or_IndPtr = SQL_NULL_DATA;
  }
  else if (value.IsString()) {

    Napi::String string = value.ToString();

    param->ValueType         = SQL_C_TCHAR;
    param->ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
    #ifdef UNICODE
          param->ParameterType     = SQL_WVARCHAR;
          param->BufferLength      = (string.Utf16Value().length() * sizeof(char16_t)) + sizeof(char16_t);
    #else
          param->ParameterType     = SQL_VARCHAR;
          param->BufferLength      = string.Utf8Value().length() + 1;
    #endif
          param->ParameterValuePtr = malloc(param->BufferLength);
          param->StrLen_or_IndPtr  = SQL_NTS; //param->BufferLength;

    #ifdef UNICODE
          strcpy((char *) param->ParameterValuePtr, string.Utf8Value().c_str());
    #else
          strcpy((char *) param->ParameterValuePtr, string.Utf8Value().c_str()); 

    #endif

    DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%s\n",
                  i, param->ValueType, param->ParameterType,
                  param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr, 
                  (char*) param->ParameterValuePtr);
  }
  else if (value.IsNumber()) {
    // check whether it is an INT or a Double
    double orig_val = value.As<Napi::Number>().DoubleValue();
    int64_t int_val = value.As<Napi::Number>().Int64Value();

    if (orig_val == int_val) {
      // is an integer
      int64_t  *number = new int64_t(value.As<Napi::Number>().Int64Value());
      param->ValueType = SQL_C_SBIGINT;
      param->ParameterType   = SQL_BIGINT;
      param->ParameterValuePtr = number;
      param->StrLen_or_IndPtr = 0;

      printf("\nTrying to get the number, the number is %d", *number);
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%lld\n",
                    i, param->ValueType, param->ParameterType,
                    param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr,
                    *number);
    } else {
      // not an integer
      double *number   = new double(value.As<Napi::Number>().DoubleValue());
    
      param->ValueType         = SQL_C_DOUBLE;
      param->ParameterType     = SQL_DECIMAL;
      param->ParameterValuePtr = number;
      param->BufferLength      = sizeof(double);
      param->StrLen_or_IndPtr  = param->BufferLength;
      param->DecimalDigits     = 7;
      param->ColumnSize        = sizeof(double);

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNumber(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%f\n",
                    i, param->ValueType, param->ParameterType,
                    param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr,
                    *number);
    }
  }
  else if (value.IsBoolean()) {
    bool *boolean = new bool(value.As<Napi::Boolean>().Value());
    param->ValueType         = SQL_C_BIT;
    param->ParameterType     = SQL_BIT;
    param->ParameterValuePtr = boolean;
    param->StrLen_or_IndPtr  = 0;
    
    DEBUG_PRINTF("ODBC::GetParametersFromArray - IsBoolean(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli\n",
                  i, param->ValueType, param->ParameterType,
                  param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr);
  }
  else {
    // TODO
  }
}

void ODBC::BindParameters(QueryData *data) {

  for (int i = 0; i < data->paramCount; i++) {

    Parameter prm = data->params[i];

    DEBUG_TPRINTF(
      SQL_T("ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%i size=%i\n"), i, prm.ValueType, prm.ParameterType,
      prm.BufferLength, prm.ColumnSize);

    data->sqlReturnCode = SQLBindParameter(
      data->hSTMT,                        // StatementHandle
      i + 1,                              // ParameterNumber
      prm.InputOutputType,                // InputOutputType
      prm.ValueType,                      // ValueType
      prm.ParameterType,                  // ParameterType
      prm.ColumnSize,                     // ColumnSize
      prm.DecimalDigits,                  // DecimalDigits
      prm.ParameterValuePtr,              // ParameterValuePtr
      prm.BufferLength,                   // BufferLength
      &data->params[i].StrLen_or_IndPtr); // StrLen_or_IndPtr

    if (data->sqlReturnCode == SQL_ERROR) {
      return;
    }
  }
}

Napi::Array ODBC::GetNapiParameters(Napi::Env env, Parameter *parameters, int parameterCount) {

  Napi::Array parametersObject = Napi::Array::New(env);

  for (int i = 0; i < parameterCount; i++) {

    if (parameters[i].InputOutputType != SQL_PARAM_INPUT) {

      if (parameters[i].ParameterType == SQL_BIGINT) {
        parametersObject.Set(Napi::Number::New(env, i), *(int64_t *)parameters[i].ParameterValuePtr);
      } else {
        parametersObject.Set(Napi::Number::New(env, i), *(double *)parameters[i].ParameterValuePtr);
      }
    }
  }

  return parametersObject;
}

/*
 * CreateConnection
 */

class CreateConnectionAsyncWorker : public Napi::AsyncWorker {

  public:
    CreateConnectionAsyncWorker(ODBC *odbcObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcObject(odbcObject) {}

    ~CreateConnectionAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBC::CreateConnectionAsyncWorker::Execute\n");
  
      uv_mutex_lock(&ODBC::g_odbcMutex);
      //allocate a new connection handle
      sqlReturnCode = SQLAllocHandle(SQL_HANDLE_DBC, odbcObject->m_hEnv, &hDBC);
      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBC::CreateConnectionAsyncWorker::OnOk\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // return the SQLError
      if (!SQL_SUCCEEDED(sqlReturnCode)) {

        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_ENV, odbcObject->m_hEnv)); // callbackArguments[0]
        
        Callback().Call(callbackArguments);
      }
      // return the Connection
      else {

        // pass the HENV and HDBC values to the ODBCConnection constructor
        std::vector<napi_value> connectionArguments;
        connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &(odbcObject->m_hEnv))); // connectionArguments[0]
        connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC));   // connectionArguments[1]
        
        // create a new ODBCConnection object as a Napi::Value
        Napi::Value connectionObject = ODBCConnection::constructor.New(connectionArguments);

        // pass the arguments to the callback function
        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());       // callbackArguments[0]  
        callbackArguments.push_back(connectionObject); // callbackArguments[1]

        Callback().Call(callbackArguments);
      }
    }

  private:
    ODBC *odbcObject;
    SQLRETURN sqlReturnCode;
    SQLHDBC hDBC;
};

Napi::Value ODBC::CreateConnection(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CreateConnectionAsyncWorker *worker = new CreateConnectionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBC::CreateConnectionSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode;
  SQLHDBC hDBC;

  uv_mutex_lock(&ODBC::g_odbcMutex);
  sqlReturnCode = SQLAllocHandle(SQL_HANDLE_DBC, this->m_hEnv, &hDBC);
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  // return the SQLError
  if (!SQL_SUCCEEDED(sqlReturnCode)) {

    Error(env, ODBC::GetSQLError(env, SQL_HANDLE_ENV, this->m_hEnv)).ThrowAsJavaScriptException();
    return env.Null();
  }
  // return the Connection
  else {

    // pass the HENV and HDBC values to the ODBCConnection constructor
    std::vector<napi_value> connectionArguments;
    connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &(this->m_hEnv))); // connectionArguments[0]
    connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC));   // connectionArguments[1]
    
    // create a new ODBCConnection object as a Napi::Value
    return ODBCConnection::constructor.New(connectionArguments);
  }
}

/*
 * CallbackSQLError
 */

Napi::Value ODBC::CallbackSQLError(Napi::Env env, SQLSMALLINT handleType,SQLHANDLE handle, Napi::Function* cb) {

  Napi::HandleScope scope(env);
  
  return CallbackSQLError(env, handleType, handle, "[node-odbc] SQL_ERROR", cb);
}

Napi::Value ODBC::CallbackSQLError(Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, const char* message, Napi::Function* cb) {

  Napi::HandleScope scope(env);
  
  Napi::Value objError = ODBC::GetSQLError(env, handleType, handle, message);
  
  std::vector<napi_value> callbackArguments;
  callbackArguments.push_back(objError);
  cb->Call(callbackArguments);
  
  return env.Undefined();
}

/*
 * GetSQLError
 */

Napi::Object ODBC::GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle) {

  //Napi::HandleScope scope(env);

  Napi::Object objError = GetSQLError(env, handleType, handle, "[node-odbc] SQL_ERROR");

  return objError;
}

Napi::Object ODBC::GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, const char* message) {

  //Napi::HandleScope scope(env);
  
  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);
  
  Napi::Object objError = Napi::Object::New(env);

  int32_t i = 0;
  SQLINTEGER native;
  
  SQLSMALLINT len;
  SQLINTEGER statusRecCount;
  SQLRETURN ret;
  char errorSQLState[14];
  char errorMessage[ERROR_MESSAGE_BUFFER_BYTES];

  ret = SQLGetDiagField(
    handleType,
    handle,
    0,
    SQL_DIAG_NUMBER,
    &statusRecCount,
    SQL_IS_INTEGER,
    &len);

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", ret, statusRecCount);

  Napi::Array errors = Napi::Array::New(env);

  objError.Set(Napi::String::New(env, "errors"), errors);
  
  for (i = 0; i < statusRecCount; i++){

    DEBUG_PRINTF("ODBC::GetSQLError : calling SQLGetDiagRec; i=%i, statusRecCount=%i\n", i, statusRecCount);
    
    ret = SQLGetDiagRec(
      handleType, 
      handle,
      (SQLSMALLINT)(i + 1), 
      (SQLTCHAR *) errorSQLState,
      &native,
      (SQLTCHAR *) errorMessage,
      ERROR_MESSAGE_BUFFER_CHARS,
      &len);
    
    DEBUG_PRINTF("ODBC::GetSQLError : after SQLGetDiagRec; i=%i\n", i);

    if (SQL_SUCCEEDED(ret)) {
      DEBUG_PRINTF("ODBC::GetSQLError : errorMessage=%s, errorSQLState=%s\n", errorMessage, errorSQLState);
      
      if (i == 0) {
        // First error is assumed the primary error
        objError.Set(Napi::String::New(env, "error"), Napi::String::New(env, message));
#ifdef UNICODE
        //objError.SetPrototype(Exception::Error(Napi::String::New(env, (char16_t *) errorMessage)));
        objError.Set(Napi::String::New(env, "message"), Napi::String::New(env, (char16_t *) errorMessage));
        objError.Set(Napi::String::New(env, "state"), Napi::String::New(env, (char16_t *) errorSQLState));
#else
        //objError.SetPrototype(Exception::Error(Napi::String::New(env, errorMessage)));
        objError.Set(Napi::String::New(env, "message"), Napi::String::New(env, errorMessage));
        objError.Set(Napi::String::New(env, "state"), Napi::String::New(env, errorSQLState));
#endif
      }

      Napi::Object subError = Napi::Object::New(env);

#ifdef UNICODE
      subError.Set(Napi::String::New(env, "message"), Napi::String::New(env, (char16_t *) errorMessage));
      subError.Set(Napi::String::New(env, "state"), Napi::String::New(env, (char16_t *) errorSQLState));
#else
      subError.Set(Napi::String::New(env, "message"), Napi::String::New(env, errorMessage));
      subError.Set(Napi::String::New(env, "state"), Napi::String::New(env, errorSQLState));
#endif
      errors.Set(Napi::String::New(env, std::to_string(i)), subError);

    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  if (statusRecCount == 0) {
    //Create a default error object if there were no diag records
    objError.Set(Napi::String::New(env, "error"), Napi::String::New(env, message));
    //objError.SetPrototype(Napi::Error(Napi::String::New(env, message)));
    objError.Set(Napi::String::New(env, "message"), Napi::String::New(env, 
      (const char *) "[node-odbc] An error occurred but no diagnostic information was available."));
  }

  return objError;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {

  ODBC::Init(env, exports);
  ODBCConnection::Init(env, exports);
  ODBCStatement::Init(env, exports);
  ODBCResult::Init(env, exports);

  std::vector<Napi::PropertyDescriptor> ODBC_VALUES;

  // type values
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_CHAR", Napi::Number::New(env, SQL_CHAR)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_VARCHAR", Napi::Number::New(env, SQL_VARCHAR)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_LONGVARCHAR", Napi::Number::New(env, SQL_LONGVARCHAR)));

  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_BIGINT", Napi::Number::New(env, SQL_BIGINT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_BIT", Napi::Number::New(env, SQL_BIT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_INTEGER", Napi::Number::New(env, SQL_INTEGER)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_NUMERIC", Napi::Number::New(env, SQL_NUMERIC)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_SMALLINT", Napi::Number::New(env, SQL_SMALLINT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_TINYINT", Napi::Number::New(env, SQL_TINYINT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_BIT", Napi::Number::New(env, SQL_BIT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_INTEGER", Napi::Number::New(env, SQL_INTEGER)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_DECIMAL", Napi::Number::New(env, SQL_DECIMAL)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_DOUBLE", Napi::Number::New(env, SQL_DOUBLE)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_FLOAT", Napi::Number::New(env, SQL_FLOAT)));


  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_BINARY", Napi::Number::New(env, SQL_BINARY)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_VARBINARY", Napi::Number::New(env, SQL_VARBINARY)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_LONGVARBINARY", Napi::Number::New(env, SQL_LONGVARBINARY)));

  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_DATE", Napi::Number::New(env, SQL_TYPE_DATE)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_TIME", Napi::Number::New(env, SQL_TYPE_TIME)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_TIMESTAMP", Napi::Number::New(env, SQL_TYPE_TIMESTAMP)));

  // binding values
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT", Napi::Number::New(env, SQL_PARAM_INPUT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT_OUTPUT", Napi::Number::New(env, SQL_PARAM_INPUT_OUTPUT)));
  ODBC_VALUES.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_OUTPUT", Napi::Number::New(env, SQL_PARAM_OUTPUT)));

  exports.DefineProperties(ODBC_VALUES);

  return exports;

  #ifdef dynodbc
    exports.Set(Napi::String::New(env, "loadODBCLibrary"),
                Napi::Function::New(env, ODBC::LoadODBCLibrary);());
  #endif
}

#ifdef dynodbc
Napi::Value ODBC::LoadODBCLibrary(const Napi::CallbackInfo& info) {
  Napi::HandleScope scope(env);
  
  REQ_STR_ARG(0, js_library);
  
  bool result = DynLoadODBC(*js_library);
  
  return (result) ? env.True() : env.False();W
}
#endif

NODE_API_MODULE(odbc_bindings, InitAll)
