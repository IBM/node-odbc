# Changelog
All notable changes to this project will be documented in this file.

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