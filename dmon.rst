======
 dmon
======

-------------------------------
Daemonize and monitor processes
-------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8


SYNOPSIS
========

``dmon [options] cmd [cmdoptions] [-- logcmd [logcmdoptions]]``


DESCRIPTION
===========

The ``dmon`` program will launch a program and re-launch it whenever it
dies. Optionally, the standard output streams of the programs may be piped
into a second program (named *log command*), which will receive the output
of the program in its standard input stream. The log command will be also
monitored and re-launched when it dies.


USAGE
=====

Command line options:

-p PATH       Write the PID of the master ``dmon`` process to a file in the
              specified *PATH*. You can signal the process to interact with
              it. (See SIGNALS_ below.)

-n            Do not daemonize: ``dmon`` will keep working in foreground,
              without detaching and without closing its standard input and
              output streams. This is useful for debugging and, to a limited
              extent, to run interactive programs.

-1            Run command only once: if the command exits with a success
              status (i.e. exit code is zero), then ``dmon`` will exit and
              stop the logging process. If the program dies due to a signal
              or with a non-zero exit status, it is respawned. This option
              tends to be used in conjunction with ``-n``.

-e            Redirect both the standard error and standard output streams
              to the log command. If not specified, only the standard output
              is redirected.

-s            Forward signals *CONT*, *ALRM*, *QUIT*, *USR1*, *USR2* and
              *HUP* to the monitored command when ``dmon`` receives them.

-S            Forward signals *CONT*, *ALRM*, *QUIT*, *USR1*, *USR2* and
              *HUP* to the log command when ``dmon`` receives them.

-h, -?        Show a summary of available options.

Usual log commands include `dlog(8)` and `dsyslog(8)`, which are part of the
``dmon`` suite. Other log commands like `rotlog(8)` or `multilog(8)` may be
used as long as they consume data from standard input and do not detach
themsemlves from the controlling process.


SIGNALS
=======

Signals may be used to interact with the monitored processes and ``dmon``
itself.

The ``TERM`` and ``INT`` signals are catched by ``dmon``, and they will
make it shut down gracefully: both the main command and the log command
will receive a ``TERM`` signal followed by a ``CONT`` and they will be
waited for.

When at least one of ``-s`` or ``-S`` are used, the ``CONT``, ``ALRM``,
``QUIT``, ``USR1``, ``USR2`` and ``HUP`` signals are forwarded to the
managed processes. By default, if none of the options are used, those
signals are ignored.


EXAMPLE
=======

The following command will supervise a shell which prints a string each
fifth second, and the output is logged to a file with timestamps::

  dmon -n sh -c 'while echo "Hello World" ; do sleep 5 ; done' \
    -- dlog logfile

In order to turn the previous example into a daemon, we only need to
remove the ``-n``. I may be convenient to specify a PID file path::

  dmon -p example.pid \
    sh -c 'while echo "Hello dmon" ; do sleep 5 ; done' \
    -- dlog logfile

If you have a PID file, terminating the daemon is an easy task::

  kill $(cat example.pid)


SEE ALSO
========

`dlog(8)`, `dsyslog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

