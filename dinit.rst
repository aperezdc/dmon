=======
 dinit
=======

------------------------------------------
System services control and initialization
------------------------------------------

:Author: Adrian Perez <aperez@igalia.com>
:Manual section: 8


SYNOPSIS
========

``dinit [ -r ]``


DESCRIPTION
===========

The ``dnit`` program is the parent of all processes. Its primary role is to
launch and monitor services needed for system operation.


USAGE
=====

Command line options:

-r, --rescue  Initialize system in rescue mode: ``dinit`` will boot directly
              into a single-user shell, doing the absolutely minimum needed
              to make it run. If the shell is exited with a non-error (zero)
              return code, normal system intialization will continue;
              otherwise the system will be halted..

-h, -?        Show a summary of available options.

Other command line options supported by legacy `init(8)` will be ignored for
compatibility.


ENVIRONMENT
===========


SEE ALSO
========

`dmon(8)`, `supervise(8)`

http://cr.yp.to/daemontools.html

