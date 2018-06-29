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

#include <string.h>
#include <napi.h>
#include <uv.h>
#include <node_version.h>
#include <time.h>
#include <uv.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"
#include "iostream"

using namespace Napi;

Napi::FunctionReference ODBCStatement::constructor;

HENV ODBCStatement::m_hENV;
HDBC ODBCStatement::m_hDBC;
HSTMT ODBCStatement::m_hSTMT;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports, HENV hENV, HDBC hDBC, HSTMT hSTMT) {
  DEBUG_PRINTF("ODBCStatement::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCStatement", {
    InstanceMethod("execute", &ODBCStatement::Execute),
    InstanceMethod("executeSync", &ODBCStatement::ExecuteSync),
    
    InstanceMethod("executeDirect", &ODBCStatement::ExecuteDirect),
    InstanceMethod("executeDirectSync", &ODBCStatement::ExecuteDirectSync),
    
    InstanceMethod("executeNonQuery", &ODBCStatement::ExecuteNonQuery),
    InstanceMethod("executeNonQuerySync", &ODBCStatement::ExecuteNonQuerySync),
    
    InstanceMethod("prepare", &ODBCStatement::Prepare),
    InstanceMethod("prepareSync", &ODBCStatement::PrepareSync),
    
    InstanceMethod("bind", &ODBCStatement::Bind),
    InstanceMethod("bindSync", &ODBCStatement::BindSync),
    
    InstanceMethod("closeSync", &ODBCStatement::CloseSync)
  });

  // Attach the Database Constructor to the target object
  constructor = Napi::Persistent(constructorFunction);
  constructor.SuppressDestruct();

  ODBCStatement::m_hENV = hENV;
  ODBCStatement::m_hDBC = hDBC;
  ODBCStatement::m_hSTMT = hSTMT;

  exports.Set("ODBCStatement", constructorFunction);

  return exports;
}

ODBCStatement::ODBCStatement(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ODBCStatement>(info) {
  printf("\nODBCStatement Constructor\n");
  printf("Length of info is %d\n", info.Length());
  printf("setting m_henv\n");
  this->m_hENV = *(info[0].As<Napi::External<SQLHENV>>().Data());
  printf("setting m_hDBC\n");
  this->m_hDBC = *(info[1].As<Napi::External<SQLHDBC>>().Data());
  printf("setting m_hstmt\n");
  this->m_hSTMT = *(info[2].As<Napi::External<SQLHSTMT>>().Data());
}

ODBCStatement::~ODBCStatement() {
  this->Free();
}

void ODBCStatement::Free() {
  DEBUG_PRINTF("ODBCStatement::Free\n");
  // //if we previously had parameters, then be sure to free them
  if (paramCount) {
    int count = paramCount;
    paramCount = 0;
    
    Parameter prm;
    
    //free parameter memory
    for (int i = 0; i < count; i++) {
      if (prm = params[i], prm.ParameterValuePtr != NULL) {
        switch (prm.ValueType) {
          case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break;
          case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
          case SQL_C_SBIGINT: delete (int64_t *)prm.ParameterValuePtr; break;
          case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
          case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
        }
      }
    }

    free(params);
  }
  
  if (m_hSTMT) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle(SQL_HANDLE_STMT, m_hSTMT);
    m_hSTMT = NULL;
    
    uv_mutex_unlock(&ODBC::g_odbcMutex);
    
    if (bufferLength > 0) {
      free(buffer);
    }
  }
}
//Do not think we need New() because we can call constructor.New() instead.
Napi::Value ODBCStatement::New(const Napi::CallbackInfo& info) {
  // Napi::Env env = info.Env();
  // DEBUG_PRINTF("ODBCStatement::New\n");
  // Napi::HandleScope scope(env);
  
  // REQ_EXT_ARG(0, js_henv, HENV);
  // REQ_EXT_ARG(1, js_hdbc, HDBC);
  // REQ_EXT_ARG(2, js_hstmt, HSTMT);
  
  // HENV *hENV = js_henv.Data();
  // HDBC *hDBC = js_hdbc.Data();
  // HSTMT *hSTMT = js_hstmt.Data();
  
  // //create a new OBCResult object
  // ODBCStatement* stmt = new ODBCStatement(hENV, hDBC, hSTMT);
  
  // //specify the buffer length
  // stmt->bufferLength = MAX_VALUE_SIZE - 1;
  
  // //initialze a buffer for this object
  // stmt->buffer = (uint16_t *) malloc(stmt->bufferLength + 1);
  // //TODO: make sure the malloc succeeded

  // //set the initial colCount to 0
  // stmt->colCount = 0;
  
  // //initialize the paramCount
  // stmt->paramCount = 0;
  
  // stmt->Wrap(info.Holder());
  
  // return info.Holder();
}

