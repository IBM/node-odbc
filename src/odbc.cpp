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

#include <string.h>
#include <napi.h>
#include <node_version.h>
#include <time.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

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
    InstanceValue("SQL_CLOSE", Napi::Number::New(env, SQL_CLOSE)),
    InstanceValue("SQL_DROP", Napi::Number::New(env, SQL_DROP)),
    InstanceValue("SQL_UNBIND", Napi::Number::New(env, SQL_UNBIND)),
    InstanceValue("SQL_RESET_PARAMS", Napi::Number::New(env, SQL_RESET_PARAMS)),
    InstanceValue("SQL_DESTROY", Napi::Number::New(env, SQL_DESTROY)),
    InstanceValue("FETCH_ARRAY", Napi::Number::New(env, FETCH_ARRAY)),
    InstanceValue("SQL_USER_NAME", Napi::Number::New(env, SQL_USER_NAME))
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

  printf("\nODBC::ODBC() set alloc handle response is %d", ret);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(ret)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
    Napi::Value objError = ODBC::GetSQLError(env, SQL_HANDLE_ENV, this->m_hEnv);
    
    Napi::Error(env, objError).ThrowAsJavaScriptException();
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
  if (m_hEnv) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (m_hEnv) {
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hEnv = NULL;      
    }

    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

// Napi::Value ODBC::New(const Napi::CallbackInfo& info) {

//   DEBUG_PRINTF("ODBC::New\n");

//   Napi::Env env = info.Env();
//   Napi::HandleScope scope(env);
//   Napi::Object dbo = new ODBC(info);
//   //ODBC* dbo = new ODBC(info);
  
//   //dbo->Wrap(info.Holder());

//   dbo->m_hEnv = NULL;
  
//   uv_mutex_lock(&ODBC::g_odbcMutex);
  
//   // Initialize the Environment handle
//   int ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &dbo->m_hEnv);
  
//   uv_mutex_unlock(&ODBC::g_odbcMutex);
  
//   if (!SQL_SUCCEEDED(ret)) {
//     DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
//     Napi::Value objError = ODBC::GetSQLError(SQL_HANDLE_ENV, dbo->m_hEnv);
    
//     Napi::Error(env, objError).ThrowAsJavaScriptException();
//     return env.Null();
//   }
  
//   // Use ODBC 3.x behavior
//   SQLSetEnvAttr(dbo->m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
  
//   return dbo;
// }

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
      sqlReturnCode = SQLAllocHandle(SQL_HANDLE_DBC, odbcObject->m_hEnv, &(odbcObject->m_hDBC));

      printf("\nCreateConnectionAsyncWorker::Execute SQLAllocHandle return code is %d", sqlReturnCode);
      
      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBC::CreateConnectionAsyncWorker::OnOk\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // return the SQLError
      if (!SQL_SUCCEEDED(sqlReturnCode)) {

        printf("IF");

        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(ODBC::GetSQLError(env, SQL_HANDLE_ENV, odbcObject->m_hEnv)); // callbackArguments[0]
        
        Callback().Call(callbackArguments);
      }
      // return the Connection
      else {

        printf("\nONOK THE RETURN CODE IS %d", sqlReturnCode);

        printf("\nHDBC IS %p", odbcObject->m_hDBC);

        // pass the HENV and HDBC values to the ODBCConnection constructor
        std::vector<napi_value> connectionArguments;
        connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &(odbcObject->m_hEnv))); // connectionArguments[0]
        connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &(odbcObject->m_hDBC)));   // connectionArguments[1]
        
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
};

