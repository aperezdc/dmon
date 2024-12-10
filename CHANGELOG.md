# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/) 
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

## [v0.6.0] - 2024-12-10
### Added
- New `denv` utility, inspired by daemontools' `envdir`, which may be
  used to manipulate the environment before executing a chained child
  process.

### Fixed
- Retry system calls that may return with `errno` set to `EINTR` when
  interrupted by signals to indicate they can be restarted from user space.
- Remove leftover, useless `$E` format specifier when reporting errors
  during daemonization.

### Changed
- The project is now built in C23 mode by default. In practice `-std=c2x`
  gets used to cover systems which may have slightly older compilers.

## [v0.5.1] - 2021-02-23
### Added
- The `dlog`, `dslog`, and `drlog` tools now log empty input lines. The
  `--skip-empty`/`-e` command line option has been added to disable
  logging empty lines.
- The `dmon` tool has gained a `--max-respawns`/`-m` command line option which
  can be used to specify how many times to respawn monitored processes
  before exiting. (Patch by Matt Schulte <<mschulte@wyze.com>>.)
- New `libsetunbuf.so` helper which can be used to disable buffering of
  the standard output stream on arbitrary programs via `LD_PRELOAD`.
  (Patch by Matt Schulte <<mschulte@wyze.com>>.)

### Fixed
- The exit status of monitored processes is now correctly propagated as
  exit code of `dmon` itself.
- The `dlog`, `dslog`, and `drlog` tools will no longer exit unexpectedly
  when they receive an empty input line. (Patch by Matt Schulte
  <<mschulte@wyze.com>>.)

## [v0.5.0] - 2020-08-27
### Added
- Support listing applets compiled into a multicall `dmon` binary when the
  `DMON_LIST_MULTICALL_APPLETS` environment variable is set and non-zero.

### Changed
- GNU Make is not required anymore, the included `Makefile` now works with
  the BSD variant as well, and probably others.

## [v0.4.5] - 2018-08-05
### Fixed
- Make it possible to build `dmon` with newer GCC versions.

## [v0.4.4] - 2016-10-30
### Fixed
- Correctly forward signals to the log process when using `-S`/`--log-sigs`.

## [v0.4.3] - 2016-10-13
### Added
- Allow setting the work directory with `-W`/`--work-dir` in the command line
  (or `work-dir` in a configuration file).

## [v0.4.2] - 2012-01-07
### Added
- Manual pages now also include the long command line options.
- `dlog` can now be instructed to prefix every log message with a given string,
  using the `-p`/`--prefix` command line option.
- `dlog`, `drlog`, and `dslog` have gained the `-i`/`--input-fd` command line
  option, which allows reading log messages from file descriptors different from
  the standard input.

### Fixed
- `dlog` and `drlog` now handle the `INT`, `TERM`, and `HUP` signals gracefully.

[Unreleased]: https://github.com/aperezdc/dmon/compare/v0.6.0...HEAD
[v0.6.0]: https://github.com/aperezdc/dmon/compare/v0.5.1...v0.6.0
[v0.5.1]: https://github.com/aperezdc/dmon/compare/v0.5.0...v0.5.1
[v0.5.0]: https://github.com/aperezdc/dmon/compare/v0.4.5...v0.5.0
[v0.4.5]: https://github.com/aperezdc/dmon/compare/v0.4.4...v0.4.5
[v0.4.4]: https://github.com/aperezdc/dmon/compare/v0.4.3...v0.4.4
[v0.4.3]: https://github.com/aperezdc/dmon/compare/v0.4.2...v0.4.3
[v0.4.2]: https://github.com/aperezdc/dmon/compare/v0.4.1...v0.4.2
