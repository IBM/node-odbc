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

#include <time.h>
#include <stdlib.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"
#include "odbc_cursor.h"

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

size_t strlen16(const char16_t* string)
{
   const char16_t* str = string;
   while(*str) str++;
   return str - string;
}

// error strings
const char* ODBC_ERRORS = "odbcErrors\0";
const char* STATE       = "state\0";
const char* CODE        = "code\0";
const char* MESSAGE     = "message\0";
#ifdef UNICODE
  const SQLTCHAR NO_STATE_TEXT = L'\0';
  const SQLTCHAR* NO_MSG_TEXT = (SQLTCHAR *)L"<No error information available>\0";
  const size_t NO_MSG_TEXT_LENGTH = strlen16((char16_t *)NO_MSG_TEXT);
#else
  const SQLTCHAR NO_STATE_TEXT = '\0';
  const char* NO_MSG_TEXT = "<No error information available>\0";
  const size_t NO_MSG_TEXT_LENGTH = strlen(NO_MSG_TEXT);
#endif
// byte count, needed for memcpy
const size_t NO_MSG_TEXT_SIZE = NO_MSG_TEXT_LENGTH * sizeof(SQLTCHAR);

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

  // Export the integer values for each data type so developers can utilize
  // them programmatically if needed
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_CHAR", Napi::Number::New(env, SQL_CHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_VARCHAR", Napi::Number::New(env, SQL_VARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_LONGVARCHAR", Napi::Number::New(env, SQL_LONGVARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_WCHAR", Napi::Number::New(env, SQL_WCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_WVARCHAR", Napi::Number::New(env, SQL_WVARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_WLONGVARCHAR", Napi::Number::New(env, SQL_WLONGVARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_DECIMAL", Napi::Number::New(env, SQL_DECIMAL), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NUMERIC", Napi::Number::New(env, SQL_NUMERIC), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_SMALLINT", Napi::Number::New(env, SQL_SMALLINT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTEGER", Napi::Number::New(env, SQL_INTEGER), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_REAL", Napi::Number::New(env, SQL_REAL), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_FLOAT", Napi::Number::New(env, SQL_FLOAT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_DOUBLE", Napi::Number::New(env, SQL_DOUBLE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_BIT", Napi::Number::New(env, SQL_BIT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TINYINT", Napi::Number::New(env, SQL_TINYINT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_BIGINT", Napi::Number::New(env, SQL_BIGINT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_BINARY", Napi::Number::New(env, SQL_BINARY), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_VARBINARY", Napi::Number::New(env, SQL_VARBINARY), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_LONGVARBINARY", Napi::Number::New(env, SQL_LONGVARBINARY), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_DATE", Napi::Number::New(env, SQL_TYPE_DATE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_TIME", Napi::Number::New(env, SQL_TYPE_TIME), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_TIMESTAMP", Napi::Number::New(env, SQL_TYPE_TIMESTAMP), napi_enumerable));
  // These are listed in the Microsoft ODBC documentation, but don't appear to
  // be in unixODBC
  // ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_UTCDATETIME", Napi::Number::New(env, SQL_TYPE_UTCDATETIME), napi_enumerable));
  // ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_TYPE_UTCTIME", Napi::Number::New(env, SQL_TYPE_UTCTIME), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_MONTH", Napi::Number::New(env, SQL_INTERVAL_MONTH), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_YEAR", Napi::Number::New(env, SQL_INTERVAL_YEAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_YEAR_TO_MONTH", Napi::Number::New(env, SQL_INTERVAL_YEAR_TO_MONTH), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_DAY", Napi::Number::New(env, SQL_INTERVAL_DAY), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_HOUR", Napi::Number::New(env, SQL_INTERVAL_HOUR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_MINUTE", Napi::Number::New(env, SQL_INTERVAL_MINUTE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_SECOND", Napi::Number::New(env, SQL_INTERVAL_SECOND), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_DAY_TO_HOUR", Napi::Number::New(env, SQL_INTERVAL_DAY_TO_HOUR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_DAY_TO_MINUTE", Napi::Number::New(env, SQL_INTERVAL_DAY_TO_MINUTE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_DAY_TO_SECOND", Napi::Number::New(env, SQL_INTERVAL_DAY_TO_SECOND), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_HOUR_TO_MINUTE", Napi::Number::New(env, SQL_INTERVAL_HOUR_TO_MINUTE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_HOUR_TO_SECOND", Napi::Number::New(env, SQL_INTERVAL_HOUR_TO_SECOND), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTERVAL_MINUTE_TO_SECOND", Napi::Number::New(env, SQL_INTERVAL_MINUTE_TO_SECOND), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_GUID", Napi::Number::New(env, SQL_GUID), napi_enumerable));
  // End data types


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

  SQLRETURN return_code;

  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);

  uv_mutex_lock(&ODBC::g_odbcMutex);
  // Initialize the Environment handle
  return_code =
  SQLAllocHandle
  (
    SQL_HANDLE_ENV,
    SQL_NULL_HANDLE,
    &hEnv
  );
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  if (!SQL_SUCCEEDED(return_code)) {
    // TODO: Redo
    // Napi::Error(env, Napi::String::New(env, (const char*)ODBC::GetSQLErrors(SQL_HANDLE_ENV, hEnv)[0].message)).ThrowAsJavaScriptException();
    return env.Null();
  }

  // Use ODBC 3.x behavior
  return_code = 
  SQLSetEnvAttr
  (
    hEnv,
    SQL_ATTR_ODBC_VERSION,
    (SQLPOINTER) SQL_OV_ODBC3,
    SQL_IS_UINTEGER
  );

  return exports;
}

ODBC::~ODBC() {

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

    errorObject.Set
    (
      Napi::String::New(env, STATE),
      #ifdef UNICODE
      Napi::String::New(env, (odbcError.state != NULL) ? (const char16_t*)odbcError.state : (const char16_t*)L"")
      #else
      Napi::String::New(env, (odbcError.state != NULL) ? (const char*)odbcError.state : "")
      #endif
    );

    errorObject.Set
    (
      Napi::String::New(env, CODE),
      Napi::Number::New(env, odbcError.code)
    );

    errorObject.Set
    (
      Napi::String::New(env, MESSAGE),
      #ifdef UNICODE
      Napi::String::New(env, (odbcError.message != NULL) ? (const char16_t *)odbcError.message : (const char16_t *)NO_MSG_TEXT)
      #else
      Napi::String::New(env, (odbcError.message != NULL) ? (const char *)odbcError.message : (const char *)NO_MSG_TEXT)
      #endif
    );

    // Error message has been copied off of the C ODBC error stucture, and can
    // now be deleted
    if (odbcError.message != NULL)
    {
      delete[] odbcError.message;
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
) 
{
  SQLRETURN return_code;
  SQLSMALLINT error_message_length = ERROR_MESSAGE_BUFFER_CHARS;
  SQLINTEGER statusRecCount;

  return_code = SQLGetDiagField
  (
    handleType,      // HandleType
    handle,          // Handle
    0,               // RecNumber
    SQL_DIAG_NUMBER, // DiagIdentifier
    &statusRecCount, // DiagInfoPtr
    SQL_IS_INTEGER,  // BufferLength
    NULL             // StringLengthPtr
  );

  if (!SQL_SUCCEEDED(return_code))
  {
    ODBCError *odbcErrors = new ODBCError[1];
    ODBCError error;
    error.state[0] = NO_STATE_TEXT;
    error.code = 0;
    error.message = new SQLTCHAR[NO_MSG_TEXT_LENGTH + 1];
    memcpy(error.message, NO_MSG_TEXT, NO_MSG_TEXT_SIZE + sizeof(SQLTCHAR));
    odbcErrors[0] = error;
    return odbcErrors;
  }

  ODBCError *odbcErrors = new ODBCError[statusRecCount];
  this->errorCount = statusRecCount;

  for (SQLSMALLINT i = 0; i < statusRecCount; i++) {

    ODBCError error;

    SQLSMALLINT new_error_message_length;
    return_code = SQL_SUCCESS;

    while(SQL_SUCCEEDED(return_code))
    {
      error.message = new SQLTCHAR[error_message_length];

      return_code =
      SQLGetDiagRec
      (
        handleType,                // HandleType
        handle,                    // Handle
        i + 1,                     // RecNumber
        error.state,               // SQLState
        &error.code,               // NativeErrorPtr
        error.message,             // MessageText
        error_message_length,      // BufferLength
        &new_error_message_length  // TextLengthPtr
      );

      if (error_message_length > new_error_message_length)
      {
        break;
      }

      delete[] error.message;
      error_message_length = new_error_message_length + 1;
    }

    if (!SQL_SUCCEEDED(return_code))
    {
      error.state[0] = NO_STATE_TEXT;
      error.code = 0;
      memcpy(error.message, NO_MSG_TEXT, NO_MSG_TEXT_SIZE + 1); 
    }

    odbcErrors[i] = error;
  }

  return odbcErrors;
}

// TODO: Documentation for this function
bool ODBCAsyncWorker::CheckAndHandleErrors(SQLRETURN return_code, SQLSMALLINT handleType, SQLHANDLE handle, const char *message) {
  if (!SQL_SUCCEEDED(return_code)) {
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
    GetInfoResults     get_info_results;

    SQLHENV hEnv;
    SQLHDBC hDBC;

    void Execute() {

      SQLRETURN return_code;

      uv_mutex_lock(&ODBC::g_odbcMutex);

      return_code = SQLAllocHandle(
        SQL_HANDLE_DBC,
        hEnv,
        &hDBC
      );
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_ENV, hEnv);
        SetError("[odbc] Error allocating the connection handle");
        return;
      }

      if (options->connectionTimeout > 0) {
        return_code = SQLSetConnectAttr(
          hDBC,                                   // ConnectionHandle
          SQL_ATTR_CONNECTION_TIMEOUT,            // Attribute
          (SQLPOINTER) (intptr_t) options->connectionTimeout, // ValuePtr
          SQL_IS_UINTEGER                         // StringLength
        );
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
          SetError("[odbc] Error setting the connection timeout");
          return;
        }
      }
      
      if (options->loginTimeout > 0) {
        return_code =
        SQLSetConnectAttr
        (
          hDBC,                                            // ConnectionHandle
          SQL_ATTR_LOGIN_TIMEOUT,                          // Attribute
          (SQLPOINTER) (intptr_t) options->loginTimeout, // ValuePtr
          SQL_IS_UINTEGER                                  // StringLength
        );
        if (!SQL_SUCCEEDED(return_code)) {
          this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
          SetError("[odbc] Error setting the login timeout");
          return;
        }
        // "If the specified timeout exceeds the maximum login timeout in the
        // data source, the driver substitutes that value and returns SQLSTATE
        // 01S02 (Option value changed)."
        if (return_code == SQL_SUCCESS_WITH_INFO)
        {
          return_code =
          SQLGetConnectAttr
          (
            hDBC,
            SQL_ATTR_LOGIN_TIMEOUT,
            (SQLPOINTER) &options->loginTimeout,
            IGNORED_PARAMETER,
            NULL
          );
          if (!SQL_SUCCEEDED(return_code)) {
            this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
            SetError("[odbc] Error setting retrieving the changed login timeout");
            return;
          }
        }
      }

      //Attempt to connect
      return_code =
      SQLDriverConnect
      (
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
      if (!SQL_SUCCEEDED(return_code)) {
        this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        SetError("[odbc] Error connecting to the database");
        return;
      }

      // get information about the connection
      // maximum column length
      return_code =
      SQLGetInfo
      (
        hDBC,                                     // ConnectionHandle
        SQL_MAX_COLUMN_NAME_LEN,                  // InfoType
        &get_info_results.max_column_name_length, // InfoValuePtr
        sizeof(SQLSMALLINT),                      // BufferLength
        NULL                                      // StringLengthPtr
      );
      // Some poorly-behaved drivers do not implement SQL_MAX_COLUMN_NAME_LEN,
      // and return SQL_ERROR instead of setting the value to 0 like the spec
      // requires. Bite the bullet and ignore any errors here, instead setting
      // the value to something sane like 128 ("An FIPS Intermediate
      // level-conformant driver will return at least 128.").
      if (!SQL_SUCCEEDED(return_code) || get_info_results.max_column_name_length == 0) {
        get_info_results.max_column_name_length = 128;
        // this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        // SetError("[odbc] Error getting information about maximum column length from the connection");
        // return;
      }

      // valid transaction levels
      return_code =
      SQLGetInfo
      (
        hDBC,                                         // ConnectionHandle
        SQL_TXN_ISOLATION_OPTION,                     // InfoType
        &get_info_results.available_isolation_levels, // InfoValuePtr
        sizeof(SQLUINTEGER),                          // BufferLength
        NULL                                          // StringLengthPtr
      );
      // Some poorly-behaved drivers do not implement SQL_TXN_ISOLATION_OPTION,
      // and return SQL_ERROR. Bite the bullet again and ignore any errors here,
      // instead setting the bitmask to 0 so no isolation levels are listed as
      // supported.
      if (!SQL_SUCCEEDED(return_code)) {
        get_info_results.available_isolation_levels = 0;
        // this->errors = GetODBCErrors(SQL_HANDLE_DBC, hDBC);
        // SetError("[odbc] Error getting information about available transaction isolation options from the connection");
        // return;
      }

      SQLUINTEGER sql_getdata_extensions_bitmask;

      // valid get data extensions
      return_code =
      SQLGetInfo
      (
        hDBC,                                         // ConnectionHandle
        SQL_GETDATA_EXTENSIONS,                       // InfoType
        (SQLPOINTER) &sql_getdata_extensions_bitmask, // InfoValuePtr
        IGNORED_PARAMETER,                            // BufferLength
        IGNORED_PARAMETER                             // StringLengthPtr
      );
      if (!SQL_SUCCEEDED(return_code)) {
        // Some drivers don't feel the need to implement the
        // SQL_GETDATA_EXTENSIONS option for SQLGetInfo, so this call will
        // return an error. Instead of returning an error, just set all of
        // the SQLGetData extensions to false and continue.
        get_info_results.sql_get_data_supports.any_column    = false;
        get_info_results.sql_get_data_supports.any_order     = false;
        get_info_results.sql_get_data_supports.block         = false;
        get_info_results.sql_get_data_supports.bound         = false;
        get_info_results.sql_get_data_supports.output_params = false;
      } else {
        // call the bitmask to populate the sql_get_data_supports struct
        get_info_results.sql_get_data_supports.any_column =
          (bool) (sql_getdata_extensions_bitmask & SQL_GD_ANY_COLUMN);
        get_info_results.sql_get_data_supports.any_order =
          (bool) (sql_getdata_extensions_bitmask & SQL_GD_ANY_ORDER);
        get_info_results.sql_get_data_supports.block =
          (bool) (sql_getdata_extensions_bitmask & SQL_GD_BLOCK);
        get_info_results.sql_get_data_supports.bound =
          (bool) (sql_getdata_extensions_bitmask & SQL_GD_BOUND);
        get_info_results.sql_get_data_supports.output_params =
          (bool) (sql_getdata_extensions_bitmask & SQL_GD_OUTPUT_PARAMS);
      }
    }

    void OnOK() {

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // pass the HENV and HDBC values to the ODBCConnection constructor
      std::vector<napi_value> connectionArguments;
      connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &hEnv)); // connectionArguments[0]
      connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC)); // connectionArguments[1]
      connectionArguments.push_back(Napi::External<ConnectionOptions>::New(env, options)); // connectionArguments[2]
      connectionArguments.push_back(Napi::External<GetInfoResults>::New(env, &get_info_results)); // connectionArguments[3]
        
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
      delete options;
      delete[] connectionStringPtr;
    }
};

