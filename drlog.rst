=======
 drlog
=======

----------------------------------------------------------
Read lines from stdin and append them to auto-rotated logs
----------------------------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8

SYNOPSIS
========

``drlog [options] directory``


DESCRIPTION
===========

``drlog`` will read its standard input, distributing it as output in a set
of named ``log-YYYY-mm-dd-HH:MM:SS`` and a ``current`` file. Output is always
appended to ``current``, but when user-defined maximum file size (``-s``) or
file usage time (``-t``) it will be renamed with a timestamp in its file name,
a new ``current`` file will be opened and, if there are stored more than
a number of timestamped files (``-m``) old ones will be deleted.

The names of the files are designed to make them appear time-ordered in
output from commands like `ls(1)`. Also, the ``current`` file will appear at
the top of file listings.

If ``drlog`` receives a *TERM* signal, it will read and process data until
the next newline and then exit, leaving *stdin* at the first byte of data it
has not yet precessed.

Upon a ``HUP`` signal, ``drlog`` will close and re-open the ``current``
log file, just in case you want rotate logs using an external tool, though
using it that way is unsupported.


USAGE
=====

Command line options:

-m NUMBER   Maximum amount of maintained log files. When ``drlog`` sees
            more than *NUMBER* log files in the log *directory* it will
            remove the oldest log file.

-t TIME     Maximum number of time to use a log file. Once ``drlog`` spends
            more than *TIME* using a log file it will start writing to a new
            one. Suffixes *m* (minutes), *h* (hours), *d* (days), *w* (weeks),
            *M* (months) and *y* (years) may be used after the number. If no
            suffix is given, it is assummed that *TIME* is in seconds.

-s SIZE     Maximum size of each log file. When a log file grows over
            *SIZE* then ``drlog`` will rotate logs and open a new one.
            Suffixes *k* (kilobytes), *m* (megabytes) and *g* (gigabytes)
            may be used after the number. If no suffix is given, it is
            assumed that ``SIZE`` is in bytes.

-t          Prepend a timestamp to each line. The timestamp format
            is ``YYYY-mm-dd/HH:MM:SS``, following that of rotated log files.
            It is easy to parse and sort. And human-readable, too.


SEE ALSO
========

`multilog(8)`, `supervise(8)`, `svc(8)`, `dslog(8)`, `dlog(8)`, `dmon(8)`

