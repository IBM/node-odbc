/*
  Copyright (c) 2019, 2021 IBM
  Copyright (c) 2013, Dan VerWeire<dverweire@gmail.com>
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

#ifndef _SRC_ODBC_CONNECTION_H
#define _SRC_ODBC_CONNECTION_H

#include <napi.h>

class ODBCConnection : public Napi::ObjectWrap<ODBCConnection> {

  // ODBCConnection AsyncWorker classes
  friend class CloseAsyncWorker;
  friend class CreateStatementAsyncWorker;
  friend class QueryAsyncWorker;
  friend class BeginTransactionAsyncWorker;
  friend class EndTransactionAsyncWorker;
  friend class PrimaryKeysAsyncWorker;
  friend class ForeignKeysAsyncWorker;
  friend class TablesAsyncWorker;
  friend class ColumnsAsyncWorker;
  friend class GetInfoAsyncWorker;
  friend class GetAttributeAsyncWorker;
  friend class CallProcedureAsyncWorker;
  friend class SetIsolationLevelAsyncWorker;
  friend class FetchAsyncWorker;

  friend class ODBCStatement;
  friend class ODBCCursor;
  // ODBCStatement AsyncWorker classes
  friend class PrepareAsyncWorker;
  friend class BindAsyncWorker;
  friend class ExecuteAsyncWorker;
  friend class CloseStatementAsyncWorker;

  public:

  static Napi::FunctionReference constructor;
  static Napi::Object Init(Napi::Env env, Napi::Object exports);

  ODBCConnection(const Napi::CallbackInfo& info);
  ~ODBCConnection();

  private:

  SQLRETURN Free();

  // Functions exposed to the Node.js environment
  Napi::Value Close(const Napi::CallbackInfo& info);
  Napi::Value CreateStatement(const Napi::CallbackInfo& info);
  Napi::Value Query(const Napi::CallbackInfo& info);
  Napi::Value CallProcedure(const Napi::CallbackInfo& info);

  Napi::Value BeginTransaction(const Napi::CallbackInfo& info);
  Napi::Value Commit(const Napi::CallbackInfo &info);
  Napi::Value Rollback(const Napi::CallbackInfo &rollback);

  Napi::Value GetUsername(const Napi::CallbackInfo &info);

  Napi::Value Columns(const Napi::CallbackInfo& info);
  Napi::Value Tables(const Napi::CallbackInfo& info);
  Napi::Value PrimaryKeys(const Napi::CallbackInfo& info);
  Napi::Value ForeignKeys(const Napi::CallbackInfo& info);

  Napi::Value GetConnAttr(const Napi::CallbackInfo& info);
  Napi::Value SetConnAttr(const Napi::CallbackInfo& info);

  Napi::Value SetIsolationLevel(const Napi::CallbackInfo &info);

  // Property Getter/Setterss
  Napi::Value ConnectedGetter(const Napi::CallbackInfo& info);
  Napi::Value ConnectionTimeoutGetter(const Napi::CallbackInfo& info);
  Napi::Value LoginTimeoutGetter(const Napi::CallbackInfo& info);
  Napi::Value AutocommitGetter(const Napi::CallbackInfo& info);

  Napi::Value GetInfo(const Napi::Env env, const SQLUSMALLINT option);

  void ParametersToArray(Napi::Reference<Napi::Array> *napiParameters, StatementData *data, unsigned char *overwriteParameters);

  bool isConnected;
  bool autocommit;

  int numStatements;

  SQLHENV hENV;
  SQLHDBC hDBC;

  ConnectionOptions connectionOptions;

  GetInfoResults    getInfoResults;
};

Napi::Array process_data_for_napi(Napi::Env env, StatementData *data, Napi::Array napiParameters);
SQLRETURN bind_buffers(StatementData *data);
SQLRETURN prepare_for_fetch(StatementData *data);
SQLRETURN fetch_and_store(StatementData *data, bool set_position, bool *alloc_error);
SQLRETURN fetch_all_and_store(StatementData *data, bool set_position, bool *alloc_error);
SQLRETURN set_fetch_size(StatementData *data, SQLULEN fetch_size);
Napi::Value parse_query_options(Napi::Env env, Napi::Value options_value, QueryOptions *query_options);
#endif