// Connect
Napi::Value ODBC::Connect(const Napi::CallbackInfo& info) {

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
      parameter->ParameterValuePtr = new SQLBIGINT(value.As<Napi::BigInt>().Int64Value(&lossless));
      parameter->isbigint = true;
    } else if (value.IsNumber()) {
      double double_val = value.As<Napi::Number>().DoubleValue();
      int64_t int_val = value.As<Napi::Number>().Int64Value();
      if (double_val == int_val) {
        parameter->ValueType = SQL_C_SBIGINT;
        parameter->ParameterValuePtr = new SQLBIGINT(value.As<Napi::Number>().Int64Value());
        parameter->isbigint = false;
      } else {
        parameter->ValueType = SQL_C_DOUBLE;
        parameter->ParameterValuePtr = new SQLDOUBLE(value.As<Napi::Number>().DoubleValue());
      }
    } else if (value.IsBoolean()) {
      parameter->ValueType = SQL_C_BIT;
      parameter->ParameterValuePtr = new bool(value.As<Napi::Boolean>().Value());
    } else if (value.IsBuffer()) {
      Napi::Buffer<SQLCHAR> bufferValue = value.As<Napi::Buffer<SQLCHAR>>();
      parameter->ValueType = SQL_C_BINARY;
      parameter->BufferLength = bufferValue.Length();
      parameter->ParameterValuePtr = new SQLCHAR[parameter->BufferLength]();
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
      // Napi::String string = value.ToString();
      // parameter->ValueType = SQL_C_WCHAR;
      // parameter->BufferLength = (string.Utf16Value().length() + 1) * sizeof(SQLWCHAR);
      // parameter->ParameterValuePtr = new SQLWCHAR[parameter->BufferLength];
      // parameter->StrLen_or_IndPtr = SQL_NTS;
      // memcpy((SQLWCHAR*) parameter->ParameterValuePtr, string.Utf16Value().c_str(), parameter->BufferLength);
      Napi::String string = value.ToString();
      parameter->ValueType = SQL_C_CHAR;
      parameter->BufferLength = (string.Utf8Value().length() + 1);
      parameter->ParameterValuePtr = new SQLCHAR[parameter->BufferLength]();
      parameter->StrLen_or_IndPtr = SQL_NTS;
      memcpy((SQLCHAR*) parameter->ParameterValuePtr, string.Utf8Value().c_str(), parameter->BufferLength);
    } else {
      // TODO: Throw error, don't support other types
    }
  }
}