Napi::Value ODBC::CreateConnection(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  // TODO: Check that only one argument, a function, was passed in

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Function callback = info[0].As<Napi::Function>();

  CreateConnectionAsyncWorker *worker = new CreateConnectionAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * CreateConnectionSync
 */

Napi::Value ODBC::CreateConnectionSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBC::CreateConnectionSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
   
  HDBC hDBC;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  //allocate a new connection handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, this->m_hEnv, &hDBC);
  
  if (!SQL_SUCCEEDED(ret)) {
    //TODO: do something!
  }
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  // pass the HENV and HDBC values to the ODBCConnection constructor
  std::vector<napi_value> connectionArguments;
  connectionArguments.push_back(Napi::External<HENV>::New(env, &(this->m_hEnv))); // connectionArguments[0]
  connectionArguments.push_back(Napi::External<HDBC>::New(env, &hDBC));           // connectionArguments[1]
  
  // create a new ODBCConnection object as a Napi::Value
  Napi::Value connectionObject = ODBCConnection::constructor.Call(connectionArguments);

  return connectionObject;
}

/*
 * GetColumns
 */

Column* ODBC::GetColumns(SQLHSTMT hStmt, SQLSMALLINT *colCount) {

  SQLRETURN sqlReturnCode;

  printf("\ngetcolums01");

  //get the number of columns in the result set
  sqlReturnCode = SQLNumResultCols(hStmt, colCount);
  
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    printf("\nsqlReturnCode is %d", sqlReturnCode);
    *colCount = 0;
    return new Column[0];
  }

  printf("\ngetcolums0");
  
  Column *columns = new Column[*colCount];

  printf("\ngetcolums1");

  for (int i = 0; i < *colCount; i++) {

    columns[i].index = i + 1; // Column number of result data, starting at 1

    printf("\ngetcolumsloop");
  

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
      &(columns[i].nameSize),   // NameLengthPtr,
      &(columns[i].type),       // DataTypePtr
      &(columns[i].precision),  // ColumnSizePtr,
      &(columns[i].scale),      // DecimalDigitsPtr,
      &(columns[i].nullable)    // NullablePtr
    );
#endif
  }


  printf("\nCOLCOUNT IS %d", *colCount);
  printf("\ngetcolums2");


  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    // TODO: Something on failure
  }

  return columns;


// OLD: Used SQLColAttribute... SQLDescribeCol seems a lot easier to use,
// So changed it over. Maybe investigate if there are any differences between
// them.
  
//     //get the column name
//     ret = SQLColAttribute( hStmt,
//                            columns[i].index,
// #ifdef STRICT_COLUMN_NAMES
//                            SQL_DESC_NAME,
// #else
//                            SQL_DESC_LABEL,
// #endif
//                            columns[i].name,
//                            (SQLSMALLINT) MAX_FIELD_SIZE,
//                            &(columns[i].len),
//                            //(SQLSMALLINT *) &buflen,
//                            NULL);
    
//     //store the len attribute
//     columns[i].len = buflen;
    
//     //get the column type and store it directly in column[i].type
//     ret = SQLColAttribute( hStmt,
//                            columns[i].index,
//                            SQL_DESC_TYPE,
//                            NULL,
//                            0,
//                            NULL,
//                            &columns[i].type);
//   }
}

/*
 * BindColumnData
 */
