# Changelog
All notable changes to this project will be documented in this file.

## [2.4.7] - 2023-01-26
### Fixed
- Fixed static cursor declaration causeing performance degredatio
- Fixed a memory leak with certain long binary and character types

## [2.4.6] - 2022-09-22
### Fixed
- Fixed TypeScript definition error preventing compilation

## [2.4.5] - 2022-09-12
### Added
- `primaryKeys` instance function on `Connection` to call ODBC SQLPrimaryKeys function
- `foreignKeys` instance function on `Connection` to call ODBC SQLForeignKeys function
- Binaries added for all supported N-API versions for all GitHub Actions runners 

### Fixed
- Fixed VARCHAR(MAX) fields creating 0-sized buffers (MSSQL)
- Fixed various TypeScript type definitions

## [2.4.4] - 2022-04-26
### Fixed
- Fixed application crashing when `callProcedure` was given the wrong procedure name or number of parameters
- Fixed TypeScript definition for Connection's `tables` function

### Added
- `binding.gyp` path for OS400 (IBM i)

## [2.4.3] - 2022-03-31
### Fixed
- Updated dependencies for security fixes
- Fixed generation of `callProcedure` sql string when `UNICODE` is defined

## [2.4.2] - 2022-02-07
### Added
- `binding.gyp` build instructions for MacOS 
- `Statement`'s `.execute` function can now return a `Cursor` when the correct queryOption is passed

### Fixed
- `Statement` and `Cursor` should now better handle freeing memory
- `Connection`'s `.callProcedure` should now work on Windows with `UNICODE` defined
- Fixed up TypeScript definitions


## [2.4.1] - 2021-10-19
### Added
- Simple binding path allows driver's that don't implement block fetch and column-wise binding to still be able to fetch results
- Allow pool.query() to use query options

### Fixed
- Update timeout definitions in README.md
- Fixed multiple memory leaks
- Fixed multiple segfaults


## [2.4.0] - 2021-07-06
### Added
- NEW Cursor class that is returned when new `cursor` query option is set to `true`. Cursor allows users to fetch partial result sets through calling `fetch`
- NEW `timeout` query property allows users to define the number of seconds to wait for execution before returning to the application
- NEW `initialBufferSize` query property property allows users to define the size of a buffer for SQL_LONG data types before resizing
- Tests for multiple DBMSs added
- Support for FreeBSD build

### Fixed
- Connection generation in pools is now more efficient and doesn't block queries
- Retrieving binary data 
- Improved TypeScript definitions
- BIGINT fields are now bound by default correctly
- Fixed multiple memory leaks
- Fixed multiple uncaught errors
- Dozens of minor fixes (see GitHub issues)

### Changed
- SQL_LONG* fields now use SQLGetData ODBC function, greatly increasing performance
- Connection options can now be passed through to pool connections
- Debugging no longer done through `DEBUG` define, but through existing connection manager facilities
- Updated dependencies

## [2.3.6] - 2021-02-19
- Emergency version to fix a push made to `npm` in error.

## [2.3.5] - 2020-09-14
### Fixed
- Fixed multiple connections being created after `pool.query()` is called.

## [2.3.4] - 2020-08-09
### Fixed
- Fixed bug when UNICODE is defined where statement in result object and column name properties wouldn't encode correctly
- Update package-lock.json with vulnerability fixes

## [2.3.3] - 2020-07-31
### Fixed
- Fixed bug when UNICODE is defined where error message, error state, and column names wouldn't encode correctly

## [2.3.2] - 2020-07-28
### Fixed
- Fixed bug with REAL, DECIMAL, and NUMERIC fields ocassionaly returning incorrect results

### Changed 
- Windows binaries are now built with `UNICODE` defined by default (like in 1.x)

### Added
- `columns` array on result set now includes `columnSize`, `decimalDigits`, and `nullable` data from `SQLDescribeCol`

## [2.3.1] - 2020-07-24
### Fixed
- Fixed bug with `callProcedure` on big-endian systems

## [2.3.0] - 2020-05-21
### Added
- `node-pre-gyp` added to dependencies to download pre-built binaries
- TypeScript definitions added for all functions and objects

### Changed
- Refactored how column values are bound (now bound to correct C type)

### Fixed
- Promises no longer overwrite `odbcErrors` object on Errors
- Parameters can now correctly be classified as integers or doubles
- Multi-byte UTF-8 strings are now returned correctly

## [2.2.2] - 2019-10-27
### Fixed
- Fixed SQL_DECIMAL, SQL_REAL, and SQL_NUMERIC losing precision

## [2.2.1] - 2019-09-13
### Fixed
- pool.query() now closes the connections after query
- Closing queries rapidly no longer causes segfaults

## [2.2.0] - 2019-08-28
### Added
- Added `CHANGELONG.md`
- Added Connection function `.setIsolationLevel(level, callback?)`

### Changed
- Refactored how parameters are stored and returned

### Fixed
- SQL_NO_TOTAL should no longer return error on queries
- connection.close() is now more stable

## [2.1.3] - 2019-08-16
### Changed
- Created much more rebust DEBUG messages

## [2.1.2] - 2019-08-08
### Added
- Added support for `SQL_TINYINT`

## [2.1.1] - 2019-08-05
### Changed
- Upgraded `lodash` from version 4.17.11 to 4.17.15

## [2.1.0] - 2019-08-02
### Added
- Added support for JavaScript BigInt binding to `SQL_BIGINT`

## [2.0.0-4] - 2019-06-26
### Changed
- Removed a debug message

## [2.0.0-3] - 2019-06-13
### Changed
- Fixed errors in `.callProcedure()`

## [2.0.0-2] - 2019-06-11
### Changed
- Fixed Windows compilation issues

## [2.0.0-1] - 2019-06-10
### Changed
- Fixed dependency issue with `async` package

## [2.0.0] - 2019-06-10
### Added
- **COMPLETE REWRITE OF THE PACKAGE**