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
  
  SQL_ERROR Value is: -1
  SQL_SUCCESS Value is: 0
  SQL_SQL_SUCCESS_WITH_INFO Value is: 1
  SQL_NEED_DATA Value is: 99

*/

#include <napi.h>
#include <uv.h>
#include <time.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

using namespace Napi;

Napi::FunctionReference ODBCStatement::constructor;

HENV ODBCStatement::m_hENV;
HDBC ODBCStatement::m_hDBC;
HSTMT ODBCStatement::m_hSTMT;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports) {
  DEBUG_PRINTF("ODBCStatement::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCStatement", {

    InstanceMethod("executeDirect", &ODBCStatement::ExecuteDirect),
    InstanceMethod("executeDirectSync", &ODBCStatement::ExecuteDirectSync),

    InstanceMethod("executeNonQuery", &ODBCStatement::ExecuteNonQuery),
    InstanceMethod("executeNonQuerySync", &ODBCStatement::ExecuteNonQuerySync),

    InstanceMethod("prepare", &ODBCStatement::Prepare),
    InstanceMethod("prepareSync", &ODBCStatement::PrepareSync),
    
    InstanceMethod("bind", &ODBCStatement::Bind),
    InstanceMethod("bindSync", &ODBCStatement::BindSync),

    InstanceMethod("execute", &ODBCStatement::Execute),
    InstanceMethod("executeSync", &ODBCStatement::ExecuteSync),

    InstanceMethod("close", &ODBCStatement::Close),
    InstanceMethod("closeSync", &ODBCStatement::CloseSync)
  });

  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  exports.Set("ODBCStatement", constructorFunction);

  return exports;
}