/*
 * Execute
 */
class ExecuteAsyncWorker : public Napi::AsyncWorker {
  public:
    ExecuteAsyncWorker(execute_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      data(data), callback(callback){}

    ~ExecuteAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker void Execute()\n");
      sqlReturnCode = SQLExecute(data->stmt->m_hSTMT); 
      data->result = sqlReturnCode;
      printf("SQLRETURN CODE SQLExecute(): %d", sqlReturnCode);
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker void OnOk()\n");  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);
        
      // //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {
        printf("SQLError(%d) Occured During ExecuteAsyncWorker OnOk() Method", data->result);

        Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->stmt->m_hSTMT );
        std::vector<napi_value> errorVec;
        errorVec.push_back(error);
        errorVec.push_back(env.Null());
        Callback().Call(errorVec);

      }
      else {
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(data->stmt->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(data->stmt->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->stmt->m_hSTMT)));
        resultArguments.push_back(Napi::Boolean::New(env, 0));

        Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());    // callbackArguments[0]  
        callbackArguments.push_back(resultObject); // callbackArguments[1]
        Callback().Call(callbackArguments);
      }
      delete data;
      free(data);
    }

  private:
    execute_work_data* data;
    SQLRETURN sqlReturnCode;
    Napi::Function& callback;
};

Napi::Value ODBCStatement::Execute(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::Execute\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  //checks that the first arg in info[] is a Napi::Function & initializes callback to be a Napi::Function
  // REQ_FUN_ARG(0, callback); 
  if(!info[0].IsFunction()){
    Napi::TypeError::New(env, "Argument 1 Must be a function");
  }

  Napi::Function callback = info[0].As<Napi::Function>();
  execute_work_data* data = 
    (execute_work_data *) calloc(1, sizeof(execute_work_data));

  data->stmt = this;
  
  ExecuteAsyncWorker *worker = new ExecuteAsyncWorker(data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * ExecuteSync
 * 
 */

Napi::Value ODBCStatement::ExecuteSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteSync\n");
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode = SQLExecute(this->m_hSTMT); 
  std::cout << "SQLRETURN CODE FROM SQLEXECUTE(): " <<sqlReturnCode << std::endl;

  if(sqlReturnCode == SQL_ERROR) {
    printf("SQLError(%d) Occured During ExecuteSync()", sqlReturnCode);
    Napi::Value error = ODBC::GetSQLError(
      env,
      SQL_HANDLE_STMT,
      this->m_hSTMT,
      (char *) "[node-odbc] Error in ODBCStatement::ExecuteSync"
    );
    Napi::Error(env, error).ThrowAsJavaScriptException();

    return env.Null();
  }
  else {
    std::vector<napi_value> resultArguments;
    resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(this->m_hENV)));
    resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
    resultArguments.push_back(Napi::External<HSTMT>::New(env, &(this->m_hSTMT)));
    resultArguments.push_back(Napi::Boolean::New(env, 0));
    Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);
    
    return resultObject;
  }
}

/*
 * ExecuteNonQuery
 */

class ExecuteNonQueryAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteNonQueryAsyncWorker(execute_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      data(data), callback(callback){}

    ~ExecuteNonQueryAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteNonQueryAsyncWorker in Execute()\n");
      sqlReturnCode = SQLExecute(data->stmt->m_hSTMT); 
      data->result = sqlReturnCode;
      printf("SQL RETURN CODE after SQLEXECUTE(%d)\n", sqlReturnCode);

    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::AsyncWorkerExecuteNonQuery in OnOk()\n");  
      Napi::Env env = Env();      
      Napi::HandleScope scope(env);
      
      //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {
      printf("SQLError(%d) Occured During ExecuteNonQueryAsyncWorker() OnOk() Method", sqlReturnCode);
        Napi::Value error = ODBC::GetSQLError(
          env,
          SQL_HANDLE_STMT,
          data->stmt->m_hSTMT,
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
        SQLRETURN sqlReturnCode = SQLRowCount(data->stmt->m_hSTMT, &rowCount);
        
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          rowCount = 0;
        }

        uv_mutex_lock(&ODBC::g_odbcMutex);
        SQLFreeStmt(data->stmt->m_hSTMT, SQL_CLOSE);
        uv_mutex_unlock(&ODBC::g_odbcMutex);
        
        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());    // callbackArguments[0]  
        callbackArguments.push_back(Napi::Number::New(env, rowCount)); // callbackArguments[1]
        Callback().Call(callbackArguments);
      }
      delete data->cb;
      
      free(data);
    }

  private:
    execute_work_data* data;
    SQLRETURN sqlReturnCode;
    Napi::Function& callback;
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

  execute_work_data* data = 
    (execute_work_data *) calloc(1, sizeof(execute_work_data));

  data->stmt = this;
  
  ExecuteNonQueryAsyncWorker *worker = new ExecuteNonQueryAsyncWorker(data, callback);
  worker->Queue();

  return env.Undefined();
}


