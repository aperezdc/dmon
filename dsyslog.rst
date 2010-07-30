=========
 dsyslog
=========

------------------------------------------------
Send lines from standard input to the system log
------------------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8


SYNOPSIS
========

``dsyslog [options] name``


DESCRIPTION
===========

The ``dsyslog`` program sends lines given as standard input to the system
logger, one line at a time, with a selectable priority, facility and origin
program name.


USAGE
=====

Command line options:

-p PRIORITY   Priority of messages. Refer to `syslog(3)` to see possible
              values. Just pass any valid priority without the ``LOG_``
              prefix. Case does not matter.

-f FACILITY   Logging facility. Refer to `syslog(3)` to see possible values.
              Just pass any valid facility without the ``LOG_`` prefix. Case
              does not matter.

-c            If a message cannot be sent to the system logger, print a copy
              of it to the system console.

-h, -?        Show a summary of available options.

Albeit it can be used stan-alone, most of the time you will be running
``dsyslog`` under a process control tool like `dmon(8)` or `supervise(8)`.


SEE ALSO
========

`dmon(8)`, `dlog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

