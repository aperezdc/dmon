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

-i TIME       When execution of the process ends with a successful (zero)
              exit status, wait for *TIME* seconds before respawning the
              process instead of doing it immediately. This can be used to
              make ``dmon`` behave as el-cheapo `cron(8)` replacement. This
              option cannot be used along with ``-1``.

-t TIME       If the process takes longer than *TIME* seconds to complete,
              terminate it by sending the *TERM*/*CONT* signal combo. Then
              the process will be respawned again. This is useful to ensure
              that potentially locking processes which should take less than
              some known time limit do not hog the computer. Most likely,
              this flag is useful in conjunction with ``-1``, and with
              ``-n`` e.g. when using it in a `cron(8)` job.

-L NUMBER     Enable tracking the system's load average, and suspend the
              execution of the command process when the system load goes
              over *NUMBER*. To pause the process, *STOP* signal will be
              sent to it. You may want to use ``-l`` as well to specify
              under which load value the process is resumed, otherwise
              when the system load falls below *NUMBER/2* the process will
              be resumed.

-l NUMBER     When using ``-L``, the command process execution will be
              resumed when the system load falls below *NUMBER*, instead of
              using the default behavior of resuming the process when the
              load falls below half the limit specified with ``-L``.

-u UIDGID     Executes the command with the credentials of user *UID*,
              and additional group *GID* specified separated with
              semicolons. Both user and group identifiers might be given
              as strings or numerically.

-U UIDGID     Executes the **log** command with the credentials of user
              *UID*, and additional group *GID* specified separated with
              semicolons. Both user and group identifiers might be given
              as strings or numerically.

-n            Do not daemonize: ``dmon`` will keep working in foreground,
              without detaching and without closing its standard input and
              output streams. This is useful for debugging and, to a limited
              extent, to run interactive programs.

-1            Run command only once: if the command exits with a success
              status (i.e. exit code is zero), then ``dmon`` will exit and
              stop the logging process. If the program dies due to a signal
              or with a non-zero exit status, it is respawned. This option
              tends to be used in conjunction with ``-n``, and cannot be
              used with ``-i``.

-e            Redirect both the standard error and standard output streams
              to the log command. If not specified, only the standard output
              is redirected.

-s            Forward signals *CONT*, *ALRM*, *QUIT*, *USR1*, *USR2* and
              *HUP* to the monitored command when ``dmon`` receives them.

-S            Forward signals *CONT*, *ALRM*, *QUIT*, *USR1*, *USR2* and
              *HUP* to the log command when ``dmon`` receives them.

-r LIMIT      Set *LIMIT* for process execution. Limits are specified as
              ``name=value`` strings, and multiple limits may be set by
              using ``-r`` multiple times. The available set of limits
              depends on the current operating system, to get a list
              ``-r help`` can be used.

-h            Show a summary of available options.

Usual log commands include `dlog(8)` and `dslog(8)`, which are part of the
``dmon`` suite. Other log commands like `rotlog(8)` or `multilog(8)` may be
used as long as they consume data from standard input and do not detach
themsemlves from the controlling process.

As a convenience, time values passed to ``-i``, ``-t`` and values of limits
specified with ``-r`` may be given with the following suffixes:

- ``m``: Minutes, e.g. ``30m`` means "30 minutes".
- ``h``: Hours, e.g. ``4h`` means "4 hours".
- ``d``: Days, e.g. ``3d`` means "3 days".
- ``w``: Weeks, e.g. ``1w`` means "1 week".

For size values (bytes) the strings passed to ``-r`` as limits may have the
following suffixes:

- ``k``: Kilobytes.
- ``m``: Megabytes.
- ``g``: Gigabytes.


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


EXAMPLES
========

The following command will supervise a shell which prints a string each
fifth second, and the output is logged to a file with timestamps::

  dmon -n sh -c 'while echo "Hello World" ; do sleep 5 ; done' \
    -- dlog logfile

In order to turn the previous example into a daemon, we only need to
remove the ``-n``. I may be convenient to specify a PID file path::

  dmon -p example.pid \
    sh -c 'while echo "Hello dmon" ; do sleep 5 ; done' \
    -- dlog logfile

The following example launches the `cron(8)` daemon with the logging
process running as user and group ``log:wheel``::

  dmon -p /var/run/crond.pid -u log:wheel -e cron -f --
    -- dlog /var/log/cron.log

This example will run a (probably lengthy) backup process, pausing it when
the system load goes above 3.5 and resuming it when the load drops below
1.0::

  dmon -1 -n -l 1 -L 3.5 rsync -avz ~/ /backup/homedir

If you have a PID file, terminating the daemon is an easy task::

  kill $(cat example.pid)


ENVIRONMENT
===========

Additional options will be picked from the ``DMON_OPTIONS`` environment
variable, if defined. Any command line option can be specified this way.
Arguments read from the environment variable will be prepended to the ones
given in the command line, so they may still be overriden.


SEE ALSO
========

`dlog(8)`, `dslog(8)`, `rotlog(8)`, `multilog(8)`, `supervise(8)`, `cron(8)`

http://cr.yp.to/daemontools.html