/*
 * ExecuteNonQuerySync
 * 
 */

Napi::Value ODBCStatement::ExecuteNonQuerySync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteNonQuerySync\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  SQLRETURN sqlReturnCode; 
  sqlReturnCode = SQLExecute(this->m_hSTMT); 
  printf("SQL RETURN CODE after SQLEXECUTE(%d)", sqlReturnCode);
  
  if(sqlReturnCode == SQL_ERROR) {
    Napi::Value error = ODBC::GetSQLError(env,
      SQL_HANDLE_STMT,
      this->m_hSTMT,
      (char *) "[node-odbc] Error in ODBCStatement::ExecuteSync"
    );

    Napi::Error(env, error).ThrowAsJavaScriptException();  
    return env.Null();
  }
  else {
    SQLLEN rowCount = 0;
    
    sqlReturnCode = SQLRowCount(this->m_hSTMT, &rowCount);
    printf("SQL RETURN CODE after SQLROWCOUNT(%d)", sqlReturnCode);

    
    if (!SQL_SUCCEEDED(sqlReturnCode)) {
      rowCount = 0;
    }
    
    uv_mutex_lock(&ODBC::g_odbcMutex);
    SQLFreeStmt(this->m_hSTMT, SQL_CLOSE);
    uv_mutex_unlock(&ODBC::g_odbcMutex);
    
    return Napi::Number::New(env, rowCount);
  }
}

/*
 * ExecuteDirect
 * 
 */

class ExecuteDirectAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteDirectAsyncWorker(execute_direct_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      data(data), callback(callback){}

    ~ExecuteDirectAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteDirectAsynWorker in Execute()\n");

      sqlReturnCode = SQLExecDirect(
        data->stmt->m_hSTMT,
        (SQLCHAR *) data->sql,
        data->sqlLen);

      data->result = sqlReturnCode;
      printf("SQLRETURN CODE FROM SQLExecDirect: %d\n", sqlReturnCode);
      
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::ExecuteDirectAsyncWorker in OnOk()\n");  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      //First thing, let's check if the execution of the query returned any errors 
      printf("data-> result RETURN CODE FROM SQLExecDirect: %d\n", data->result);

      if(data->result == SQL_ERROR) {
        printf("SQLError Occured During ExecuteAsyncWorker OnOk() Method");

        Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->stmt->m_hSTMT );
        std::vector<napi_value> errorVec;

        errorVec.push_back(error);
        errorVec.push_back(env.Null());
        Callback().Call(errorVec);
      }
      else {
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(data->stmt->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(data->stmt->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(data->stmt->m_hSTMT)));
        resultArguments.push_back(Napi::Boolean::New(env, 0));

        Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

        std::vector<napi_value> callbackArguments;
        callbackArguments.push_back(env.Null());    // callbackArguments[0]  
        callbackArguments.push_back(resultObject); // callbackArguments[1]

        Callback().Call(callbackArguments);
      }
      delete data;
      
      free(data->sql);
      free(data);
    }

  private:
    execute_direct_work_data* data;
    SQLRETURN sqlReturnCode;
    Napi::Function& callback;
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
  std::cout << "SQL String FROM JS is: " << test << std::endl;

  execute_direct_work_data* data = 
    (execute_direct_work_data *) calloc(1, sizeof(execute_direct_work_data));

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

  data->stmt = this;

  ExecuteDirectAsyncWorker *worker = new ExecuteDirectAsyncWorker(data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * ExecuteDirectSync
 * 
 */

Napi::Value ODBCStatement::ExecuteDirectSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::ExecuteDirectSync\n");
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);
  
   if(!info[0].IsString()){
    Napi::Error::New(env , "Argument 1 must be a string");
    return env.Null();
  }
  Napi::String sql = info[0].ToString();
  
