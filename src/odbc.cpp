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
#include <stdlib.h> 

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"

#ifdef dynodbc
#include "dynodbc.h"
#endif

uv_mutex_t ODBC::g_odbcMutex;
SQLHENV ODBC::hEnv;

Napi::Value ODBC::Init(Napi::Env env, Napi::Object exports) {

  hEnv = NULL;

  Napi::HandleScope scope(env);

  // Wrap ODBC constants in an object that we can then expand 
  std::vector<Napi::PropertyDescriptor> ODBC_CONSTANTS;

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_COMMIT", Napi::Number::New(env, SQL_COMMIT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_ROLLBACK", Napi::Number::New(env, SQL_ROLLBACK), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_USER_NAME", Napi::Number::New(env, SQL_USER_NAME), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT", Napi::Number::New(env, SQL_PARAM_INPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT_OUTPUT", Napi::Number::New(env, SQL_PARAM_INPUT_OUTPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_OUTPUT", Napi::Number::New(env, SQL_PARAM_OUTPUT), napi_enumerable));

  exports.DefineProperties(ODBC_CONSTANTS);

  exports.Set("connect", Napi::Function::New(env, ODBC::Connect));
  exports.Set("connectSync", Napi::Function::New(env, ODBC::ConnectSync));
  
  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);

  uv_mutex_lock(&ODBC::g_odbcMutex);
  // Initialize the Environment handle
  SQLRETURN sqlReturnCode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    Napi::Error(env, Napi::String::New(env, ODBC::GetSQLError(SQL_HANDLE_ENV, hEnv))).ThrowAsJavaScriptException();
    return env.Null();
  }
  
  // Use ODBC 3.x behavior
  SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);

  return exports;
}

ODBC::~ODBC() {
  DEBUG_PRINTF("ODBC::~ODBC\n");
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  if (hEnv) {
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    hEnv = NULL;
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);
}

/*
 * Connect
 */
class ConnectAsyncWorker : public Napi::AsyncWorker {

  private:
  
    SQLTCHAR *connectionStringPtr;
    SQLHENV hEnv;
    SQLHDBC hDBC;
    SQLUSMALLINT canHaveMoreResults;
    SQLRETURN sqlReturnCode;

  public:
    ConnectAsyncWorker(HENV hEnv, SQLTCHAR *connectionStringPtr, Napi::Function& callback) : Napi::AsyncWorker(callback),
      connectionStringPtr(connectionStringPtr),
      hEnv(hEnv) {}

    ~ConnectAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBC::ConnectAsyncWorker::Execute\n");
  
      uv_mutex_lock(&ODBC::g_odbcMutex);
      sqlReturnCode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDBC);

      unsigned int connectTimeout = 5;
      unsigned int loginTimeout = 5;

      if (connectTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                                // ConnectionHandle
          SQL_ATTR_CONNECTION_TIMEOUT,         // Attribute
          (SQLPOINTER) size_t(connectTimeout), // ValuePtr
          SQL_IS_UINTEGER);                    // StringLength
      }
      
      if (loginTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                              // ConnectionHandle
          SQL_ATTR_LOGIN_TIMEOUT,            // Attribute
          (SQLPOINTER) size_t(loginTimeout), // ValuePtr
          SQL_IS_UINTEGER);                  // StringLength
      }

      //Attempt to connect
      sqlReturnCode = SQLDriverConnect(
        hDBC, // ConnectionHandle
        NULL,                         // WindowHandle
        connectionStringPtr,          // InConnectionString
        SQL_NTS,                      // StringLength1
        NULL,                         // OutConnectionString
        0,                            // BufferLength - in characters
        NULL,                         // StringLength2Ptr
        SQL_DRIVER_NOPROMPT);         // DriverCompletion
      
      if (SQL_SUCCEEDED(sqlReturnCode)) {

        // odbcConnectionObject->connected = true;

        HSTMT hStmt;
        
        //allocate a temporary statment
        sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, hDBC, &hStmt);
        
        //try to determine if the driver can handle
        //multiple recordsets
        sqlReturnCode = SQLGetFunctions(
          hDBC,
          SQL_API_SQLMORERESULTS, 
          &canHaveMoreResults
        );

        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          canHaveMoreResults = 0;
        }
        
        //free the handle
        sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

      } else {
        SetError(ODBC::GetSQLError(SQL_HANDLE_DBC, hDBC, (char *) "[node-odbc] Error in ConnectAsyncWorker"));
      }

      uv_mutex_unlock(&ODBC::g_odbcMutex);
    }

    void OnOK() {

      DEBUG_PRINTF("ODBC::ConnectAsyncWorker::OnOk\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // pass the HENV and HDBC values to the ODBCConnection constructor
      std::vector<napi_value> connectionArguments;
      connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &hEnv)); // connectionArguments[0]
      connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC)); // connectionArguments[1]
      
      // create a new ODBCConnection object as a Napi::Value
      Napi::Value connectionObject = ODBCConnection::constructor.New(connectionArguments);

      // pass the arguments to the callback function
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());       // callbackArguments[0]  
      callbackArguments.push_back(connectionObject); // callbackArguments[1]

      Callback().Call(callbackArguments);
      
    }
};

