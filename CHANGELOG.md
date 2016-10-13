# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/) 
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
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

[Unreleased]: https://github.com/aperezdc/dmon/compare/v0.4.2...HEAD
[v0.4.2]: https://github.com/aperezdc/dmon/compare/v0.4.1...v0.4.2