#ifdef UNICODE
  // REQ_WSTR_ARG(0, sql);
  std::u16string sqlString = sql.Utf16Value();
  std::vector<char16_t> *sqlVec = new std::vector<char16_t>(sqlString.begin(), sqlString.end());
  sqlVec.push_back('\0');
  data->sql = &((*sqlVec)[0]);

#else
  // REQ_STR_ARG(0, sql);
  std::string sqlString = sql.Utf8Value();
  std::vector<char> *sqlVec = new std::vector<char>(sqlString.begin(), sqlString.end());
  sqlVec->push_back('\0');
  // data->sql = &((*sqlVec)[0]);

#endif
  //TODO:: will unsigned char vector hold wstring values?
  // std::vector<unsigned char> sqlVec(sqlString.begin(), sqlString.end());
  // sqlVec.push_back('\0');
  
  SQLRETURN sqlReturnCode = SQLExecDirect(
    this->m_hSTMT,
    (SQLCHAR *) &((*sqlVec)[0]), 
    (SQLINTEGER) sqlString.length()+1);  

  if(sqlReturnCode == SQL_ERROR) {
    Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, this->m_hSTMT );
    Napi::Error(env, error).ThrowAsJavaScriptException();
    return env.Null();
  }
  else {
    std::vector<napi_value> resultArguments;
    resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(this->m_hENV)));
    resultArguments.push_back(Napi::External<HDBC>::New(env, &(this->m_hDBC)));
    resultArguments.push_back(Napi::External<HSTMT>::New(env, &(this->m_hSTMT)));
    resultArguments.push_back(Napi::Boolean::New(env, 0));

    Napi::Value resultObject = ODBCResult::constructor.New(resultArguments);

    return resultObject;
  }
}

/*
 * Prepare
 * 
 */
class PrepareAsyncWorker : public Napi::AsyncWorker {

  public:
    PrepareAsyncWorker(prepare_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      data(data), callback(callback){}

    ~PrepareAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in Execute()\n");
      
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       data->stmt->m_hENV,
       data->stmt->m_hDBC,
       data->stmt->m_hSTMT
      );

      sqlReturnCode = SQLPrepare(
        data->stmt->m_hSTMT,
        (SQLCHAR *) data->sql, 
        data->sqlLen);

      printf("SQLRETURN CODE FROM SQLPrepare: %d\n", sqlReturnCode);

      data->result = sqlReturnCode;
      
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in OnOk()\n");
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       data->stmt->m_hENV,
       data->stmt->m_hDBC,
       data->stmt->m_hSTMT
      );

      Napi::Env env = Env();  
      Napi::HandleScope scope(env);

      //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {
        printf("SQLError(%d) Occured During PrepareAsyncWorker OnOk() Method SQL_ERROR", data->result);

        Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->stmt->m_hSTMT );
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

      delete data;
      free(data->sql);
      free(data);
    }

  private:
    prepare_work_data* data;
    SQLRETURN sqlReturnCode;
    Napi::Function& callback;
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

  prepare_work_data* data = 
    (prepare_work_data *) calloc(1, sizeof(prepare_work_data));

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

  data->stmt = this;
  PrepareAsyncWorker *worker = new PrepareAsyncWorker(data, callback);
  worker->Queue();

  return env.Undefined();
}


/*
 * PrepareSync
 * 
 */
Napi::Value ODBCStatement::PrepareSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::PrepareSync\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  // REQ_STRO_ARG(0, sql);
  if(!info[0].IsString()){
    Napi::Error::New(env, "Argument 1 must be a string");
    return env.Null();
  }
  Napi::String sql = info[0].ToString();
  
  SQLRETURN sqlReturnCode;

#ifdef UNICODE
  // int sqlLen = sql->Length() + 1;
  // uint16_t* sql2 = (uint16_t *) malloc(sqlLen * sizeof(uint16_t));
  // sql->Write(sql2);
  std::cout << "UNICODE IS DEFINED";
  std::u16string tempString = sql.Utf16Value();
  std::vector<char16_t> sqlVec(tempString .begin(), tempString .end());
  sqlVec.push_back('\0');
  int sqlLen = tempString.length()+1;
  data->sql = &sqlVec[0];


#else
  // int sqlLen = sql->Utf8Length() + 1;
  // char* sql2 = (char *) malloc(sqlLen);
  // sql->WriteUtf8(sql2);
  std::cout << "NOT UNICODE";

  std::string tempString = sql.Utf8Value();
  std::vector<char> sqlVec(tempString .begin(), tempString.end());
  sqlVec.push_back('\0');
  int sqlLen = tempString.length()+1;
