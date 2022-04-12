declare namespace odbc {

  class ColumnDefinition {
    name: string;
    dataType: number;
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
    code:    number;
    state:   string;
  }

  class NodeOdbcError extends Error {
    odbcErrors: Array<OdbcError>;
  }

  class Statement {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    prepare(sql:string, callback: (error: NodeOdbcError) => undefined): undefined;

    bind(parameters: Array<number|string>, callback: (error: NodeOdbcError) => undefined): undefined;

    execute<T>(callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;

    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    prepare(sql:string): Promise<void>;
  
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
    shrink?: boolean;
  }

  interface QueryOptions {
    cursor?: boolean|string;
    fetchSize?: number;
    timeout?: number;
    initialBufferSize?: number; 
  }

  class Connection {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    query<T>(sql: string, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    callProcedure(catalog: string, schema: string, name: string, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;
    callProcedure(catalog: string, schema: string, name: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;

    createStatement(callback: (error: NodeOdbcError, statement: Statement) => undefined): undefined;

    tables(catalog: string, schema: string, table: string, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;

    columns(catalog: string, schema: string, table: string, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;

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
    query<T>(sql: string, options: QueryOptions): Promise<Result<T>>;
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions): Promise<Result<T>>;

    callProcedure(catalog: string, schema: string, name: string, parameters?: Array<number|string>): Promise<Result<unknown>>;

    createStatement(): Promise<Statement>;

    tables(catalog: string, schema: string, table: string, type: string): Promise<Result<unknown>>;

    columns(catalog: string, schema: string, table: string, column: string): Promise<Result<unknown>>;

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
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions, callback: (error: NodeOdbcError, result: Result<T>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;


    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    connect(): Promise<Connection>;

    query<T>(sql: string): Promise<Result<T>>;
    query<T>(sql: string, parameters: Array<number|string>): Promise<Result<T>>;
    query<T>(sql: string, options: QueryOptions): Promise<Result<T>>;
    query<T>(sql: string, parameters: Array<number|string>, options: QueryOptions): Promise<Result<T>>;

    close(): Promise<void>;
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