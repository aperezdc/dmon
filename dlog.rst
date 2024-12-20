======
 dlog
======

---------------------------------------------
Send lines from standard input to a log file
---------------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8


SYNOPSIS
========

``dlog [options] [logfile]``


DESCRIPTION
===========

The ``dlog`` program sends lines given as standard input to a log file,
one line at a time, optionally adding a timestamp in front of each line.
If the log file is not specified, then lines are printed back to standard
output. The latter may be useful to add timestamps in shell pipelines.


USAGE
=====

Command line options:

-p TEXT, --prefix TEXT
              Insert the given text as prefix for each logged message. If
              adding timestamps is enabled, the text is inserted *after*
              the timestamp, but still before the logged text.

-i NUMBER, --input-fd NUMBER
              Use file descriptor ``NUMBER`` to read input. By default the
              standard input descriptor (number ``0``) is used.

-b, --buffered
              Buffered operation. If enabled, calls to `fsync(2)` will be
              avoided. This improves performance, but may cause messages to
              be lost.

-t, --timestamp
              Prepend a timestamp to each saved line. By default
              timestamps are disabled. Timestamp format is
              ``YYYY-mm-dd/HH:MM:SS``.

-e, --skip-empty
              Ignore empty input lines. An empty line is one that does not
              contain any characters; a line which contains whitespace is
              **not** considered empty.

-h, --help    Show a summary of available options.

Albeit it can be used stand-alone, most of the time you will be running
``dlog`` under a process control tool like `dmon(8)` or `supervise(8)`.


ENVIRONMENT
===========

Additional options will be picked from the ``DLOG_OPTIONS`` environment
variable, if defined. Any command line option can be specified this way.
Arguments read from the environment variable will be prepended to the ones
given in the command line, so they may still be overriden.


SEE ALSO
========

`dmon(8)`, `dslog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