ODBCStatement::ODBCStatement(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCStatement>(info) {
  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  this->m_hSTMT = *(info[2].As<Napi::External<SQLHSTMT>>().Data());

  this->data = new QueryData;
}

ODBCStatement::~ODBCStatement() {
  this->Free();
}

void ODBCStatement::Free() {

  DEBUG_PRINTF("ODBCStatement::Free\n");

  delete data;
  
  if (m_hSTMT) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle(SQL_HANDLE_STMT, m_hSTMT);

    m_hSTMT = NULL;
    
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

/*
 * ExecuteNonQuery
 */

class ExecuteNonQueryAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteNonQueryAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~ExecuteNonQueryAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteNonQueryAsyncWorker in Execute()\n");
      data->sqlReturnCode = SQLExecute(data->hSTMT); 
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::AsyncWorkerExecuteNonQuery in OnOk()\n");  
      Napi::Env env = Env();      
      Napi::HandleScope scope(env);
      
      //First thing, let's check if the execution of the query returned any errors 
      if(data->sqlReturnCode == SQL_ERROR) {
        Napi::Value error = ODBC::GetSQLError(
          env,
          SQL_HANDLE_STMT,
          data->hSTMT,
          (char *) "[node-odbc] Error in ODBCStatement::ExecuteNonQueryAsyncWorker"
        );
        Napi::Error(env, error).ThrowAsJavaScriptException();

        std::vector<napi_value> errorVec;
        errorVec.push_back(error);
        errorVec.push_back(env.Undefined());
        Callback().Call(errorVec);
      }
      else {
        SQLLEN rowCount = 0;
        data->sqlReturnCode = SQLRowCount(data->hSTMT, &rowCount);
        
        if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
          rowCount = 0;
        }

        uv_mutex_lock(&ODBC::g_odbcMutex);
        SQLFreeStmt(data->hSTMT, SQL_CLOSE);
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        
        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());    // callbackArguments[0]  
        callbackArguments.push_back(Napi::Number::New(env, rowCount)); // callbackArguments[1]
        Callback().Call(callbackArguments);
      }
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::ExecuteNonQuery(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteNonQuery\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // REQ_FUN_ARG(0, callback);
  if(!info[0].IsFunction()){
    Napi::Error::New(env, "Argument 1 Must be a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  ExecuteNonQueryAsyncWorker *worker = new ExecuteNonQueryAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::ExecuteNonQuerySync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO
  return env.Undefined();
}

/*
 * ExecuteDirect
 * 
 */
class ExecuteDirectAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteDirectAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~ExecuteDirectAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("\nODBCConnection::QueryAsyncWorke::Execute");

      DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*)data->sql);

      if (data->paramCount > 0) {
        printf("Binding Params\n");
        // BindParams
        Parameter prm;

        for (int i = 0; i < data->paramCount; i++) {
          prm = data->params[i];

          DEBUG_TPRINTF(
            SQL_T("ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%i size=%i length=%i &length=%X\n"), i, prm.ValueType, prm.ParameterType,
            prm.BufferLength, prm.ColumnSize, prm.length, &data->params[i].length);

          data->sqlReturnCode = SQLBindParameter(
            data->hSTMT,                        // StatementHandle
            i + 1,                              // ParameterNumber
            SQL_PARAM_INPUT,                    // InputOutputType
            prm.ValueType,                      // ValueType  
            prm.ParameterType,                  // ParameterType
            prm.ColumnSize,                     // ColumnSize
            prm.DecimalDigits,                  // DecimalDigits
            prm.ParameterValuePtr,              // ParameterValuePtr
            prm.BufferLength,                   // BufferLength
            &data->params[i].StrLen_or_IndPtr); // StrLen_or_IndPtr

          if (data->sqlReturnCode == SQL_ERROR) {
            printf("\nERROR BINDING");
            return;
          }
        }
      }

      // execute the query directly
      data->sqlReturnCode = SQLExecDirect(
        data->hSTMT,
        (SQLCHAR *)data->sql,
        data->sqlLen);

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        printf("SQLExecDirect Passed\n");
        data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
        data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

        //Only loop through the recordset if there are columns
        printf("Column Count is %d\n", data->columnCount);
        if (data->columnCount > 0) {
          // fetches all data and puts it in data->storedRows
          ODBC::FetchAll(data);
        }
      } else {
        printf("\n ERROR EXECUTE DIRECT RETURN CODE");
      }
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCConnection::QueryAsyncWorker::OnOk : data->sqlReturnCode=%i, data->noResultObject=%i\n", data->sqlReturnCode);
  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      ODBC::GetSQLError(env, SQL_HANDLE_STMT, data->hSTMT, "[node-odbc] Error in ODBC::GetColumnValue");

      std::vector<napi_value> callbackArguments;


        //check to see if the result set has columns
        if (data->columnCount == 0) {
          //this most likely means that the query was something like
          //'insert into ....'

          callbackArguments.push_back(env.Null());
          callbackArguments.push_back(env.Undefined());

          Callback().Call(callbackArguments);

        }
        //check to see if there was an error
        else if (data->sqlReturnCode == SQL_ERROR)  {

          // data->objError = ODBC::GetSQLError(env,
          //     SQL_HANDLE_STMT, 
          //     hSTMT,
          //     (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll"
          // );
        }
        else {

          Napi::Array rows = ODBC::GetNapiRowData(env, &(data->storedRows), data->columns, data->columnCount, false);
          Napi::Object fields = ODBC::GetNapiColumns(env, data->columns, data->columnCount);

          // the result object returned
          Napi::Object result = Napi::Object::New(env);
          result.Set(Napi::String::New(env, "rows"), rows);
          result.Set(Napi::String::New(env, "fields"), fields);
              
          std::vector<napi_value> callbackArguments;

          callbackArguments.push_back(env.Null());
          callbackArguments.push_back(result);

          Callback().Call(callbackArguments);
      }
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::ExecuteDirect(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteDirect\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  // REQ_STRO_ARG(0, sql);
  // REQ_FUN_ARG(1, callback);
  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 1 must be a string , Argument 2 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

  std::string test = sql.Utf8Value();

#ifdef UNICODE
  // data->sqlLen = sql->Length();
  // data->sql = (uint16_t *) malloc((data->sqlLen * sizeof(uint16_t)) + sizeof(uint16_t));
  // sql.Write((uint16_t *) data->sql);

    std::u16string tempString = sql.Utf16Value();
    data->sqlLen = tempString.length()+1;
    std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
    sqlVec.push_back('\0');
    data->sql = &((*sqlVec)[0]);
#else
  // data->sqlLen = sql.Utf8Length();
  // data->sql = (char *) malloc(data->sqlLen +1);
  // sql.WriteUtf8((char *) data->sql);

    std::string tempString = sql.Utf8Value();
    data->sqlLen = tempString.length()+1;
    std::vector<char> *sqlVec = new std::vector<char>(tempString .begin(), tempString.end());
    sqlVec->push_back('\0');
    data->sql = &((*sqlVec)[0]);
#endif

  ExecuteDirectAsyncWorker *worker = new ExecuteDirectAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::ExecuteDirectSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO
  return env.Undefined();
}

