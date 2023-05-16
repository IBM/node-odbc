declare namespace odbc {

  class ColumnDefinition {
    name: string;
    dataType: number;
    dataTypeName: string;
    columnSize: number;
    decimalDigits: number;
    nullable: boolean;
  }

  class Result<T> extends Array<T> {
    count: number;
    columns: Array<ColumnDefinition>;
    statement: string;
    parameters: Array<number|string>;
    return: number;
  }

  class OdbcError {
    message: string;
    code: number;
    state: string;
  }

  class NodeOdbcError extends Error {
    odbcErrors: Array<OdbcError>;
  }

  class Statement {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    prepare(sql: string, callback: (error: NodeOdbcError) => undefined): undefined;

    bind(parameters: Array<number|string>, callback: (error: NodeOdbcError) => undefined): undefined;

    execute<T>(callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;

    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    prepare(sql: string): Promise<void>;

    bind(parameters: Array<number|string>): Promise<void>;

    execute<T>(): Promise<Result<T>>;

    close(): Promise<void>;
  }

  interface ConnectionParameters {
    connectionString: string;
    connectionTimeout?: number;
    loginTimeout?: number;
  }
  interface PoolParameters {
    connectionString: string;
    connectionTimeout?: number;
    loginTimeout?: number;
    initialSize?: number;
    incrementSize?: number;
    maxSize?: number;
    reuseConnections?: boolean;
    shrink?: boolean;
  }

  interface QueryOptions {
    cursor?: boolean|string;
    fetchSize?: number;
    timeout?: number;
    initialBufferSize?: number;
  }

  interface CursorQueryOptions extends QueryOptions {
    cursor: boolean|string
  }

  class Connection {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    query<T>(sql: string, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Result<T> | Cursor) => undefined): undefined;
    query<T, O extends QueryOptions>(sql: string, options: O, callback: (error: NodeOdbcError, result: O extends CursorQueryOptions ? Cursor : Result<T>) => undefined): undefined;
    query<T, O extends QueryOptions>(sql: string, parameters: Array<number|string>, options: O, callback: (error: NodeOdbcError, result: O extends CursorQueryOptions ? Cursor : Result<T>) => undefined): undefined;

    callProcedure<T>(catalog: string|null, schema: string|null, name: string, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    callProcedure<T>(catalog: string|null, schema: string|null, name: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    createStatement(callback: (error: NodeOdbcError, statement: Statement) => undefined): undefined;

    primaryKeys<T>(catalog: string|null, schema: string|null, table: string|null, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    foreignKeys<T>(pkCatalog: string|null, pkSchema: string|null, pkTable: string|null, fkCatalog: string|null, fkSchema: string|null, fkTable: string|null, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    tables<T>(catalog: string|null, schema: string|null, table: string|null, type: string|null, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    columns<T>(catalog: string|null, schema: string|null, table: string|null, column: string|null, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    setIsolationLevel(level: number, callback: (error: NodeOdbcError) => undefined): undefined;

    beginTransaction(callback: (error: NodeOdbcError) => undefined): undefined;

    commit(callback: (error: NodeOdbcError) => undefined): undefined;

    rollback(callback: (error: NodeOdbcError) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;

    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    query<T>(sql: string): Promise<Result<T>>;
    query<T>(sql: string, parameters: Array<number|string>): Promise<Result<T>>;
    query<T, O extends QueryOptions>(sql: string, options: O): O extends CursorQueryOptions ? Promise<Cursor> : Promise<Result<T>>;
    query<T, O extends QueryOptions>(sql: string, parameters: Array<number|string>, options: O): O extends CursorQueryOptions ? Promise<Cursor> : Promise<Result<T>>;

    callProcedure<T>(catalog: string|null, schema: string|null, name: string, parameters?: Array<number|string>): Promise<Result<T>>;

    createStatement(): Promise<Statement>;

    primaryKeys<T>(catalog: string|null, schema: string|null, table: string|null):  Promise<Result<T>>;

    foreignKeys<T>(pkCatalog: string|null, pkSchema: string|null, pkTable: string|null, fkCatalog: string|null, fkSchema: string|null, fkTable: string|null):  Promise<Result<T>>;

    tables<T>(catalog: string|null, schema: string|null, table: string|null, type: string|null): Promise<Result<T>>;

    columns<T>(catalog: string|null, schema: string|null, table: string|null, column: string|null): Promise<Result<T>>;

    setIsolationLevel(level: number): Promise<void>;

    beginTransaction(): Promise<void>;

    commit(): Promise<void>;

    rollback(): Promise<void>;

    close(): Promise<void>;
  }

  class Pool {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    connect(callback: (error: NodeOdbcError, connection: Connection) => undefined): undefined;

    query<T>(sql: string, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Result<T> | Cursor) => undefined): undefined;
    query<T, O extends QueryOptions>(sql: string, options: O, callback: (error: NodeOdbcError, result: O extends CursorQueryOptions ? Cursor : Result<T>) => undefined): undefined;
    query<T, O extends QueryOptions>(sql: string, parameters: Array<number|string>, options: O, callback: (error: NodeOdbcError, result: O extends CursorQueryOptions ? Cursor : Result<T>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;


    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    connect(): Promise<Connection>;

    query<T>(sql: string): Promise<Result<T>>;
    query<T>(sql: string, parameters: Array<number|string>): Promise<Result<T>>;
    query<T, O extends QueryOptions>(sql: string, options: O): O extends CursorQueryOptions ? Promise<Cursor> : Promise<Result<T>>;
    query<T, O extends QueryOptions>(sql: string, parameters: Array<number|string>, options: O): O extends CursorQueryOptions ? Promise<Cursor> : Promise<Result<T>>;

    close(): Promise<void>;
  }

  class Cursor {
    noData: boolean

    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    fetch<T>(): Promise<Result<T>>

    close(): Promise<void>

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    fetch<T>(callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined

    close(callback: (error: NodeOdbcError) => undefined): undefined
  }

  function connect(connectionString: string, callback: (error: NodeOdbcError, connection: Connection) => undefined): undefined;
  function connect(connectionObject: ConnectionParameters, callback: (error: NodeOdbcError, connection: Connection) => undefined): undefined;

  function connect(connectionString: string): Promise<Connection>;
  function connect(connectionObject: ConnectionParameters): Promise<Connection>;


  function pool(connectionString: string, callback: (error: NodeOdbcError, pool: Pool) => undefined): undefined;
  function pool(connectionObject: PoolParameters, callback: (error: NodeOdbcError, pool: Pool) => undefined): undefined;

  function pool(connectionString: string): Promise<Pool>;
  function pool(connectionObject: PoolParameters): Promise<Pool>;
}

export = odbc;
