declare namespace odbc {
  class Statement {

    //////////////////////////////////////////////////////////////////////////////
    //   Callbacks   /////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    prepare(sql:string, callback: (error: Error) => undefined): undefined;

    bind(parameters: Array<number|string>, callback: (error: Error) => undefined): undefined;

    execute(callback: (error: Error, result: Array<any>) => undefined): undefined;

    close(callback: (error: Error) => undefined): undefined;

    //////////////////////////////////////////////////////////////////////////////
    //   Promises   //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    prepare(sql:string): Promise<void>;
  
    bind(parameters: Array<number|string>): Promise<void>;
  
    execute(): Promise<Array<any>>;
  
    close(): Promise<void>;
  }

  class Connection {

    //////////////////////////////////////////////////////////////////////////////
    //   Callbacks   /////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    query(sql: string, callback: (error: Error, result: Array<any>) => undefined): undefined;
    query(sql: string, parameters: Array<number|string>, callback: (error: Error, result: Array<any>) => undefined): undefined;

    callProcedure(catalog: string, schema: string, name: string, callback: (error: Error, result: Array<any>) => undefined): undefined;
    callProcedure(catalog: string, schema: string, name: string, parameters: Array<number|string>, callback: (error: Error, result: Array<any>) => undefined): undefined;

    createStatement(callback: (error: Error, statement: Statement) => undefined): undefined;

    tables(catalog: string, schema: string, table: string, callback: (error: Error, result: Array<any>) => undefined): undefined;

    columns(catalog: string, schema: string, table: string, callback: (error: Error, result: Array<any>) => undefined): undefined;

    setIsolationLevel(level: number, callback: (error: Error) => undefined): undefined;

    beginTransaction(callback: (error: Error) => undefined): undefined;

    commit(callback: (error: Error) => undefined): undefined;

    rollback(callback: (error: Error) => undefined): undefined;

    close(callback: (error: Error) => undefined): undefined;

    //////////////////////////////////////////////////////////////////////////////
    //   Promises   //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    query(sql: string, parameters?: Array<number|string>): Promise<Array<any>>;

    callProcedure(catalog: string, schema: string, name: string, parameters?: Array<number|string>): Promise<Array<any>>;

    createStatement(): Promise<Statement>;

    tables(catalog: string, schema: string, table: string): Promise<Array<any>>;

    columns(catalog: string, schema: string, table: string, column: string): Promise<Array<any>>;

    setIsolationLevel(level: number): Promise<void>;

    beginTransaction(): Promise<void>;

    commit(): Promise<void>;

    rollback(): Promise<void>;

    close(): Promise<void>;
  }

  class Pool {

    //////////////////////////////////////////////////////////////////////////////
    //   Callbacks   /////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    connect(callback: (error: Error, connection: Connection) => undefined): undefined;

    query(sql: string, callback: (error: Error, result: Array<any>) => undefined): undefined;
    query(sql: string, parameters: Array<number|string>, callback: (error: Error, result: Array<any>) => undefined): undefined;

    close(callback: (error: Error) => undefined): undefined;


    //////////////////////////////////////////////////////////////////////////////
    //   Promises   //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    connect(): Promise<Connection>;

    query(sql: string, parameters?: Array<number|string>): Promise<Array<any>>;

    close(): Promise<void>;
  }

  function connect(connectionString: string, callback: (error: Error, connection: Connection) => undefined): undefined;
  function connect(connectionObject: object, callback: (error: Error, connection: Connection) => undefined): undefined;

  function connect(connectionString: string): Promise<Connection>;
  function connect(connectionObject: object): Promise<Connection>;


  function pool(connectionString: string, callback: (error: Error, pool: Pool) => undefined): undefined;
  function pool(connectionObject: object, callback: (error: Error, pool: Pool) => undefined): undefined;

  function pool(connectionString: string): Promise<Pool>;
  function pool(connectionObject: object): Promise<Pool>;
}

export = odbc;