// Connect
Napi::Value ODBC::Connect(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString;
  Napi::Function callback;

  SQLTCHAR *connectionStringPtr = nullptr;

  if(info.Length() != 2) {
    Napi::TypeError::New(env, "connect(connectionString, callback) requires 2 parameters.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    connectionString = info[0].As<Napi::String>();
    connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);
  } else {
    Napi::TypeError::New(env, "connect: first parameter must be a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[1].IsFunction()) {
    callback = info[1].As<Napi::Function>();
  } else {
    Napi::TypeError::New(env, "connect: second parameter must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  ConnectAsyncWorker *worker = new ConnectAsyncWorker(hEnv, connectionStringPtr, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBC::ConnectSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::Value error;
  Napi::Value returnValue;

  SQLUSMALLINT canHaveMoreResults;
  SQLRETURN sqlReturnCode;
  SQLHDBC hDBC;

  if(info.Length() != 1) {
    Napi::TypeError::New(env, "connectSync(connectionString) requires 1 parameter.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "connectSync: first parameter must be a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::String connectionString = info[0].As<Napi::String>();
  SQLTCHAR *connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);

  uv_mutex_lock(&ODBC::g_odbcMutex);
  sqlReturnCode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDBC);

  unsigned int connectTimeout = 5;
  unsigned int loginTimeout = 5;
      
  if (connectTimeout > 0) {
    sqlReturnCode = SQLSetConnectAttr(
      hDBC,                                // ConnectionHandle
      SQL_ATTR_CONNECTION_TIMEOUT,         // Attribute
      (SQLPOINTER) size_t(connectTimeout), // ValuePtr
      SQL_IS_UINTEGER);                    // StringLength
  }

  if (loginTimeout > 0) {
    sqlReturnCode = SQLSetConnectAttr(
      hDBC,                   // ConnectionHandle
      SQL_ATTR_LOGIN_TIMEOUT, // Attribute
      (SQLPOINTER) size_t(loginTimeout), // ValuePtr
      SQL_IS_UINTEGER);       // StringLength
  }

  //Attempt to connect
  //NOTE: SQLDriverConnect requires the thread to be locked
  sqlReturnCode = SQLDriverConnect(
    hDBC,                           // ConnectionHandle
    NULL,                           // WindowHandle
    connectionStringPtr,            // InConnectionString
    SQL_NTS,                        // StringLength1
    NULL,                           // OutConnectionString
    0,                              // BufferLength - in characters
    NULL,                           // StringLength2Ptr
    SQL_DRIVER_NOPROMPT             // DriverCompletion
  );


  
  if (SQL_SUCCEEDED(sqlReturnCode)) {

    HSTMT hStmt;
    
    //allocate a temporary statment
    sqlReturnCode = SQLAllocHandle(SQL_HANDLE_STMT, hDBC, &hStmt);
    
    //try to determine if the driver can handle
    //multiple recordsets
    sqlReturnCode = SQLGetFunctions(
      hDBC,
      SQL_API_SQLMORERESULTS, 
      &canHaveMoreResults);

    if (!SQL_SUCCEEDED(sqlReturnCode)) {
      canHaveMoreResults = 0;
    }

    //free the handle
    sqlReturnCode = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    returnValue = Napi::Boolean::New(env, true);

  } else {
    Napi::Error::New(env, ODBC::GetSQLError(SQL_HANDLE_DBC, hDBC)).ThrowAsJavaScriptException();
    returnValue = env.Null();
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex); 

  // return the SQLError
  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    Napi::Error::New(env, ODBC::GetSQLError(SQL_HANDLE_ENV, hEnv)).ThrowAsJavaScriptException();
    return env.Null();
  }
  // return the Connection
  else {

    // pass the HENV and HDBC values to the ODBCConnection constructor
    std::vector<napi_value> connectionArguments;
    connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &hEnv)); // connectionArguments[0]
    connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC)); // connectionArguments[1]
    
    // create a new ODBCConnection object as a Napi::Value
    return ODBCConnection::constructor.New(connectionArguments);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////// UTILITY FUNCTIONS ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Take a Napi::String, and convert it to an SQLTCHAR*
