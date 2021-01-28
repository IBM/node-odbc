/*
  Copyright (c) 2019, 2021 IBM
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

// // object keys for the result object
// const char* NAME = "name\0";
// const char* DATA_TYPE = "dataType\0";
// const char* STATEMENT = "statement\0";
// const char* PARAMETERS = "parameters\0";
// const char* RETURN = "return\0";
// const char* COUNT = "count\0";
// const char* COLUMNS = "columns\0";

// error strings
const char* ODBC_ERRORS = "odbcErrors\0";
const char* STATE = "state\0";
const char* CODE = "code\0";
const char* MESSAGE = "message\0";
#ifdef UNICODE
  const char16_t* NO_MSG_TEXT = u"No message text available.\0";
#else
  const char* NO_MSG_TEXT = "No message text available.\0";
#endif

uv_mutex_t ODBC::g_odbcMutex;
SQLHENV ODBC::hEnv;

Napi::Value ODBC::Init(Napi::Env env, Napi::Object exports) {

  hEnv = NULL;
  Napi::HandleScope scope(env);

  // Wrap ODBC constants in an object that we can then expand
  std::vector<Napi::PropertyDescriptor> ODBC_CONSTANTS;

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("ODBCVER", Napi::Number::New(env, ODBCVER), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_COMMIT", Napi::Number::New(env, SQL_COMMIT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_ROLLBACK", Napi::Number::New(env, SQL_ROLLBACK), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_USER_NAME", Napi::Number::New(env, SQL_USER_NAME), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT", Napi::Number::New(env, SQL_PARAM_INPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT_OUTPUT", Napi::Number::New(env, SQL_PARAM_INPUT_OUTPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_OUTPUT", Napi::Number::New(env, SQL_PARAM_OUTPUT), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_VARCHAR", Napi::Number::New(env, SQL_VARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTEGER", Napi::Number::New(env, SQL_INTEGER), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_SMALLINT", Napi::Number::New(env, SQL_SMALLINT), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NO_NULLS", Napi::Number::New(env, SQL_NO_NULLS), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NULLABLE", Napi::Number::New(env, SQL_NULLABLE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NULLABLE_UNKNOWN", Napi::Number::New(env, SQL_NULLABLE_UNKNOWN), napi_enumerable));

  // setIsolationLevel options
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TXN_READ_UNCOMMITTED", Napi::Number::New(env, SQL_TXN_READ_UNCOMMITTED), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TRANSACTION_READ_UNCOMMITTED", Napi::Number::New(env, SQL_TRANSACTION_READ_UNCOMMITTED), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TXN_READ_COMMITTED", Napi::Number::New(env, SQL_TXN_READ_COMMITTED), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TRANSACTION_READ_COMMITTED", Napi::Number::New(env, SQL_TRANSACTION_READ_COMMITTED), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TXN_REPEATABLE_READ", Napi::Number::New(env, SQL_TXN_REPEATABLE_READ), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TRANSACTION_REPEATABLE_READ", Napi::Number::New(env, SQL_TRANSACTION_REPEATABLE_READ), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TXN_SERIALIZABLE", Napi::Number::New(env, SQL_TXN_SERIALIZABLE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TRANSACTION_SERIALIZABLE", Napi::Number::New(env, SQL_TRANSACTION_SERIALIZABLE), napi_enumerable));

  Napi::Object odbcConstants = Napi::Object::New(env);
  odbcConstants.DefineProperties(ODBC_CONSTANTS);
  exports.Set("odbcConstants", odbcConstants);

  exports.Set("connect", Napi::Function::New(env, ODBC::Connect));

  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);

  uv_mutex_lock(&ODBC::g_odbcMutex);
  // Initialize the Environment handle
  SQLRETURN sqlReturnCode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    // TODO: Redo
    // Napi::Error(env, Napi::String::New(env, (const char*)ODBC::GetSQLErrors(SQL_HANDLE_ENV, hEnv)[0].message)).ThrowAsJavaScriptException();
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

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// ODBCAsyncWorker ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// This class extends Napi::AsyncWorker to standardize error handling for all
// AsyncWorkers used by the package 
//
////////////////////////////////////////////////////////////////////////////////

ODBCAsyncWorker::ODBCAsyncWorker(Napi::Function& callback)
  : Napi::AsyncWorker(callback) {};

// TODO: Documentation for this function
void ODBCAsyncWorker::OnError(const Napi::Error &e) {

  Napi::Env env = Env();
  Napi::HandleScope scope(env);

  // add the additional information to the Error object
  Napi::Error error = Napi::Error::New(env, e.Message());
  Napi::Array odbcErrors = Napi::Array::New(env);

  for (SQLINTEGER i = 0; i < errorCount; i++) {
    ODBCError odbcError = errors[i];
    Napi::Object errorObject = Napi::Object::New(env);

    errorObject.Set(
      Napi::String::New(env, STATE),
      #ifdef UNICODE
      Napi::String::New(env, (const char16_t*)odbcError.state)
      #else
      Napi::String::New(env, (const char*)odbcError.state)
      #endif
    );
    errorObject.Set(
      Napi::String::New(env, CODE),
      Napi::Number::New(env, odbcError.code)
    );

    errorObject.Set(
      Napi::String::New(env, MESSAGE),
      #ifdef UNICODE
      Napi::String::New(env, (odbcError.message != NULL) ? (const char16_t *)odbcError.message : NO_MSG_TEXT)
      #else
      Napi::String::New(env, (odbcError.message != NULL) ? (const char *)odbcError.message : NO_MSG_TEXT)
      #endif
    );

    if (odbcError.message != NULL) {
      delete odbcError.message;
      odbcError.message = NULL;
    }

    odbcErrors.Set(i, errorObject);
  }

  error.Set(
    Napi::String::New(env, ODBC_ERRORS),
    odbcErrors
  );

  std::vector<napi_value> callbackArguments;
  callbackArguments.push_back(error.Value());

  Callback().Call(callbackArguments);
}

// After a SQL Function doesn't pass SQL_SUCCEEDED, the handle type and handle
// are sent to this function, which gets the information and stores it in an
// array of ODBCErrors
ODBCError* ODBCAsyncWorker::GetODBCErrors
(
  SQLSMALLINT handleType,
  SQLHANDLE handle
) {
  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);

  SQLSMALLINT textLength;
  SQLINTEGER statusRecCount;
  SQLRETURN returnCode;
  SQLTCHAR errorSQLState[SQL_SQLSTATE_SIZE + 1];
  SQLINTEGER nativeError;
  SQLTCHAR errorMessage[ERROR_MESSAGE_BUFFER_BYTES];
  size_t byteCount = 0;

  returnCode = SQLGetDiagField (
    handleType,      // HandleType
    handle,          // Handle
    0,               // RecNumber
    SQL_DIAG_NUMBER, // DiagIdentifier
    &statusRecCount, // DiagInfoPtr
    SQL_IS_INTEGER,  // BufferLength
    NULL             // StringLengthPtr
  );

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", returnCode, statusRecCount);

  ODBCError *odbcErrors = new ODBCError[statusRecCount]();
  this->errorCount = statusRecCount;

  for (SQLSMALLINT i = 0; i < statusRecCount; i++) {

    ODBCError error;

    DEBUG_PRINTF("ODBC::GetSQLError : calling SQLGetDiagRec; i=%i, statusRecCount=%i\n", i, statusRecCount);

    returnCode = SQLGetDiagRec(
      handleType,                 // HandleType
      handle,                     // Handle
      i + 1,                      // RecNumber
      errorSQLState,              // SQLState
      &nativeError,               // NativeErrorPtr
      errorMessage,               // MessageText
      ERROR_MESSAGE_BUFFER_CHARS, // BufferLength
      &textLength                 // TextLengthPtr
    );

    if (SQL_SUCCEEDED(returnCode)) {
      DEBUG_PRINTF("ODBC::GetSQLError : errorMessage=%s, errorSQLState=%s\n", errorMessage, errorSQLState);
      error.state = errorSQLState;
      error.code = nativeError;
      #ifdef UNICODE
        byteCount = (std::char_traits<char16_t>::length((char16_t *)errorMessage) + 1) * 2;
      #else
        byteCount = std::strlen((const char *)errorMessage) + 1;
      #endif
      error.message = new SQLTCHAR[byteCount];
      std::memcpy(error.message, errorMessage, byteCount);

    } else {
      error.state = (SQLTCHAR *)"<No error information available>";
      error.code = 0;
      error.message = (SQLTCHAR *)"<No error information available>";
    }

    odbcErrors[i] = error;
  }

  return odbcErrors;
}

// TODO: Documentation for this function
bool ODBCAsyncWorker::CheckAndHandleErrors(SQLRETURN returnCode, SQLSMALLINT handleType, SQLHANDLE handle, const char *message) {
  if (!SQL_SUCCEEDED(returnCode)) {
    this->errors = GetODBCErrors(handleType, handle);
    SetError(message);
    return true;
  }
  return false;
}

// End ODBCAsyncWorker /////////////////////////////////////////////////////////

/*
 * Connect
 */