SQLCHAR** ODBC::BindColumnData(HSTMT hSTMT, Column *columns, SQLSMALLINT *columnCount)
{
  SQLCHAR **rowData = new SQLCHAR*[*columnCount];
  SQLLEN maxColumnLength;
  SQLSMALLINT targetType;

  SQLRETURN sqlReturnCode;

  printf("Column count in bind columns is %d", *columnCount);

  for (int i = 0; i < *columnCount; i++)
  {
    printf("\nBinding a column");

    // TODO: These are taken from idb-connector. Make sure we handle all ODBC cases
    switch(columns[i].type) {
      case SQL_DECIMAL :
      case SQL_NUMERIC :

        maxColumnLength = columns[i].precision * 256 + columns[i].scale;
        targetType = SQL_C_CHAR;
        break;

      case SQL_VARBINARY :
      case SQL_BINARY :

        maxColumnLength = columns[i].precision;
        targetType = SQL_C_BINARY;
        break;

      // case SQL_BLOB :


      //   break;

      case SQL_WCHAR :
      case SQL_WVARCHAR :

        maxColumnLength = columns[i].precision << 2;
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
    printf("\nBinding SQL RETURN CODE: %d", sqlReturnCode);
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

/*
 * GetColumnValue
 */

Napi::Value ODBC::GetColumnValue(Napi::Env env, SQLHSTMT hStmt, Column column, uint16_t* buffer, int bufferLength) {

  Napi::EscapableHandleScope scope(env);
  SQLLEN len = 0;

  //reset the buffer
  buffer[0] = '\0';

  //TODO: SQLGetData can supposedly return multiple chunks, need to do this to 
  //retrieve large fields
  int ret; 
  
  switch ((int) column.type) {
    case SQL_INTEGER : 
    case SQL_SMALLINT :
    case SQL_TINYINT : {
        int32_t value = 0;
        
        ret = SQLGetData(
          hStmt, 
          column.index, 
          SQL_C_SLONG,
          &value, 
          sizeof(value), 
          &len);
        
        DEBUG_PRINTF("ODBC::GetColumnValue - Integer: index=%i name=%s type=%lli len=%lli ret=%i val=%li\n", 
                    column.index, column.name, column.type, len, ret, value);
        
        if (len == SQL_NULL_DATA) {
          return scope.Escape(env.Null());
        }
        else {
          return scope.Escape(Napi::Number::New(env, value));
        }
      }
      break;
    case SQL_NUMERIC :
    case SQL_DECIMAL :
    case SQL_BIGINT :
    case SQL_FLOAT :
    case SQL_REAL :
    case SQL_DOUBLE : {
        double value;
        
        ret = SQLGetData(hStmt, column.index, SQL_C_DOUBLE, &value, sizeof(value), &len);
        
         DEBUG_PRINTF("ODBC::GetColumnValue - Number: index=%i name=%s type=%lli len=%lli ret=%i val=%f\n", 
                    column.index, column.name, column.type, len, ret, value);
        
        if (len == SQL_NULL_DATA) {
          return scope.Escape(env.Null());
          //return Null();
        }
        else {
          return scope.Escape(Napi::Number::New(env, value));
          //return Number::New(value);
        }
      }
      break;
    case SQL_DATETIME :
    case SQL_TIMESTAMP : {
      //I am not sure if this is locale-safe or cross database safe, but it 
      //works for me on MSSQL
#ifdef _WIN32
      struct tm timeInfo = {};

      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_CHAR,
        (char *) buffer, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - W32 Timestamp: index=%i name=%s type=%lli len=%lli\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(env.Null());
        //return Null();
      }
      else {
        if (strptime((char *) buffer, "%Y-%m-%d %H:%M:%S", &timeInfo)) {
          //a negative value means that mktime() should use timezone information
          //and system databases to attempt to determine whether DST is in effect
          //at the specified time.
          timeInfo.tm_isdst = -1;
          
          //return scope.Escape(Date::New(Isolate::GetCurrent(), (double(mktime(&timeInfo)) * 1000));
          return scope.Escape(Napi::Date::New(env, double(mktime(&timeInfo)) * 1000));
        }
        else {
          return scope.Escape(Napi::New(env, (char *)buffer));
        }
      }
#else
      struct tm timeInfo = { 
        tm_sec : 0
        , tm_min : 0
        , tm_hour : 0
        , tm_mday : 0
        , tm_mon : 0
        , tm_year : 0
        , tm_wday : 0
        , tm_yday : 0
        , tm_isdst : 0
        #ifndef _AIX //AIX does not have these 
        , tm_gmtoff : 0
        , tm_zone : 0
        #endif
      };

      SQL_TIMESTAMP_STRUCT odbcTime;
      
      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_TYPE_TIMESTAMP,
        &odbcTime, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Unix Timestamp: index=%i name=%s type=%i len=%i\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(env.Null());
        //return Null();
      }
      else {
        timeInfo.tm_year = odbcTime.year - 1900;
        timeInfo.tm_mon = odbcTime.month - 1;
        timeInfo.tm_mday = odbcTime.day;
        timeInfo.tm_hour = odbcTime.hour;
        timeInfo.tm_min = odbcTime.minute;
        timeInfo.tm_sec = odbcTime.second;

        //a negative value means that mktime() should use timezone information 
        //and system databases to attempt to determine whether DST is in effect 
        //at the specified time.
        timeInfo.tm_isdst = -1;
#ifdef TIMEGM
        return scope.Escape(Napi::Date::New(env, (double(timegm(&timeInfo)) * 1000)
                          + (odbcTime.fraction / 1000000)));
#else
#ifdef _AIX
    #define timelocal mktime
#endif
        // TODO: MI: This isn't right, no Napi::Date
        // return scope.Escape(Napi::Date::New(env, (double(timelocal(&timeInfo)) * 1000)
        //                   + (odbcTime.fraction / 1000000)));
#endif
        //return Date::New((double(timegm(&timeInfo)) * 1000) 
        //                  + (odbcTime.fraction / 1000000));
      }
#endif
    } break;
    case SQL_BIT :
      //again, i'm not sure if this is cross database safe, but it works for 
      //MSSQL
      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_CHAR,
        (char *) buffer, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Bit: index=%i name=%s type=%lli len=%lli\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(env.Null());
      }
      else {
        return scope.Escape(Napi::Boolean::New(env, (*buffer == '0') ? false : true));
      }
    default :
      Napi::String str;
      int count = 0;
      
      do {
        ret = SQLGetData(
          hStmt,
          column.index,
          SQL_C_TCHAR,
          (char *) buffer,
          bufferLength,
          &len);

        DEBUG_PRINTF("ODBC::GetColumnValue - String: index=%i name=%s type=%lli len=%lli value=%s ret=%i bufferLength=%i\n", 
                      column.index, column.name, column.type, len,(char *) buffer, ret, bufferLength);

        if (len == SQL_NULL_DATA && str.IsEmpty()) {
          return scope.Escape(env.Null());
          //return Null();
        }
        
        if (SQL_NO_DATA == ret) {
          //we have captured all of the data
          //double check that we have some data else return null
          if (str.IsEmpty()){
            return scope.Escape(env.Null());
          }

          break;
        }
        else if (SQL_SUCCEEDED(ret)) {
          //we have not captured all of the data yet
          
          if (count == 0) {
            //no concatenation required, this is our first pass
#ifdef UNICODE
            str = Napi::String::New(env, (char16_t *) buffer);
#else
            str = Napi::String::New(env, (char *) buffer);
#endif
          }
          else {
            //we need to concatenate
#ifdef UNICODE
            // TODO: MI : NEED to do the 2 below
            //str = String::Concat(str, Napi::New(env, (char16_t *) buffer));
#else
            //str = String::Concat(str, Napi::New(env, (char *) buffer));
#endif
          }
          
          //if len is zero let's break out of the loop now and not attempt to
          //call SQLGetData again. The specific reason for this is because
          //some ODBC drivers may not correctly report SQL_NO_DATA the next
          //time around causing an infinite loop here
          if (len == 0) {
            break;
          }
          
          count += 1;
        }
        else {
          //an error has occured
          //possible values for ret are SQL_ERROR (-1) and SQL_INVALID_HANDLE (-2)

          //If we have an invalid handle, then stuff is way bad and we should abort
          //immediately. Memory errors are bound to follow as we must be in an
          //inconsisant state.
          if (ret != SQL_INVALID_HANDLE) {
            return env.Null();
          }

          //Not sure if throwing here will work out well for us but we can try
          //since we should have a valid handle and the error is something we 
          //can look into

          Napi::Value objError = ODBC::GetSQLError(env, SQL_HANDLE_STMT, hStmt, "[node-odbc] Error in ODBC::GetColumnValue");

          Napi::Error(env, objError).ThrowAsJavaScriptException();

          return scope.Escape(env.Undefined());
          break;
        }
      } while (true);
      
      return scope.Escape(str);
 }
}

