declare function odbc(options?: odbc.DatabaseOptions): odbc.Database;

declare namespace odbc {
    export const SQL_CLOSE: number;
    export const SQL_DROP: number;
    export const SQL_UNBIND: number;
    export const SQL_RESET_PARAMS: number;
    export const SQL_DESTROY: number;
    export const FETCH_ARRAY: number;
    export const FETCH_OBJECT: number;

    export let debug: boolean;

    export interface ConnctionInfo {
        [key: string]: string;
    }

    export interface DatabaseOptions {
        connectTimeout?: number;
        loginTimeout?: number;
    }

    export interface DescribeOptions {
        database: string;
        schema?: string;
        table?: string;
        type?: string;
        column?: string;
    }

    export interface SimpleQueue {
        push(fn: Function): void;
        next(): any;
        maybeNext(): any;
    }

    export interface ODBCTable {
        TABLE_CAT: string;
        TABLE_SCHEM: string;
        TABLE_NAME: string;
        TABLE_TYPE: string;
        REMARKS: string;
    }

    export interface ODBCColumn {
        TABLE_CAT: string;
        TABLE_SCHEM: string;
        TABLE_NAME: string;
        COLUMN_NAME: string;
        DATA_TYPE: number;
        TYPE_NAME: string;
        COLUMN_SIZE: number;
        BUFFER_LENGTH: number;
        DECIMAL_DIGITS: number;
        NUM_PREC_RADIX: number;
        NULLABLE: number;
        REMARKS: string;
        COLUMN_DEF: string;
        SQL_DATA_TYPE: number;
        SQL_DATETIME_SUB: number;
        CHAR_OCTET_LENGTH: number;
        ORDINAL_POSITION: number;
        IS_NULLABLE: string;
    }

    export interface ODBCConnection {
        connected: boolean;
        connectTimeout: number;
        loginTimeout: number;
        open(connctionString: string | ConnctionInfo, cb: (err: any, result: any) => void): void;
        openSync(connctionString: string | ConnctionInfo): void;
        close(cb: (err: any) => void): void;
        closeSync(): void;
        createStatement(cb: (err: any, stmt: ODBCStatement) => void): void;
        createStatementSync(): ODBCStatement;
        query(sql: string, cb: (err: any, rows: ResultRow[], moreResultSets: any) => void): void;
        query(sql: string, bindingParameters: any[], cb: (err: any, rows: ResultRow[], moreResultSets: any) => void): void;
        querySync(sql: string, bindingParameters?: any[]): ResultRow[];
        beginTransaction(cb: (err: any) => void): void;
        beginTransactionSync(): void;
        endTransaction(rollback: boolean, cb: (err: any) => void): void;
        endTransactionSync(rollback: boolean): void;
        tables(catalog: string | null, schema: string | null, table: string | null, type: string | null, cb: (err: any, result: ODBCResult) => void): void;
        columns(catalog: string | null, schema: string | null, table: string | null, column: string | null, cb: (err: any, result: ODBCResult) => void): void;
    }

    export interface ResultRow {
        [key: string]: any;
    }

    export interface ODBCResult {
        fetchMode: number;
        fetchAll(cb: (err: any, data: ResultRow[]) => void): void;
        fetchAllSync(): ResultRow[];
        fetch(cb: (err: any, data: ResultRow) => void): void;
        fetchSync(): ResultRow;
        closeSync(): void;
        moreResultsSync(): any;
        getColumnNamesSync(): string[];
    }

    export interface ODBCStatement {
        queue: SimpleQueue;
        execute(cb: (err: any, result: ODBCResult) => void): void;
        execute(bindingParameters: any[], cb: (err: any, result: ODBCResult) => void): void;
        executeSync(bindingParameters?: any[]): ODBCResult;
        executeDirect(sql: string, cb: (err: any, result: ODBCResult) => void): void;
        executeDirect(sql: string, bindingParameters: any[], cb: (err: any, result: ODBCResult) => void): void;
        executeDirectSync(sql: string, bindingParameters?: any[]): ODBCResult;
        executeNonQuery(cb: (err: any, result: number) => void): void;
        executeNonQuery(bindingParameters: any[], cb: (err: any, result: number) => void): void;
        executeNonQuerySync(bindingParameters?: any[]): number;
        prepare(sql: string, cb: (err: any) => void): void;
        prepareSync(sql: string): void;
        bind(bindingParameters: any[], cb: (err: any) => void): void;
        bindSync(bindingParameters: any[]): void;
        closeSync(): void;
    }

    export class Database {
        constructor(options?: DatabaseOptions);
        conn: ODBCConnection;
        queue: SimpleQueue;
        connected: boolean;
        connectTimeout: number;
        loginTimeout: number;
        SQL_CLOSE: number;
        SQL_DROP: number;
        SQL_UNBIND: number;
        SQL_RESET_PARAMS: number;
        SQL_DESTROY: number;
        FETCH_ARRAY: number;
        FETCH_OBJECT: number;
        open(connctionString: string | ConnctionInfo, cb: (err: any, result: any) => void): void;
        openSync(connctionString: string | ConnctionInfo): void;
        close(cb: (err: any) => void): void;
        closeSync(): void;
        query(sql: string, cb: (err: any, rows: ResultRow[], moreResultSets: any) => void): void;
        query(sql: string, bindingParameters: any[], cb: (err: any, rows: ResultRow[], moreResultSets: any) => void): void;
        querySync(sql: string, bindingParameters?: any[]): ResultRow[];
        queryResult(sql: string, cb: (err: any, result: ODBCResult) => void): void;
        queryResult(sql: string, bindingParameters: any[], cb: (err: any, result: ODBCResult) => void): void;
        queryResultSync(sql: string, bindingParameters?: any[]): ODBCResult;
        prepare(sql: string, cb: (err: any, statement: ODBCStatement) => void): void;
        prepareSync(sql: string): ODBCStatement;
        beginTransaction(cb: (err: any) => void): void;
        beginTransactionSync(): void;
        endTransaction(rollback: boolean, cb: (err: any) => void): void;
        endTransactionSync(rollback: boolean): void;
        commitTransaction(cb: (err: any) => void): void;
        commitTransactionSync(): void;
        rollbackTransaction(cb: (err: any) => void): void;
        rollbackTransactionSync(): void;
        tables(catalog: string | null, schema: string | null, table: string | null, type: string | null, cb: (err: any, result: ODBCTable[]) => void): void;
        columns(catalog: string | null, schema: string | null, table: string | null, column: string | null, cb: (err: any, result: ODBCColumn[]) => void): void;
        describe(options: DescribeOptions, cb: (err: any, result: (ODBCTable & ODBCColumn)[]) => void): void;
    }

    export class Pool {
        constructor(options?: DatabaseOptions);
        open(connctionString: string, cb: (err: any, db: Database) => void): void;
        close(cb: (err: any) => void): void;
    }

    export function open(connctionString: string | ConnctionInfo, cb: (err: any, result: any) => void): void;
}

export = odbc;