class ConnectAsyncWorker : public ODBCAsyncWorker {

  private:

    SQLTCHAR *connectionStringPtr;
    ConnectionOptions *options;
    SQLSMALLINT maxColumnNameLength;
    SQLUINTEGER availableIsolationLevels;
    SQLHENV hEnv;
    SQLHDBC hDBC;

    SQLRETURN sqlReturnCode;

    void Execute() {
      DEBUG_PRINTF("[SQLHENV: %p] ODBC::ConnectAsyncWorker::Execute()\n", hEnv);

      uv_mutex_lock(&ODBC::g_odbcMutex);

      sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_DBC,
        hEnv,
        &hDBC
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p] ODBCConnection::ConnectAsyncWorker::Execute(): SQLAllocHandle returned %d\n", hEnv, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_ENV, hEnv);
        SetError("[odbc] Error allocating the connection handle");
        return;
      }

      if (options->connectionTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                                   // ConnectionHandle
          SQL_ATTR_CONNECTION_TIMEOUT,            // Attribute
          (SQLPOINTER) size_t(options->connectionTimeout), // ValuePtr
          SQL_IS_UINTEGER                         // StringLength
        );
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ConnectAsyncWorker::Execute(): SQLSetConnectAttr returned %d\n", hEnv, hDBC, sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
          SetError("[odbc] Error setting the connection timeout");
          return;
        }
      }
      
      if (options->loginTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                              // ConnectionHandle
          SQL_ATTR_LOGIN_TIMEOUT,            // Attribute
          (SQLPOINTER) size_t(options->loginTimeout), // ValuePtr
          SQL_IS_UINTEGER                    // StringLength
        );
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ConnectAsyncWorker::Execute(): SQLSetConnectAttr returned %d\n", hEnv, hDBC, sqlReturnCode);
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
          SetError("[odbc] Error setting the login timeout");
          return;
        }
      }

      //Attempt to connect
      sqlReturnCode = SQLDriverConnect(
        hDBC,                // ConnectionHandle
        NULL,                // WindowHandle
        connectionStringPtr, // InConnectionString
        SQL_NTS,             // StringLength1
        NULL,                // OutConnectionString
        0,                   // BufferLength - in characters
        NULL,                // StringLength2Ptr
        SQL_DRIVER_NOPROMPT  // DriverCompletion
      );
      uv_mutex_unlock(&ODBC::g_odbcMutex);
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ConnectAsyncWorker::Execute(): SQLDriverConnect returned %d\n", hEnv, hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        SetError("[odbc] Error connecting to the database");
        return;
      }

      // get information about the connection
      // maximum column length
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::Execute(): Calling SQLGetInfo(ConnectionHandle = %p, InfoType = %d (SQL_MAX_COLUMN_NAME_LEN), InfoValuePtr = %p, BufferLength = %lu, StringLengthPtr = %lu)\n", hEnv, hDBC, hDBC, SQL_MAX_COLUMN_NAME_LEN, &maxColumnNameLength, sizeof(SQLSMALLINT), NULL);
      sqlReturnCode = SQLGetInfo(
        hDBC,                    // ConnectionHandle
        SQL_MAX_COLUMN_NAME_LEN, // InfoType
        &maxColumnNameLength,    // InfoValuePtr
        sizeof(SQLSMALLINT),     // BufferLength
        NULL                     // StringLengthPtr
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::Execute(): SQLGetInfo FAILED: SQLRETURN = %d\n", hEnv, hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        SetError("[odbc] Error getting information about maximum column length from the connection");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::Execute(): SQLGetInfo succeeded: SQLRETURN = %d, InfoValue = %d\n", hEnv, hDBC, sqlReturnCode, maxColumnNameLength);

      // valid transaction levels
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::Execute(): Calling SQLGetInfo(ConnectionHandle = %p, InfoType = %d (SQL_TXN_ISOLATION_OPTION), InfoValuePtr = %p, BufferLength = %lu, StringLengthPtr = %lu)\n", hEnv, hDBC, hEnv, SQL_TXN_ISOLATION_OPTION, &availableIsolationLevels, sizeof(SQLUINTEGER), NULL);
      sqlReturnCode = SQLGetInfo(
        hDBC,
        SQL_TXN_ISOLATION_OPTION,
        &availableIsolationLevels,
        sizeof(SQLUINTEGER),
        NULL
      );
      if (!SQL_SUCCEEDED(sqlReturnCode)) {
        DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBCConnection::ConnectAsyncWorker::Execute(): SQLGetInfo returned %d\n", hEnv, hDBC, sqlReturnCode);
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        SetError("[odbc] Error getting information about available transaction isolation options from the connection");
        return;
      }
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::Execute(): SQLGetInfo succeeded: SQLRETURN = %d, InfoValue = %u\n", hEnv, hDBC, sqlReturnCode, availableIsolationLevels);
    }

    void OnOK() {
      DEBUG_PRINTF("[SQLHENV: %p][SQLHDBC: %p] ODBC::ConnectAsyncWorker::OnOk()\n", hEnv, hDBC);

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // pass the HENV and HDBC values to the ODBCConnection constructor
      std::vector<napi_value> connectionArguments;
      connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &hEnv)); // connectionArguments[0]
      connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC)); // connectionArguments[1]
      connectionArguments.push_back(Napi::External<ConnectionOptions>::New(env, options)); // connectionArguments[2]
      connectionArguments.push_back(Napi::External<SQLSMALLINT>::New(env, &maxColumnNameLength)); // connectionArguments[3]
      connectionArguments.push_back(Napi::External<SQLUINTEGER>::New(env, &availableIsolationLevels)); // connectionArguments[4]
        
      Napi::Value connection = ODBCConnection::constructor.New(connectionArguments);

      // pass the arguments to the callback function
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());  // callbackArguments[0]
      callbackArguments.push_back(connection); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }

  public:
    ConnectAsyncWorker(HENV hEnv, SQLTCHAR *connectionStringPtr, ConnectionOptions *options, Napi::Function& callback) : ODBCAsyncWorker(callback),
      connectionStringPtr(connectionStringPtr),
      options(options),
      hEnv(hEnv) {}

    ~ConnectAsyncWorker() {
      delete[] connectionStringPtr;
    }
};