#endif

  sqlReturnCode = SQLPrepare(
    this->m_hSTMT,
    (SQLCHAR *) &sqlVec[0], 
    sqlLen);
  std::cout << "SQLRETURN CODE FROM SQLPREPARE(): " <<sqlReturnCode << std::endl;

  if (SQL_SUCCEEDED(sqlReturnCode)) {
    return Napi::Boolean::New(env, 1);
  }
  else {
    Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, this->m_hSTMT );
    Napi::Error(env, error).ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, 0);
  }
}

/*
 * Bind
 * 
 */
class BindAsyncWorker : public Napi::AsyncWorker {

  public:
    BindAsyncWorker(bind_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      data(data), callback(callback){}

    ~BindAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::BindAsyncWorker in Execute()\n");
      DEBUG_PRINTF("ODBCStatement::UV_Bind\n");
      DEBUG_PRINTF("ODBCStatement::UV_Bind m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       data->stmt->m_hENV,
       data->stmt->m_hDBC,
       data->stmt->m_hSTMT
      );
      
      Parameter prm;
      //Was getting paramCount is protected in this context
      //So made the protected params public for now
      for (int i = 0; i < data->stmt->paramCount; i++) {
        prm = data->stmt->params[i];
        
        DEBUG_PRINTF(
          "ODBCStatement::UV_Bind - param[%i]: c_type=%i type=%i "
          "buffer_length=%i size=%i &length=%X decimals=%i value=%s\n",
          i, prm.ValueType, prm.ParameterType, prm.BufferLength, prm.ColumnSize,
          &data->stmt->params[i].StrLen_or_IndPtr, prm.DecimalDigits, prm.ParameterValuePtr
        );

        sqlReturnCode = SQLBindParameter(
          data->stmt->m_hSTMT,        //StatementHandle
          i + 1,                      //ParameterNumber
          SQL_PARAM_INPUT,            //InputOutputType
          prm.ValueType,
          prm.ParameterType,
          prm.ColumnSize,
          prm.DecimalDigits,
          prm.ParameterValuePtr,
          prm.BufferLength,
          &data->stmt->params[i].StrLen_or_IndPtr);

        if (sqlReturnCode == SQL_ERROR) {
          break;
        }
      }

      data->result = sqlReturnCode;
      
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::UV_AfterBind\n");
      Napi::Env env = Env();      
      Napi::HandleScope scope(env);
      
      //an easy reference to the statment object
      // ODBCStatement* self = data->stmt->self();

      //Check if there were errors 
      if(data->result == SQL_ERROR) {
         printf("SQLError Occured During BindAsyncWorker OnOk() Method");
        //Converted typeof callback in CallbackSQLError to Napi::Function instead of Napi::FunctionReference
        // ODBC::CallbackSQLError(
        //   env,
        //   SQL_HANDLE_STMT,
        //   data->stmt->m_hSTMT,
        //   &callback);

        Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, data->stmt->m_hSTMT );
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

      delete data;
      free(data);
    }

  private:
    bind_work_data* data;
    SQLRETURN sqlReturnCode;
    Napi::Function& callback;
};

Napi::Value ODBCStatement::Bind(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::Bind\n");
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() || !info[1].IsFunction() ) {
    Napi::Error::New(env, "Argument 1 must be an Array & Argument 2 Must be a Function").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array values = info[0].As<Napi::Array>();
  Napi::Function callback = info[1].As<Napi::Function>();
  
  // REQ_FUN_ARG(1, callback);

  // ODBCStatement* stmt = info.Holder().Unwrap<ODBCStatement>();
    
  bind_work_data* data = 
    (bind_work_data *) calloc(1, sizeof(bind_work_data));

  //if we previously had parameters, then be sure to free them
  //before allocating more
  if (this->paramCount) {
    int count = this->paramCount;
    this->paramCount = 0;
    
    Parameter prm;
    
    //free parameter memory
    for (int i = 0; i < count; i++) {
      if (prm = this->params[i], prm.ParameterValuePtr != NULL) {
        switch (prm.ValueType) {
          case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break;
          case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
          case SQL_C_SBIGINT: delete (int64_t *)prm.ParameterValuePtr; break;
          case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
          case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
        }
      }
    }

    free(this->params);
  }
  
  data->stmt = this;
  
  DEBUG_PRINTF("ODBCStatement::Bind\n");
  DEBUG_PRINTF("ODBCStatement::Bind m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
   data->stmt->m_hENV,
   data->stmt->m_hDBC,
   data->stmt->m_hSTMT
  );
  
  data->stmt->params = ODBC::GetParametersFromArray(
    &values, 
    &data->stmt->paramCount);

  BindAsyncWorker *worker = new BindAsyncWorker(data, callback);
  worker->Queue();

  return env.Undefined();
}