/*
 * Prepare
 * 
 */
class PrepareAsyncWorker : public Napi::AsyncWorker {

  public:
    PrepareAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~PrepareAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in Execute()\n");
      
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       data->hSTMT
      );

      data->sqlReturnCode = SQLPrepare(
        data->hSTMT,
        (SQLCHAR *) data->sql, 
        data->sqlLen);   
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in OnOk()\n");
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       data->hSTMT
      );

      Napi::Env env = Env();  
      Napi::HandleScope scope(env);

      //First thing, let's check if the execution of the query returned any errors 
      if(data->sqlReturnCode == SQL_ERROR) {

        Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->hSTMT);
        std::vector<napi_value> error;
        error.push_back(errorObj);
        error.push_back(env.Null());
        Callback().Call(error);
      }
      else {
        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());
        callbackArguments.push_back(Napi::Boolean::New(env, 1));
        Callback().Call(callbackArguments);
      }
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Prepare(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::Prepare\n");
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);
  //TODO: Figure out why macros are not working.
  // REQ_STRO_ARG(0, sql);
  // REQ_FUN_ARG(1, callback);

  if(!info[0].IsString() || !info[1].IsFunction()){
    Napi::TypeError::New(env, "Argument 0 must be a string , Argument 1 must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::String sql = info[0].ToString();
  Napi::Function callback = info[1].As<Napi::Function>();

#ifdef UNICODE
  // data->sqlLen = sql->Length();
  // data->sql = (uint16_t *) malloc((data->sqlLen * sizeof(uint16_t)) + sizeof(uint16_t));
  // sql->Write((uint16_t *) data->sql);

  std::u16string tempString = sql.Utf16Value();
  data->sqlLen = tempString.length() + 1;
  std::vector<char16_t> *sqlVec = new std::vector<char16_t>(tempString .begin(), tempString.end());
  sqlVec.push_back('\0');
  data->sql = &((*sqlVec)[0]);
#else
  // data->sqlLen = sql->Utf8Length();
  // data->sql = (char *) malloc(data->sqlLen +1);
  // sql->WriteUtf8((char *) data->sql);

  // std::string tempString = sql.Utf8Value();
  // data->sqlLen = tempString.length()+1;
  // std::vector<char> sqlVec(tempString .begin(), tempString .end());
  // sqlVec.push_back('\0');
  // data->sql = &sqlVec[0];

    std::string tempString = sql.Utf8Value();
    data->sqlLen = tempString.length()+1;
    std::vector<char> *sqlVec = new std::vector<char>(tempString .begin(), tempString.end());
    sqlVec->push_back('\0');
    data->sql = &((*sqlVec)[0]);
#endif

  PrepareAsyncWorker *worker = new PrepareAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::PrepareSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO
  return env.Undefined();
}

/*
 * Bind
 */
class BindAsyncWorker : public Napi::AsyncWorker {

  public:
    BindAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
    data(odbcStatementObject->data) {}

    ~BindAsyncWorker() { }

    void Execute() {   

      if (data->paramCount > 0) {
        // binds all parameters to the query
        ODBC::BindParameters(data);
      }    
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::UV_AfterBind\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      callbackArguments.push_back(Napi::Boolean::New(env, true));
      Callback().Call(callbackArguments);
    }

    void OnError(const Error &e) {

      DEBUG_PRINTF("ODBCStatement::UV_AfterBind\n");

      Napi::Env env = Env();      
      Napi::HandleScope scope(env);

      Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->hSTMT);
      std::vector<napi_value> error;
      error.push_back(errorObj);
      error.push_back(Napi::Boolean::New(env, false));
      Callback().Call(error);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Bind(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::Bind\n");
  
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() || !info[1].IsFunction() ) {
    Napi::Error::New(env, "Argument 1 must be an Array & Argument 2 Must be a Function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array parameterArray = info[0].As<Napi::Array>();
  Napi::Function callback = info[1].As<Napi::Function>();

  this->data->params = ODBC::GetParametersFromArray(&parameterArray, &(data->paramCount));
  
  BindAsyncWorker *worker = new BindAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::BindSync(const Napi::CallbackInfo& info) {

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO
  return env.Undefined();
}

/*
 * Execute
 */
class ExecuteAsyncWorker : public Napi::AsyncWorker {
  public:
    ExecuteAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcStatementObject(odbcStatementObject),
      data(odbcStatementObject->data) {}

    ~ExecuteAsyncWorker() {}

    void Execute() {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::Execute()\n");

      data->sqlReturnCode = SQLExecute(data->hSTMT);

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        
        data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
        data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

      } else {
        SetError("TODO");
      }
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::OnOk()\n");  

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // TODO:: return an ODBCResult
    }

    void OnError(Error &e) {

      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker::OnOk()\n");  

      // TODO: handle error
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Execute(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::Execute\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  REQ_FUN_ARG(0, callback); 

  ExecuteAsyncWorker *worker = new ExecuteAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::ExecuteSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::ExecuteSync()\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  data->sqlReturnCode = SQLExecute(data->hSTMT);

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    
    data->columns = ODBC::GetColumns(data->hSTMT, &data->columnCount); // get new data
    data->boundRow = ODBC::BindColumnData(data->hSTMT, data->columns, &data->columnCount); // bind columns

  } else {
    // TODO Throw error
  }
}


/******************************************************************************
 ********************************** CLOSE *************************************
 *****************************************************************************/

class CloseAsyncWorker : public Napi::AsyncWorker {
  public:
    CloseAsyncWorker(ODBCStatement *odbcStatementObject, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcStatementObject(odbcStatementObject),
      data(odbcStatementObject->data) {}

    ~CloseAsyncWorker() {}

    void Execute() {

      // TODO: Get this in here
      int closeOption;

      if (closeOption == SQL_DESTROY) {
        odbcStatementObject->Free();
      } else {
        uv_mutex_lock(&ODBC::g_odbcMutex);
        data->sqlReturnCode = SQLFreeStmt(odbcStatementObject->m_hSTMT, closeOption);
        uv_mutex_unlock(&ODBC::g_odbcMutex);
      }

      if (SQL_SUCCEEDED(data->sqlReturnCode)) {
        return;
      } else {
        SetError("TODO");
      }

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::Execute()\n");
    }

    void OnOK() {

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::OnOk()\n");  

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());
      Callback().Call(callbackArguments);
    }

    void OnError(Error &e) {

      DEBUG_PRINTF("ODBCStatement::CloseAsyncWorker::OnError()\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Undefined()); // TODO: Case error here
      Callback().Call(callbackArguments);
    }

  private:
    ODBCStatement *odbcStatementObject;
    QueryData *data;
};

Napi::Value ODBCStatement::Close(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::Close\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  // TODO: Handle these arguments
  // REQ_INT_ARG(0, closeOption);
  REQ_FUN_ARG(1, callback);

  CloseAsyncWorker *worker = new CloseAsyncWorker(this, callback);
  worker->Queue();

  return env.Undefined();
}

Napi::Value ODBCStatement::CloseSync(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBCStatement::CloseSync\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  
  DEBUG_PRINTF("ODBCStatement::CloseSync closeOption=%i\n", closeOption);
  
  if (closeOption == SQL_DESTROY) {
    this->Free();
  } else {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    data->sqlReturnCode = SQLFreeStmt(this->m_hSTMT, closeOption);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  if (SQL_SUCCEEDED(data->sqlReturnCode)) {
    return Napi::Boolean::New(env, true);
  } else {
    // return false if there was a problem closing the statement
    return Napi::Boolean::New(env, false);
  }
}