// Connect
Napi::Value ODBC::Connect(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("[SQLHENV: %p] ODBC::Connect()\n", hEnv);

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString;
  Napi::Function callback;

  SQLTCHAR *connectionStringPtr = nullptr;

  ConnectionOptions *options = new ConnectionOptions();
  options->connectionTimeout = 0;
  options->loginTimeout = 0;
  options->fetchArray = false;

  if(info.Length() != 2) {
    Napi::TypeError::New(env, "connect(connectionString, callback) requires 2 parameters.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    connectionString = info[0].As<Napi::String>();
    connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);
  } else if (info[0].IsObject()) {
    Napi::Object connectionObject = info[0].As<Napi::Object>();
    if (connectionObject.Has("connectionString") && connectionObject.Get("connectionString").IsString()) {
      connectionString = connectionObject.Get("connectionString").As<Napi::String>();
      connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);
    } else {
      Napi::TypeError::New(env, "connect: A configuration object must have a 'connectionString' property that is a string.").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (connectionObject.Has("connectionTimeout") && connectionObject.Get("connectionTimeout").IsNumber()) {
      options->connectionTimeout = connectionObject.Get("connectionTimeout").As<Napi::Number>().Int32Value();
    }
    if (connectionObject.Has("loginTimeout") && connectionObject.Get("loginTimeout").IsNumber()) {
      options->loginTimeout = connectionObject.Get("loginTimeout").As<Napi::Number>().Int32Value();
    }
    if (connectionObject.Has("fetchArray") && connectionObject.Get("fetchArray").IsBoolean()) {
      options->fetchArray = connectionObject.Get("fetchArray").As<Napi::Boolean>();
    }
  } else {
    Napi::TypeError::New(env, "connect: first parameter must be a string or an object.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[1].IsFunction()) {
    callback = info[1].As<Napi::Function>();
  } else {
    Napi::TypeError::New(env, "connect: second parameter must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  ConnectAsyncWorker *worker = new ConnectAsyncWorker(hEnv, connectionStringPtr, options, callback);
  worker->Queue();

  return env.Undefined();
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// UTILITY FUNCTIONS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Take a Napi::String, and convert it to an SQLTCHAR*, which maps to:
//    UNICODE : SQLWCHAR*
// no UNICODE : SQLCHAR*
SQLTCHAR* ODBC::NapiStringToSQLTCHAR(Napi::String string) {

  size_t byteCount = 0;

  #ifdef UNICODE
    std::u16string tempString = string.Utf16Value();
    byteCount = (tempString.length() + 1) * 2;
  #else
    std::string tempString = string.Utf8Value();
    byteCount = tempString.length() + 1;
  #endif
  SQLTCHAR *sqlString = new SQLTCHAR[byteCount];
  std::memcpy(sqlString, tempString.c_str(), byteCount);
  return sqlString;
}

/******************************************************************************
 **************************** BINDING PARAMETERS ******************************
 *****************************************************************************/

/*
 * GetParametersFromArray
 *  Array of parameters can hold either/and:
 *    Value:
 *      One value to bind, In/Out defaults to SQL_PARAM_INPUT, dataType defaults based on the value
 *    Arrays:ns when you elect n
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


// This function solves the problem of losing access to "Napi::Value"s when entering
// an AsyncWorker. In Connection::Query, once we enter an AsyncWorker we do not leave it again,
// but we can't call SQLNumParams and SQLDescribeParam until after SQLPrepare. So the array of
// values to bind to parameters must be saved off in the closest, largest data type to then
// convert to the right C Type once the SQL Type of the parameter is known.
void ODBC::StoreBindValues(Napi::Array *values, Parameter **parameters) {

  uint32_t numParameters = values->Length();

  for (uint32_t i = 0; i < numParameters; i++) {

    Napi::Value value = values->Get(i);
    Parameter *parameter = parameters[i];

    if(value.IsNull()) {
      parameter->ValueType = SQL_C_DEFAULT;
      parameter->ParameterValuePtr = NULL;
      parameter->StrLen_or_IndPtr = SQL_NULL_DATA;
    } else if (value.IsBigInt()) {
      // TODO: need to check for signed/unsigned?
      bool lossless = true;
      parameter->ValueType = SQL_C_SBIGINT;
      parameter->ParameterValuePtr = new int64_t(value.As<Napi::BigInt>().Int64Value(&lossless));
    } else if (value.IsNumber()) {
      double double_val = value.As<Napi::Number>().DoubleValue();
      int64_t int_val = value.As<Napi::Number>().Int64Value();
      if (double_val == int_val) {
        parameter->ValueType = SQL_C_SBIGINT;
        parameter->ParameterValuePtr = new int64_t(value.As<Napi::Number>().Int64Value());
      } else {
        parameter->ValueType = SQL_C_DOUBLE;
        parameter->ParameterValuePtr = new double(value.As<Napi::Number>().DoubleValue());
      }
    } else if (value.IsBoolean()) {
      parameter->ValueType = SQL_C_BIT;
      parameter->ParameterValuePtr = new bool(value.As<Napi::Boolean>().Value());
    } else if (value.IsBuffer()) {
      Napi::Buffer<SQLCHAR> bufferValue = value.As<Napi::Buffer<SQLCHAR>>();
      parameter->ValueType = SQL_C_BINARY;
      parameter->BufferLength = bufferValue.Length();
      parameter->ParameterValuePtr = new SQLCHAR[parameter->BufferLength];
      parameter->StrLen_or_IndPtr = parameter->BufferLength;
      memcpy((SQLCHAR *) parameter->ParameterValuePtr, bufferValue.Data(), parameter->BufferLength);
    } else if (value.IsArrayBuffer()) {
      Napi::ArrayBuffer arrayBufferValue = value.As<Napi::ArrayBuffer>();
      parameter->ValueType = SQL_C_BINARY;
      parameter->BufferLength = arrayBufferValue.ByteLength();
      parameter->ParameterValuePtr = new SQLCHAR[parameter->BufferLength];
      parameter->StrLen_or_IndPtr = parameter->BufferLength;
      memcpy((SQLCHAR *) parameter->ParameterValuePtr, arrayBufferValue.Data(), parameter->BufferLength);
    } else if (value.IsString()) {
      Napi::String string = value.ToString();
      parameter->ValueType = SQL_C_WCHAR;
      parameter->BufferLength = (string.Utf16Value().length() + 1) * sizeof(SQLWCHAR);
      parameter->ParameterValuePtr = new SQLWCHAR[parameter->BufferLength];
      parameter->StrLen_or_IndPtr = SQL_NTS;
      memcpy((SQLWCHAR*) parameter->ParameterValuePtr, string.Utf16Value().c_str(), parameter->BufferLength);
    } else {
      // TODO: Throw error, don't support other types
    }
  }
}

SQLRETURN ODBC::DescribeParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN returnCode = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (SQLSMALLINT i = 0; i < parameterCount; i++) {

    Parameter *parameter = parameters[i];

    // "Except in calls to procedures, all parameters in SQL statements are input parameters."
    parameter->InputOutputType = SQL_PARAM_INPUT;

    returnCode = SQLDescribeParam(
      hSTMT,                     // StatementHandle,
      i + 1,                     // ParameterNumber,
      &parameter->ParameterType, // DataTypePtr,
      &parameter->ColumnSize,    // ParameterSizePtr,
      &parameter->DecimalDigits, // DecimalDigitsPtr,
      NULL                       // NullablePtr // Don't care for this package, send NULLs and get error
    );

    // if there is an error, return early and retrieve error in calling function
    if (!SQL_SUCCEEDED(returnCode)) {
      return returnCode;
    }
  }

  return returnCode;
}

SQLRETURN ODBC::BindParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN sqlReturnCode = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (int i = 0; i < parameterCount; i++) {

    Parameter* parameter = parameters[i];

    DEBUG_PRINTF("[TODO][SQLHSTMT: %p] ODBC::BindParameters(): Calling SQLBindParameter(StatementHandle = %p, ParameterNumber = %d, InputOutputType = %d, ValueType = %d, ParameterType = %d, ColumnSize = %lu, DecimalDigits = %d, ParameterValuePtr = %p, BufferLength = %ld, StrLen_or_IndPtr = %p)\n", hSTMT, hSTMT, i + i, parameter->InputOutputType, parameter->ValueType, parameter->ParameterType, parameter->ColumnSize, parameter->DecimalDigits, parameter->ParameterValuePtr, parameter->BufferLength, &parameter->StrLen_or_IndPtr);
    sqlReturnCode = SQLBindParameter(
      hSTMT,                        // StatementHandle
      i + 1,                        // ParameterNumber
      parameter->InputOutputType,   // InputOutputType
      parameter->ValueType,         // ValueType
      parameter->ParameterType,     // ParameterType
      parameter->ColumnSize,        // ColumnSize
      parameter->DecimalDigits,     // DecimalDigits
      parameter->ParameterValuePtr, // ParameterValuePtr
      parameter->BufferLength,      // BufferLength
      &parameter->StrLen_or_IndPtr  // StrLen_or_IndPtr
    );
    // If there was an error, return early
    if (!SQL_SUCCEEDED(sqlReturnCode)) {
      DEBUG_PRINTF("[TODO][SQLHSTMT: %p] ODBC::BindParameters(): SQLBindParameter FAILED with SQLRETURN = %d", hSTMT, sqlReturnCode);
      return sqlReturnCode;
    }
    DEBUG_PRINTF("[TODO][SQLHSTMT: %p] ODBC::BindParameters(): SQLBindParameter passed with SQLRETURN = %d, StrLen_or_IndPtr = %ld", hSTMT, sqlReturnCode, parameter->StrLen_or_IndPtr);
  }

  // If returns success, know that SQLBindParameter returned SUCCESS or
  // SUCCESS_WITH_INFO for all calls to SQLBindParameter.
  return sqlReturnCode;
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