/*
 * BindSync
 * 
 */

Napi::Value ODBCStatement::BindSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::BindSync\n");
  Napi::Env env = info.Env();  
  Napi::HandleScope scope(env);

  if ( !info[0].IsArray() ) {
    Napi::TypeError::New(env, "Argument 1 must be an Array").ThrowAsJavaScriptException();
    return env.Null();
  }

  // ODBCStatement* stmt = info.Holder().Unwrap<ODBCStatement>();  
  DEBUG_PRINTF("ODBCStatement::BindSync m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
   this->m_hENV,
   this->m_hDBC,
   this->m_hSTMT
  );
  
  // //if we previously had parameters, then be sure to free them
  // //before allocating more
  if (this->paramCount) {
    int count = this->paramCount;
    this->paramCount = 0;
    
    Parameter prm;
    
    //free parameter memory
    for (int i = 0; i < count; i++) {
      if (prm = this->params[i], prm.ParameterValuePtr != NULL) {
        switch (prm.ValueType) {
          case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break;
          case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
          case SQL_C_SBIGINT: delete (int64_t *)prm.ParameterValuePtr; break;
          case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
          case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
        }
      }
    }

    free(this->params);
  }

  Napi::Array values = info[0].As<Napi::Array>();
  this->params = ODBC::GetParametersFromArray(
    &values, 
    &this->paramCount);
  
  SQLRETURN sqlReturnCode;
  Parameter prm;
  
  for (int i = 0; i < this->paramCount; i++) {
    prm = this->params[i];
    
    DEBUG_PRINTF(
      "ODBCStatement::BindSync - param[%i]: c_type=%i type=%i "
      "buffer_length=%i size=%i &length=%X decimals=%i value=%s\n",
      i, prm.ValueType, prm.ParameterType, prm.BufferLength, prm.ColumnSize, 
      &this->params[i].StrLen_or_IndPtr, prm.DecimalDigits, prm.ParameterValuePtr
    );

    sqlReturnCode = SQLBindParameter(
      this->m_hSTMT,                               //SQLHSTMT StatementHandle
      i + 1,                                      //SQLUSMALLINT ParameterNumber
      SQL_PARAM_INPUT,                           //SQLSMALLINT InputOutputType
      prm.ValueType,                            //SQLSMALLINT ValueType
      prm.ParameterType,                       //SQLSMALLINT ParameterType
      prm.ColumnSize,                         //SQLULEN ColumnSize
      prm.DecimalDigits,                     //SQLSMALLINT DecimalDigits
      prm.ParameterValuePtr,                //SQLPOINTER ParameterValuePtr
      prm.BufferLength,                    //SQLLEN BufferLength
      &this->params[i].StrLen_or_IndPtr); //SQLLEN * StrLen_orIndPtr

    if (sqlReturnCode == SQL_ERROR) {
      DEBUG_PRINTF("SQL ERROR in ODBC_Statement::BindSync for-loop\n");
      break;
    }
  }

  if (SQL_SUCCEEDED(sqlReturnCode)) {
    DEBUG_PRINTF("Bind has succeeded!\n");
    return Napi::Boolean::New(env, 1);
  }
  else {
    Napi::Value objError = ODBC::GetSQLError(
      env,
      SQL_HANDLE_STMT,
      this->m_hSTMT,
      (char *) "[node-odbc] Error in ODBCStatement::BindSync"
    );

    Napi::Error(env, objError).ThrowAsJavaScriptException();    
    return Napi::Boolean::New(env, 0);
  }
}

/*
 * CloseSync
 */

Napi::Value ODBCStatement::CloseSync(const Napi::CallbackInfo& info) {
  DEBUG_PRINTF("ODBCStatement::CloseSync\n");
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  
  // ODBCStatement* stmt = info.Holder().Unwrap<ODBCStatement>();
  
  DEBUG_PRINTF("ODBCStatement::CloseSync closeOption=%i\n", 
               closeOption);
  
  if (closeOption == SQL_DESTROY) {
    this->Free();
  }
  else {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeStmt(this->m_hSTMT, closeOption);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }

  return Napi::Boolean::New(env, 1);
}