SQLRETURN ODBC::DescribeParameters(SQLHSTMT hstmt, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN return_code = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (SQLSMALLINT i = 0; i < parameterCount; i++) {

    Parameter *parameter = parameters[i];

    // "Except in calls to procedures, all parameters in SQL statements are input parameters."
    parameter->InputOutputType = SQL_PARAM_INPUT;

    return_code =
    SQLDescribeParam
    (
      hstmt,                     // StatementHandle,
      i + 1,                     // ParameterNumber,
      &parameter->ParameterType, // DataTypePtr,
      &parameter->ColumnSize,    // ParameterSizePtr,
      &parameter->DecimalDigits, // DecimalDigitsPtr,
      &parameter->Nullable       // NullablePtr
    );

    // if there is an error, return early and retrieve error in calling function
    if (!SQL_SUCCEEDED(return_code)) {
      return return_code;
    }
  }

  return return_code;
}

SQLRETURN ODBC::BindParameters(SQLHSTMT hstmt, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN return_code = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (int i = 0; i < parameterCount; i++) {

    Parameter* parameter = parameters[i];

    return_code = SQLBindParameter(
      hstmt,                        // StatementHandle
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
    if (!SQL_SUCCEEDED(return_code)) {
      return return_code;
    }
  }

  // If returns success, know that SQLBindParameter returned SUCCESS or
  // SUCCESS_WITH_INFO for all calls to SQLBindParameter.
  return return_code;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {

  ODBC::Init(env, exports);
  ODBCConnection::Init(env, exports);
  ODBCStatement::Init(env, exports);
  ODBCCursor::Init(env, exports);

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
