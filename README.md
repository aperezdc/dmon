# DMon - Process Monitoring With Style

[![Build Status](https://drone.io/github.com/aperezdc/dmon/status.png)](https://drone.io/github.com/aperezdc/dmon/latest)

This README contains only some random bits. For more in-depth writing, you
may want to read the articles on DMon:

  http://blogs.igalia.com/aperez/tag/dmon/

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