SQLTCHAR* ODBC::NapiStringToSQLTCHAR(Napi::String string) {

  #ifdef UNICODE
    std::u16string tempString = string.Utf16Value();
  #else
    std::string tempString = string.Utf8Value();
  #endif
  std::vector<SQLTCHAR> *stringVector = new std::vector<SQLTCHAR>(tempString.begin(), tempString.end());
  stringVector->push_back('\0');

  return &(*stringVector)[0];
}

void ODBC::RetrieveData(QueryData *data) {
  ODBC::BindColumns(data);
  ODBC::FetchAll(data);
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
        row[i].data = new SQLTCHAR[row[i].size];
        memcpy(row[i].data, data->boundRow[i], row[i].size);
      }
    }

    data->storedRows.push_back(row);
  }
}

// All of data has been loaded into data->storedRows. Have to take the data
// stored in there and convert it it into JavaScript to be given to the
// Node.js runtime.
Napi::Array ODBC::ProcessDataForNapi(Napi::Env env, QueryData *data) {

  std::vector<ColumnData*> *storedRows = &data->storedRows;
  Column *columns = data->columns;
  int columnCount = data->columnCount;

  Napi::Array rows = Napi::Array::New(env);
  if (storedRows->size() > 0) {
    rows.Set(Napi::String::New(env, COUNT), Napi::Number::New(env, storedRows->size()));
  } else {
    rows.Set(Napi::String::New(env, COUNT), Napi::Number::New(env, data->rowCount));
  }

  // attach the column information
  Napi::Array napiColumns = Napi::Array::New(env);

  for (SQLSMALLINT h = 0; h < columnCount; h++) {
    Napi::Object column = Napi::Object::New(env);
    column.Set(Napi::String::New(env, NAME), Napi::String::New(env, (const char*)columns[h].name));
    column.Set(Napi::String::New(env, DATA_TYPE), Napi::Number::New(env, columns[h].type));

    napiColumns.Set(h, column);
  }

  rows.Set(Napi::String::New(env, COLUMNS), napiColumns);

  for (size_t i = 0; i < storedRows->size(); i++) {

    // Arrays are a subclass of Objects
    Napi::Object row = Napi::Object::New(env);

    ColumnData *storedRow = (*storedRows)[i];

    // Iterate over each column, putting the data in the row object
    for (int j = 0; j < columnCount; j++) {

      Napi::Value value;

      // check for null data
      if (storedRow[j].size == SQL_NULL_DATA) {

        value = env.Null();

      } else {

        switch(columns[j].type) {
          case SQL_REAL:
          case SQL_NUMERIC :
            value = Napi::Number::New(env, atof((const char*)storedRow[j].data));
            break;
          // Napi::Number
          case SQL_DECIMAL :
          case SQL_FLOAT :
          case SQL_DOUBLE :
            value = Napi::Number::New(env, strtod((const char*)storedRow[j].data);
            break;
          case SQL_INTEGER :
          case SQL_SMALLINT :
          case SQL_BIGINT :
            value = Napi::Number::New(env, atoi((const char*)storedRow[j].data);
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
      }

      row.Set(Napi::String::New(env, (const char*)columns[j].name), value);

      delete storedRow[j].data;
    }
    rows.Set(i, row);
  }

  storedRows->clear();

  return rows;
}

/******************************************************************************
 ****************************** BINDING COLUMNS *******************************
 *****************************************************************************/
void ODBC::BindColumns(QueryData *data) {

  // SQLNumResultCols returns the number of columns in a result set.
  data->sqlReturnCode = SQLNumResultCols(
                          data->hSTMT,       // StatementHandle
                          &data->columnCount // ColumnCountPtr
                        );
                        
  // if there was an error, set columnCount to 0 and return
  // TODO: Should throw an error?
  if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
    data->columnCount = 0;
    return;
  }

  // create Columns for the column data to go into
  data->columns = new Column[data->columnCount];
  data->boundRow = new SQLTCHAR*[data->columnCount];

  for (int i = 0; i < data->columnCount; i++) {

    data->columns[i].index = i + 1; // Column number of result data, starting at 1
    data->columns[i].name = new SQLTCHAR[SQL_MAX_COLUMN_NAME_LEN]();
    data->sqlReturnCode = SQLDescribeCol(
      data->hSTMT,                   // StatementHandle
      data->columns[i].index,        // ColumnNumber
      data->columns[i].name,         // ColumnName
      SQL_MAX_COLUMN_NAME_LEN,       // BufferLength,  
      &(data->columns[i].nameSize),  // NameLengthPtr,
      &(data->columns[i].type),      // DataTypePtr
      &(data->columns[i].precision), // ColumnSizePtr,
      &(data->columns[i].scale),     // DecimalDigitsPtr,
      &(data->columns[i].nullable)   // NullablePtr
    );

    // TODO: Should throw an error?
    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      return;
    }

    SQLLEN maxColumnLength;
    SQLSMALLINT targetType;

    // bind depending on the column
    switch(data->columns[i].type) {

      case SQL_DECIMAL :
      case SQL_NUMERIC :

        maxColumnLength = data->columns[i].precision + data->columns[i].scale + 1;
        targetType = SQL_C_CHAR;

        break;

      case SQL_DOUBLE :

        maxColumnLength = data->columns[i].precision;
        targetType = SQL_C_DOUBLE;
        break;

      case SQL_INTEGER :

        maxColumnLength = data->columns[i].precision;
        targetType = SQL_C_SLONG;
        break;

      case SQL_BIGINT :

       maxColumnLength = data->columns[i].precision;
       targetType = SQL_C_SBIGINT;
       break;

      case SQL_BINARY :
      case SQL_VARBINARY :
      case SQL_LONGVARBINARY :

        maxColumnLength = data->columns[i].precision;
        targetType = SQL_C_BINARY;
        break;

      case SQL_WCHAR :
      case SQL_WVARCHAR :

        maxColumnLength = (data->columns[i].precision << 2) + 1;
        targetType = SQL_C_CHAR;
        break;

      default:
      
        maxColumnLength = data->columns[i].precision + 1;
        targetType = SQL_C_CHAR;
        break;
    }

    data->boundRow[i] = new SQLTCHAR[maxColumnLength]();

    // SQLBindCol binds application data buffers to columns in the result set.
    data->sqlReturnCode = SQLBindCol(
      data->hSTMT,              // StatementHandle
      i + 1,                    // ColumnNumber
      targetType,               // TargetType
      data->boundRow[i],        // TargetValuePtr
      maxColumnLength,          // BufferLength
      &(data->columns[i].dataLength)  // StrLen_or_Ind
    );

    // TODO: Error
    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      return;
    }
  }
}

