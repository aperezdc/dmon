# DMon - Process Monitoring With Style

[![Build Status](https://api.cirrus-ci.com/github/aperezdc/dmon.svg)](https://cirrus-ci.com/github/aperezdc/dmon)

This README contains only some random bits. For more in-depth writing, you
may want to read the articles on DMon:

* 2010/08/27: [DMon: Process monitoring with style](https://perezdecastro.org/2010/dmon-process-monitoring-with-style.html)
* 2010/10/04: [DMon status report: evolution to 0.3.7](https://perezdecastro.org/2010/dmon-status-report-0-3-7.html)
* 2011/07/25: [DMon 0.4: New guts & new features](https://perezdecastro.org/2011/dmon-0-4-new-guts-and-new-features.html)
* 2012/01/07: [DMon 0.4.2 “Three Wise Men” released](https://perezdecastro.org/2012/dmon-0-4-2-released.html)

There are also manual pages, so please take a look at them.


## Bulding standalone binaries

By default all tools are built into a single binary which can be symlinked
with different names to switch between them (àla BusyBox). This is useful
to save space and (to some degree) system memory.

You can build all DMon tools as separate binaries passing `MULTICALL=0`
to Make:

```sh
make MULTICALL=0
```

Remember to pass the option when doing `make install` as well:

```sh
make MULTICALL=0 install
```


## Building libnofork.so

A tiny `LD_PRELOAD`-able “`libnofork.so`” library can be built by passing
`LIBNOFORK=1` to make. This library overrides the `fork(2)` and `daemon(3)`
functions from the system libraries, in such a way that the process under
effect will not be able of forking. This is interesting for running DMon
with programs that have no option to instruct them not to fork.