/*
 * GetRecordTuple
 */

Napi::Value ODBC::GetRecordTuple (Napi::Env env, SQLHSTMT hStmt, Column* columns, short* colCount, uint16_t* buffer, int bufferLength) {

  Napi::EscapableHandleScope scope(env);
  
  Napi::Object tuple = Napi::Object::New(env);
        
  for(int i = 0; i < *colCount; i++) {
    #ifdef UNICODE
      tuple.Set(Napi::String::New(env, (char16_t *) columns[i].name),
                  GetColumnValue(env, hStmt, columns[i], buffer, bufferLength));
    #else
      tuple.Set(Napi::String::New(env, (const char *)columns[i].name),
                GetColumnValue(env, hStmt, columns[i], buffer, bufferLength));
    #endif
  }
  
  return scope.Escape(tuple);
}

/*
 * GetRecordArray
 */

Napi::Value ODBC::GetRecordArray (Napi::Env env, SQLHSTMT hStmt, Column* columns, short* colCount, uint16_t* buffer, int bufferLength) {

  Napi::HandleScope scope(env);
  
  Napi::Array array = Napi::Array::New(env);
        
  for(int i = 0; i < *colCount; i++) {
    array.Set( Napi::String::New(env, std::to_string(i)), GetColumnValue(env, hStmt, columns[i], buffer, bufferLength));
  }

  return array;
  
  //return scope.Escape(array);
}