/******************************************************************************
 **************************** BINDING PARAMETERS ******************************
 *****************************************************************************/

/*
 * GetParametersFromArray
 *  Array of parameters can hold either/and:
 *    Value:
 *      One value to bind, In/Out defaults to SQL_PARAM_INPUT, dataType defaults based on the value
 *    Arrays:
 *      between 1 and 3 entries in lenth, with the following signfigance and default values:
 *        1. Value (REQUIRED): The value to bind
 *        2. In/Out (Optional): Defaults to SQL_PARAM_INPUT
 *        3. DataType (Optional): Defaults based on the value 
 *    Objects:
 *      can hold any of the following properties (but requires at least 'value' property)
 *        value (Requited): The value to bind
 *        inOut (Optional): the In/Out type to use, Defaults to SQL_PARAM_INPUT
 *        dataType (Optional): The data type, defaults based on the value
 *        
 *        
 */
Parameter* ODBC::GetParametersFromArray(Napi::Array *bindArray, int *paramCount) {

  DEBUG_PRINTF("ODBC::GetParametersFromArray\n");

  *paramCount = bindArray->Length();
  
  Parameter* params = NULL;
  
  if (*paramCount > 0) {
    params = new Parameter[*paramCount];
  }
  
  for (int i = 0; i < *paramCount; i++) {

    // default values
    params[i].ColumnSize       = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;
    params[i].BufferLength     = 0;
    params[i].DecimalDigits    = 0;
    params[i].InputOutputType  = SQL_PARAM_INPUT;

    // The entry in the array that is bound. Can be an array, object, or stand-alone value
    Napi::Value bindArrayEntry = bindArray->Get(i);

    // Parameters in array
    if (bindArrayEntry.IsArray()) {

      Napi::Array parameterArray = bindArrayEntry.As<Napi::Array>();
      int size = parameterArray.Length();

      if (size < 1 || size > 2) {
        // TODO ERROR: Parameter arrays must have between 1 and 3 items: [value, inOutType, dataType]
      }

      Napi::Value value = parameterArray.Get((unsigned int)0);
      if (!(value.IsString() || value.IsNumber() || value.IsNull() || value.IsBoolean())) {
        // TODO: ERROR: Parameter arrays' first item [value] must be a string, number, boolean, or null
        return nullptr;
      }

      if (size == 2) {
        SQLSMALLINT inOutType = parameterArray.Get(1).ToNumber().Int32Value();
        if (inOutType == SQL_PARAM_INPUT || inOutType == SQL_PARAM_INPUT_OUTPUT || inOutType == SQL_PARAM_OUTPUT) {
            params[i].InputOutputType = inOutType;
          } else {
            // TODO: ERROR: Parameter arrays' second item must be SQL_PARAM_INPUT, SQL_PARAM_INPUT_OUTPUT, or SQL_PARAM_OUTPUT
          }
      } else {
        params[i].InputOutputType = SQL_PARAM_INPUT;
      }

      ODBC::DetermineParameterType(value, &params[i]);

    // parameters in Object
    } else if (bindArrayEntry.IsObject()) {

      Napi::Object parameterObject = bindArrayEntry.As<Napi::Object>();
      Napi::Value value;

      if (parameterObject.Has("value")) {
        value = parameterObject.Get("value");
      } else {
        // TODO: Error: parameter object needs at least a value property
      }

      if (parameterObject.Has("inOutType")) {
        SQLSMALLINT inOutType = parameterObject.Get("inOutType").ToNumber().Int32Value();
        if (inOutType == SQL_PARAM_INPUT || inOutType == SQL_PARAM_INPUT_OUTPUT || inOutType == SQL_PARAM_OUTPUT) {
            params[i].InputOutputType = inOutType;
          } else {
            // TODO: ERROR: Parameter arrays' second item must be SQL_PARAM_INPUT, SQL_PARAM_INPUT_OUTPUT, or SQL_PARAM_OUTPUT
          }
      } else {
        params[i].InputOutputType = SQL_PARAM_INPUT;
      }

      ODBC::DetermineParameterType(value, &params[i]);

    // parameter is just a value
    } else if (bindArrayEntry.IsString() || bindArrayEntry.IsNumber() || bindArrayEntry.IsNull() || bindArrayEntry.IsBoolean()) {

      Napi::Value value = bindArrayEntry;
      ODBC::DetermineParameterType(value, &params[i]);

    } else { // is undefined, symbol, arraybuffer, function, promise, external, etc...
      // Not an Array of Values, or an Object of Values, or just a primitve Value, then there is an error
      // TODO: Throw an error
    }
  } 
  
  return params;
}

