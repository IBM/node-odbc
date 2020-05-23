declare namespace odbc {

  class ColumnDefinition {
    name: string;
    datayType: number;
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

    execute(callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;

    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    prepare(sql:string): Promise<void>;
  
    bind(parameters: Array<number|string>): Promise<void>;
  
    execute(): Promise<Result<unknown>>;
  
    close(): Promise<void>;
  }

  class Connection {

    ////////////////////////////////////////////////////////////////////////////
    //   Callbacks   ///////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    query(sql: string, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;
    query(sql: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;

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

    query(sql: string, parameters?: Array<number|string>): Promise<Result<unknown>>;

    callProcedure(catalog: string, schema: string, name: string, parameters?: Array<number|string>): Promise<Result<unknown>>;

    createStatement(): Promise<Statement>;

    tables(catalog: string, schema: string, table: string): Promise<Result<unknown>>;

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

    query(sql: string, callback: (error: NodeOdbcError, result: Result<unknown>) => undefined): undefined;
    query(sql: string, parameters: Array<number|string>, callback: (error: NodeOdbcError, result: Array<any>) => undefined): undefined;

    close(callback: (error: NodeOdbcError) => undefined): undefined;


    ////////////////////////////////////////////////////////////////////////////
    //   Promises   ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    connect(): Promise<Connection>;

    query(sql: string, parameters?: Array<number|string>): Promise<Result<unknown>>;

    close(): Promise<void>;
  }

  function connect(connectionString: string, callback: (error: NodeOdbcError, connection: Connection) => undefined): undefined;
  function connect(connectionObject: object, callback: (error: NodeOdbcError, connection: Connection) => undefined): undefined;

  function connect(connectionString: string): Promise<Connection>;
  function connect(connectionObject: object): Promise<Connection>;


  function pool(connectionString: string, callback: (error: NodeOdbcError, pool: Pool) => undefined): undefined;
  function pool(connectionObject: object, callback: (error: NodeOdbcError, pool: Pool) => undefined): undefined;

  function pool(connectionString: string): Promise<Pool>;
  function pool(connectionObject: object): Promise<Pool>;
}

export = odbc;