/*
 * GetParametersFromArray
 */

Parameter* ODBC::GetParametersFromArray (Napi::Array *values, int *paramCount) {
  DEBUG_PRINTF("ODBC::GetParametersFromArray\n");
  *paramCount = values->Length();
  
  Parameter* params = NULL;
  
  if (*paramCount > 0) {
    params = (Parameter *) malloc(*paramCount * sizeof(Parameter));
  }
  
  for (int i = 0; i < *paramCount; i++) {
    Napi::Value value = values->Get(i);
    
    params[i].ColumnSize       = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;
    params[i].BufferLength     = 0;
    params[i].DecimalDigits    = 0;

    DEBUG_PRINTF("ODBC::GetParametersFromArray - param[%i].length = %lli\n",
                 i, params[i].StrLen_or_IndPtr);

    if (value.IsString()) {
      Napi::String string = value.ToString();
      
      params[i].ValueType         = SQL_C_TCHAR;
      params[i].ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
#ifdef UNICODE
      params[i].ParameterType     = SQL_WVARCHAR;
      params[i].BufferLength      = (string.Utf16Value().length() * sizeof(char16_t)) + sizeof(char16_t);
#else
      params[i].ParameterType     = SQL_VARCHAR;
      params[i].BufferLength      = string.Utf8Value().length() + 1;
#endif
      params[i].ParameterValuePtr = malloc(params[i].BufferLength);
      params[i].StrLen_or_IndPtr  = SQL_NTS;//params[i].BufferLength;

#ifdef UNICODE
      // TODO: MI : What is going on here?
      //string.Write((uint16_t *) params[i].ParameterValuePtr);
#else
      // TODO: MI : What is going on here?
      //string.WriteUtf8((char *) params[i].ParameterValuePtr);
#endif

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%s\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr, 
                    (char*) params[i].ParameterValuePtr);
    }
    else if (value.IsNull()) {
      params[i].ValueType = SQL_C_DEFAULT;
      params[i].ParameterType   = SQL_VARCHAR;
      params[i].StrLen_or_IndPtr = SQL_NULL_DATA;

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNull(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
    else if (value.IsNumber()) {
      int64_t  *number = new int64_t(value.As<Napi::Number>().Int64Value());
      params[i].ValueType = SQL_C_SBIGINT;
      params[i].ParameterType   = SQL_BIGINT;
      params[i].ParameterValuePtr = number;
      params[i].StrLen_or_IndPtr = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%lld\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
                    *number);
    }
    else if (value.IsNumber()) {
      double *number   = new double(value.As<Napi::Number>().DoubleValue());
      
      params[i].ValueType         = SQL_C_DOUBLE;
      params[i].ParameterType     = SQL_DECIMAL;
      params[i].ParameterValuePtr = number;
      params[i].BufferLength      = sizeof(double);
      params[i].StrLen_or_IndPtr  = params[i].BufferLength;
      params[i].DecimalDigits     = 7;
      params[i].ColumnSize        = sizeof(double);

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNumber(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%f\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
		                *number);
    }
    else if (value.IsBoolean()) {
      bool *boolean = new bool(value.As<Napi::Boolean>().Value());
      params[i].ValueType         = SQL_C_BIT;
      params[i].ParameterType     = SQL_BIT;
      params[i].ParameterValuePtr = boolean;
      params[i].StrLen_or_IndPtr  = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsBoolean(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
  } 
  
  return params;
}

/*
 * CallbackSQLError
 */

Napi::Value ODBC::CallbackSQLError(Napi::Env env, SQLSMALLINT handleType,SQLHANDLE handle, Napi::Function* cb) {

  Napi::HandleScope scope(env);
  
  return CallbackSQLError(env, handleType, handle, "[node-odbc] SQL_ERROR", cb);
}

Napi::Value ODBC::CallbackSQLError(Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, char* message, Napi::Function* cb) {

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

Napi::Value ODBC::GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle) {

  Napi::HandleScope scope(env);

  return GetSQLError(env, handleType, handle, "[node-odbc] SQL_ERROR");
}

Napi::Value ODBC::GetSQLError (Napi::Env env, SQLSMALLINT handleType, SQLHANDLE handle, char* message) {
  Napi::EscapableHandleScope scope(env);
  
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

  return scope.Escape(objError);
}

// /*
//  * GetAllRecordsSync
//  */

Napi::Array ODBC::GetAllRecordsSync (Napi::Env env, HENV hENV, HDBC hDBC, HSTMT hSTMT,uint16_t* buffer, int bufferLength) {
  DEBUG_PRINTF("ODBC::GetAllRecordsSync\n");
  
  Napi::HandleScope scope(env);
  
  int count = 0;
  int errorCount = 0;
  short colCount = 0;
  
  Column* columns = GetColumns(hSTMT, &colCount);
  
  Napi::Array rows = Napi::Array::New(env);
  
  //loop through all records
  while (true) {
    SQLRETURN ret = SQLFetch(hSTMT);
    
    //check to see if there was an error
    if (ret == SQL_ERROR)  {

      //TODO: what do we do when we actually get an error here...
      //should we throw??
      
      errorCount++;     
      Napi::Value error = ODBC::GetSQLError(env, SQL_HANDLE_STMT, hSTMT, "[node-odbc] Error in ODBC::GetAllRecordsSync");
      
      break;
    }
    
    //check to see if we are at the end of the recordset
    if (ret == SQL_NO_DATA) {
      ODBC::FreeColumns(columns, &colCount);   
      break;
    }

    rows.Set(Napi::String::New(env, std::to_string(count)), ODBC::GetRecordTuple(env, hSTMT, columns, &colCount, buffer, bufferLength));

    count++;
  }
  //TODO: what do we do about errors!?!
  //we throw them
  return rows;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {

  ODBC::Init(env, exports);
  ODBCResult::Init(env, exports, nullptr, nullptr, nullptr, false);
  ODBCConnection::Init(env, exports);
  ODBCStatement::Init(env, exports, nullptr, nullptr, nullptr);

  return exports;

  // #ifdef dynodbc
  // exports.Set(Napi::String::New(env, "loadODBCLibrary"),
  //       Napi::Function::New(env, ODBC::LoadODBCLibrary);());
  // #endif
  
  // ODBC::Init(env, exports);
  // ODBCResult::Init(env, exports, nullptr, nullptr, nullptr, false);
  // ODBCConnection::Init(env, exports);
  // ODBCStatement::Init(env, exports, nullptr, nullptr, nullptr);
  
  return exports;
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