void ODBC::DetermineParameterType(Napi::Value value, Parameter *param) {

  if (value.IsNull()) {

      param->ValueType = SQL_C_DEFAULT;
      param->ParameterType   = SQL_VARCHAR;
      param->StrLen_or_IndPtr = SQL_NULL_DATA;
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
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%lld\n",
                    i, param->ValueType, param->ParameterType,
                    param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr,
                    *number);
    } else {
      // not an integer
      double *number   = new double(value.As<Napi::Number>().DoubleValue());
    
      param->ValueType         = SQL_C_DOUBLE;
      param->ParameterType     = SQL_DOUBLE;
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
  else { // Default to string

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
          memcpy((char16_t *) param->ParameterValuePtr, string.Utf16Value().c_str(), param->BufferLength);
    #else
          memcpy((char *) param->ParameterValuePtr, string.Utf8Value().c_str(), param->BufferLength); 
    #endif

    DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%s\n",
                  i, param->ValueType, param->ParameterType,
                  param->BufferLength, param->ColumnSize, param->StrLen_or_IndPtr, 
                  (char*) param->ParameterValuePtr);
  }
}

void ODBC::BindParameters(QueryData *data) {

  for (int i = 0; i < data->paramCount; i++) {

    Parameter parameter = data->params[i];

    DEBUG_TPRINTF(
      SQL_T("ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%i size=%i\n"), i, parameter.ValueType, parameter.ParameterType,
      parameter.BufferLength, parameter.ColumnSize);

    data->sqlReturnCode = SQLBindParameter(
      data->hSTMT,                               // StatementHandle
      i + 1,                                    // ParameterNumber
      parameter.InputOutputType,                // InputOutputType
      parameter.ValueType,                      // ValueType
      parameter.ParameterType,                  // ParameterType
      parameter.ColumnSize,                     // ColumnSize
      parameter.DecimalDigits,                  // DecimalDigits
      parameter.ParameterValuePtr,              // ParameterValuePtr
      parameter.BufferLength,                   // BufferLength
      &data->params[i].StrLen_or_IndPtr);       // StrLen_or_IndPtr

    if (data->sqlReturnCode == SQL_ERROR) {
      return;
    }
  }
}

