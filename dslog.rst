======
 dlog
======

------------------------------------------------
Send lines from standard input to the system log
------------------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8


SYNOPSIS
========

``dslog [options] name``


DESCRIPTION
===========

The ``dslog`` program sends lines given as standard input to the system
logger, one line at a time, with a selectable priority, facility and origin
program name.


USAGE
=====

Command line options:

-p PRIORITY, --priority PRIORITY
              Priority of messages. Refer to `syslog(3)` to see possible
              values. Just pass any valid priority without the ``LOG_``
              prefix. Case does not matter.

-f FACILITY, --facility FACILITY
              Logging facility. Refer to `syslog(3)` to see possible values.
              Just pass any valid facility without the ``LOG_`` prefix. Case
              does not matter.

-i NUMBER, --input-fd NUMBER
              Use file descriptor ``NUMBER`` to read input. By default the
              standard input descriptor (number ``0``) is used.

-c, --console
              If a message cannot be sent to the system logger, print a copy
              of it to the system console.

-h, --help    Show a summary of available options.

Albeit it can be used stan-alone, most of the time you will be running
``dslog`` under a process control tool like `dmon(8)` or `supervise(8)`.


ENVIRONMENT
===========

Additional options will be picked from the ``DSLOG_OPTIONS`` environment
variable, if defined. Any command line option can be specified this way.
Arguments read from the environment variable will be prepended to the ones
given in the command line, so they may still be overriden.


SEE ALSO
========

`dmon(8)`, `dlog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

