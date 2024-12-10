======
 denv
======

---------------------------------------
Run a command in a modified environment
---------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8

SYNOPSIS
========

``denv [options] command [command-options]``

``envdir d child``

The second form mimics the command line interface of the `envdir(8)`
tool included in DJB's daemontools package, see CAVEATS_ for the
differences.


DESCRIPTION
===========

The ``dlog`` program modifies the environment variables as specified by
the supplied ``options`` and immediately executes the given ``command``.

By default the environment will be empty. Variables from the parent process
may be incorporated using the ``--inherit-env`` and ``--inherit`` options,
and new variables added or removed using the ``--environ``, ``--direnv``
and ``--file`` options.


USAGE
=====

Command line options:

-I, --inherit-env
            Inherit all the environment variables from the parent
            process that runs ``denv``.

-i NAME, --inherit NAME
            Inherit and environment variable from the parent process
            that runs ``denv`` given its ``NAME``. This option may
            be used multiple times.

-E NAME[=VALUE], --environ NAME[=VALUE]
            Define an environment variable with a given ``NAME``, and
            assign a ``VALUE`` to it. If the value is not specified,
            an existing variable is removed.

-d PATH, --envdir PATH
            Read each file from the directory at ``PATH``, assigning
            as variables named after the files, with their values
            being the first line of contents. Trailing spaces are
            removed from the values. If a file (or its first line)
            is empty, the variable named after the file is removed.
            Only regular files are taken into account, and in
            particular symbolic links are *not* followed.

-f PATH, --file PATH
            Read lines from the file at ``PATH``, each one containing
            a ``NAME=[VALUE]`` environment variable assignment. Both
            leading and trailing whitespace are removed from the line.
            Each ``NAME`` must be a valid variable name. Empty lines
            and lines beginning with the comment character ``#`` are
            ignored. If a ``VALUE`` is omitted, then the variable is
            removed from the environment. The file format is loosely
            based on `environment.d(5)`.

The ``denv`` program is often used in concert with `dmon(8)` to setup
the environment under which supervised programs run.


EXIT CODES
==========

``denv`` exits with code **111** if it has trouble preparing the environment
before executing the chained child process, or if the child process cannot
be executed. Otherwise its exit code is that of the child process.


SEE ALSO
========

`dmon(8)`, `envdir(8)`, `environment.d(5)`

http://cr.yp.to/daemontools.html


CAVEATS
=======

The original `envdir(8)` program included in the daemontools suite converts
null characters (``\0``) into new lines (``\n``). This behaviour is
deliberately left unimplemented in ``denv``, even when it runs in ``envdir``
mode.
