export as namespace odbc;

export class ODBCError {
  message: string;
  code:    number;
  state:   string;
}

export class Error {
  message:   string;
  odbcError: ODBCError[];
}

export class Statement {

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

export class Connection {

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


export class Pool {

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

export function connect(connectionString: string, callback: (error: Error, connection: Connection) => undefined): undefined;
export function connect(connectionObject: object, callback: (error: Error, connection: Connection) => undefined): undefined;

export function connect(connectionString: string): Promise<Connection>;
export function connect(connectionObject: object): Promise<Connection>;


export function connect(connectionString: string, callback: (error: Error, pool: Pool) => undefined): undefined;
export function connect(connectionObject: object, callback: (error: Error, pool: Pool) => undefined): undefined;

export function connect(connectionString: string): Promise<Pool>;
export function connect(connectionObject: object): Promise<Pool>;