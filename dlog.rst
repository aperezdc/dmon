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

``dlog [options] path``


DESCRIPTION
===========

The ``dlog`` program sends lines given as standard input to a log file,
one line at a time, optionally adding a timestamp in front of each line.


USAGE
=====

Command line options:

-b            Buffered operation. If enabled, calls to `fsync(2)` will be
              avoided. This improves performance, but may cause messages to
              be lost.

-c            Do not prepend a timestamp to each saved line. By default
              timestamps are enabled. Timestamp format is
              ``YYYY-mm-dd/HH:MM:SS``.

-h, -?        Show a summary of available options.

Albeit it can be used stan-alone, most of the time you will be running
``dlog`` under a process control tool like `dmon(8)` or `supervise(8)`.


SEE ALSO
========

`dmon(8)`, `dsyslog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

