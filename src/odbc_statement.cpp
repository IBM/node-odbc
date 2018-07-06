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
#include "iostream"

using namespace Napi;

Napi::FunctionReference ODBCStatement::constructor;

HENV ODBCStatement::m_hENV;
HDBC ODBCStatement::m_hDBC;
HSTMT ODBCStatement::m_hSTMT;

Napi::Object ODBCStatement::Init(Napi::Env env, Napi::Object exports) {
  DEBUG_PRINTF("ODBCStatement::Init\n");
  Napi::HandleScope scope(env);

  Napi::Function constructorFunction = DefineClass(env, "ODBCStatement", {
    InstanceMethod("execute", &ODBCStatement::Execute),
    InstanceMethod("executeDirect", &ODBCStatement::ExecuteDirect),
    InstanceMethod("executeNonQuery", &ODBCStatement::ExecuteNonQuery),
    InstanceMethod("prepare", &ODBCStatement::Prepare),
    InstanceMethod("bind", &ODBCStatement::Bind),
    InstanceMethod("close", &ODBCStatement::Close)
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
  }
}

/*
 * Execute
 */
class ExecuteAsyncWorker : public Napi::AsyncWorker {
  public:
    ExecuteAsyncWorker(ODBCStatement *odbcStatementObject, execute_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
      odbcStatementObject(odbcStatementObject),
      data(data) {}

    ~ExecuteAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker void Execute()\n");
      sqlReturnCode = SQLExecute(odbcStatementObject->m_hSTMT); 
      data->result = sqlReturnCode;
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::ExecuteAsyncWorker void OnOk()\n");  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);
        
      // //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {

        Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, odbcStatementObject->m_hSTMT );
        std::vector<napi_value> errorVec;
        errorVec.push_back(error);
        errorVec.push_back(env.Null());
        Callback().Call(errorVec);

      }
      else {
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(odbcStatementObject->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcStatementObject->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(odbcStatementObject->m_hSTMT)));
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
    ODBCStatement *odbcStatementObject;
    execute_work_data* data;
    SQLRETURN sqlReturnCode;
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
  
  ExecuteAsyncWorker *worker = new ExecuteAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}


/*
 * ExecuteNonQuery
 */

class ExecuteNonQueryAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteNonQueryAsyncWorker(ODBCStatement *odbcStatementObject, execute_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
      data(data) {}

    ~ExecuteNonQueryAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteNonQueryAsyncWorker in Execute()\n");
      sqlReturnCode = SQLExecute(odbcStatementObject->m_hSTMT); 
      data->result = sqlReturnCode;

    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::AsyncWorkerExecuteNonQuery in OnOk()\n");  
      Napi::Env env = Env();      
      Napi::HandleScope scope(env);
      
      //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {
        Napi::Value error = ODBC::GetSQLError(
          env,
          SQL_HANDLE_STMT,
          odbcStatementObject->m_hSTMT,
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
        SQLRETURN sqlReturnCode = SQLRowCount(odbcStatementObject->m_hSTMT, &rowCount);
        
        if (!SQL_SUCCEEDED(sqlReturnCode)) {
          rowCount = 0;
        }

        uv_mutex_lock(&ODBC::g_odbcMutex);
        SQLFreeStmt(odbcStatementObject->m_hSTMT, SQL_CLOSE);
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
    ODBCStatement *odbcStatementObject;
    execute_work_data* data;
    SQLRETURN sqlReturnCode;
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

  ExecuteNonQueryAsyncWorker *worker = new ExecuteNonQueryAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * ExecuteDirect
 * 
 */

class ExecuteDirectAsyncWorker : public Napi::AsyncWorker {

  public:
    ExecuteDirectAsyncWorker(ODBCStatement *odbcStatementObject, execute_direct_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
      data(data) {}

    ~ExecuteDirectAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::ExecuteDirectAsynWorker in Execute()\n");

      sqlReturnCode = SQLExecDirect(
        odbcStatementObject->m_hSTMT,
        (SQLCHAR *) data->sql,
        data->sqlLen);

      data->result = sqlReturnCode;
      
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::ExecuteDirectAsyncWorker in OnOk()\n");  
      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      //First thing, let's check if the execution of the query returned any errors 

      if(data->result == SQL_ERROR) {

        Napi::Value error = ODBC::GetSQLError(env , SQL_HANDLE_STMT, odbcStatementObject->m_hSTMT );
        std::vector<napi_value> errorVec;

        errorVec.push_back(error);
        errorVec.push_back(env.Null());
        Callback().Call(errorVec);
      }
      else {
        std::vector<napi_value> resultArguments;
        resultArguments.push_back(Napi::External<SQLHENV>::New(env, &(odbcStatementObject->m_hENV)));
        resultArguments.push_back(Napi::External<HDBC>::New(env, &(odbcStatementObject->m_hDBC)));
        resultArguments.push_back(Napi::External<HSTMT>::New(env, &(odbcStatementObject->m_hSTMT)));
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
    ODBCStatement *odbcStatementObject;
    execute_direct_work_data* data;
    SQLRETURN sqlReturnCode;
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

  ExecuteDirectAsyncWorker *worker = new ExecuteDirectAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * Prepare
 * 
 */
class PrepareAsyncWorker : public Napi::AsyncWorker {

  public:
    PrepareAsyncWorker(ODBCStatement *odbcStatementObject, prepare_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
      data(data) {}

    ~PrepareAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in Execute()\n");
      
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       odbcStatementObject->m_hSTMT
      );

      sqlReturnCode = SQLPrepare(
        odbcStatementObject->m_hSTMT,
        (SQLCHAR *) data->sql, 
        data->sqlLen);

      data->result = sqlReturnCode;
      
    }

    void OnOK() {
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker in OnOk()\n");
      DEBUG_PRINTF("ODBCStatement::PrepareAsyncWorker m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       odbcStatementObject->m_hSTMT
      );

      Napi::Env env = Env();  
      Napi::HandleScope scope(env);

      //First thing, let's check if the execution of the query returned any errors 
      if(data->result == SQL_ERROR) {

        Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, odbcStatementObject->m_hSTMT );
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
    ODBCStatement *odbcStatementObject;
    prepare_work_data* data;
    SQLRETURN sqlReturnCode;
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

  PrepareAsyncWorker *worker = new PrepareAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * Bind
 * 
 */
class BindAsyncWorker : public Napi::AsyncWorker {

  public:
    BindAsyncWorker(ODBCStatement *odbcStatementObject, bind_work_data* data, Napi::Function& callback) : Napi::AsyncWorker(callback),
    odbcStatementObject(odbcStatementObject),
      data(data) {}

    ~BindAsyncWorker() {}

    void Execute() {
      DEBUG_PRINTF("ODBCStatement::BindAsyncWorker in Execute()\n");
      DEBUG_PRINTF("ODBCStatement::UV_Bind\n");
      DEBUG_PRINTF("ODBCStatement::UV_Bind m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
       odbcStatementObject->m_hENV,
       odbcStatementObject->m_hDBC,
       odbcStatementObject->m_hSTMT
      );
      
      Parameter prm;
      //Was getting paramCount is protected in this context
      //So made the protected params public for now
      for (int i = 0; i < odbcStatementObject->paramCount; i++) {
        prm = odbcStatementObject->params[i];
        
        DEBUG_PRINTF(
          "ODBCStatement::UV_Bind - param[%i]: c_type=%i type=%i "
          "buffer_length=%i size=%i &length=%X decimals=%i value=%s\n",
          i, prm.ValueType, prm.ParameterType, prm.BufferLength, prm.ColumnSize,
          &odbcStatementObject->params[i].StrLen_or_IndPtr, prm.DecimalDigits, prm.ParameterValuePtr
        );

        sqlReturnCode = SQLBindParameter(
          odbcStatementObject->m_hSTMT,        //StatementHandle
          i + 1,                      //ParameterNumber
          SQL_PARAM_INPUT,            //InputOutputType
          prm.ValueType,
          prm.ParameterType,
          prm.ColumnSize,
          prm.DecimalDigits,
          prm.ParameterValuePtr,
          prm.BufferLength,
          &odbcStatementObject->params[i].StrLen_or_IndPtr);

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
      // ODBCStatement* self = odbcStatementObject->self();

      //Check if there were errors 
      if(data->result == SQL_ERROR) {
        //Converted typeof callback in CallbackSQLError to Napi::Function instead of Napi::FunctionReference
        // ODBC::CallbackSQLError(
        //   env,
        //   SQL_HANDLE_STMT,
        //   odbcStatementObject->m_hSTMT,
        //   &callback);

        Napi::Value errorObj = ODBC::GetSQLError(env , SQL_HANDLE_STMT, odbcStatementObject->m_hSTMT );
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
    ODBCStatement *odbcStatementObject;
    bind_work_data* data;
    SQLRETURN sqlReturnCode;
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

  DEBUG_PRINTF("ODBCStatement::Bind\n");
  DEBUG_PRINTF("ODBCStatement::Bind m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
   odbcStatementObject->m_hENV,
   odbcStatementObject->m_hDBC,
   odbcStatementObject->m_hSTMT
  );
  
  this->params = ODBC::GetParametersFromArray(
    &values, 
    &this->paramCount);

  BindAsyncWorker *worker = new BindAsyncWorker(this, data, callback);
  worker->Queue();

  return env.Undefined();
}

/*
 * Close
 */

Napi::Value ODBCStatement::Close(const Napi::CallbackInfo& info) {

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