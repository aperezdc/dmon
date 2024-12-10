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


SEE ALSO
========

`dmon(8)`, `envdir(8)`, `environment.d(5)`

http://cr.yp.to/daemontools.html