/*
 * GetSQLError
 */

std::string ODBC::GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle) {

  std::string error = GetSQLError(handleType, handle, "[node-odbc] SQL_ERROR");
  return error;
}

std::string ODBC::GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle, const char* message) {

  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);

  std::string errorMessageStr;
  
  // Napi::Object objError = Napi::Object::New(env);

  int32_t i = 0;
  SQLINTEGER native;
  
  SQLSMALLINT len;
  SQLINTEGER statusRecCount;
  SQLRETURN ret;
  char errorSQLState[14];
  char errorMessage[ERROR_MESSAGE_BUFFER_BYTES];

  ret = SQLGetDiagField (
    handleType,
    handle,
    0,
    SQL_DIAG_NUMBER,
    &statusRecCount,
    SQL_IS_INTEGER,
    &len
  );

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", ret, statusRecCount);

  // Napi::Array errors = Napi::Array::New(env);

  if (statusRecCount > 1) {
    // objError.Set(Napi::String::New(env, "errors"), errors);
  }

  errorMessageStr += "\"errors\": [";
  
  for (i = 0; i < statusRecCount; i++) {

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

      
      if (i != 0) {
        errorMessageStr += ',';
      }

#ifdef UNICODE
        // TODO:
#else
        std::string error = message;
        std::string message = errorMessage;
        std::string SQLstate = errorSQLState;
#endif
        errorMessageStr += "{ \"error\": \"" + error + "\", \"message\": \"" + errorMessage + "\", \"SQLState\": \"" + SQLstate + "\"}";

    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  // if (statusRecCount == 0) {
  //   //Create a default error object if there were no diag records
  //   objError.Set(Napi::String::New(env, "error"), Napi::String::New(env, message));
  //   //objError.SetPrototype(Napi::Error(Napi::String::New(env, message)));
  //   objError.Set(Napi::String::New(env, "message"), Napi::String::New(env, 
  //     (const char *) "[node-odbc] An error occurred but no diagnostic information was available."));
  // }

  errorMessageStr += "]";

  return errorMessageStr;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {

  ODBC::Init(env, exports);
  ODBCConnection::Init(env, exports);
  ODBCStatement::Init(env, exports);

  #ifdef dynodbc
    exports.Set(Napi::String::New(env, "loadODBCLibrary"),
                Napi::Function::New(env, ODBC::LoadODBCLibrary);());
  #endif

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
