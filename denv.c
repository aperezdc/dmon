/*
 * Copyright (C) 2024 Adrian Perez de Castro <aperez@igalia.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __linux
#define _BSD_SOURCE
#endif

#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "deps/cflag/cflag.h"
#include "deps/clog/clog.h"
#include "deps/dbuf/dbuf.h"
#include "util.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if !(defined(MULTICALL) && MULTICALL)
# define denv_main main
#endif /* MULTICALL */

extern char **environ;

static char *argv0 = "denv";
static char **env = NULL;
static size_t env_a = 0;
static size_t env_n = 0;

static void
env_del(const char* name, size_t namelen)
{
	if (!env)
		return;

	size_t i;
	for (i = 0; env[i]; i++) {
		const char *equalsign = strchr(env[i], '=');
		assert(equalsign);

		size_t envlen = equalsign - env[i];
		if (envlen != namelen)
			continue;

		if (memcmp(env[i], name, namelen) == 0) {
			for (; env[i]; i++) {
				env[i] = env[i + 1];
			}
			assert(env_n > 0);
			env_n--;
			return;
		}
	}
}

static void
env_add(char *entry)
{
	assert(entry);

	const char *equalsign = strchr(entry, '=');
	assert(equalsign);

	if (env_n + 1 >= env_a) {
		env_a = env_a * 2 + 5;
		if (env) {
			env = reallocarray(env, env_a, sizeof(char*));
		} else {
			env = calloc(env_a, sizeof(char*));
		}
		if (!env)
			fatal("%s: Cannot allocate memory.\n", argv0);
	}
	assert(env_a > (env_n + 1));

	env_del(entry, equalsign - entry);
	env[env_n++] = entry;
	env[env_n] = NULL;
}

static void
env_inherit_all(void)
{
	for (size_t i = 0; environ[i]; i++) {
		assert(strchr(environ[i], '='));
		env_add(environ[i]);
	}
}

static enum cflag_status
opt_inherit_env(const struct cflag *spec, const char *arg)
{
	(void) arg;
	if (!spec)
		return CFLAG_OK;

	env_inherit_all();
	return CFLAG_OK;
}

static enum cflag_status
opt_inherit(const struct cflag *spec, const char *arg)
{
	if (!spec)
		return CFLAG_NEEDS_ARG;

	for (size_t i = 0; environ[i]; i++) {
		const char *equalsign = strchr(environ[i], '=');
		assert(equalsign);
		if (strncmp(arg, environ[i], equalsign - environ[i]) == 0) {
			env_add(environ[i]);
			return CFLAG_OK;
		}
	}

	clog_debug("Cannot inherit undefined variable '%s'", arg);
	return CFLAG_OK;
}

static enum cflag_status
opt_environ(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    const char *equalsign;
    if ((equalsign = strchr(arg, '=')) == NULL) {
        env_del(arg, strlen(arg));
    } else {
		env_add((char*) arg);
    }
    return CFLAG_OK;
}

#ifndef AT_NO_AUTOMOUNT
#define AT_NO_AUTOMOUNT 0
#endif /* !AT_NO_AUTOMOUNT */

static inline bool
is_trim_char(int c)
{
	switch (c) {
		case '\r':
		case '\n':
		case '\v':
		case '\t':
		case '\f':
		case ' ':
			return true;
		default:
			return false;
	}
}

static void
env_envdir(const char *path)
{
	assert(path);

	DIR *d = opendir(path);
	if (!d)
		fatal("%s: Cannot open directory '%s': %s.\n", argv0, path, ERRSTR);

	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.')
			continue;

		struct stat st;
		if (fstatat(dirfd(d), de->d_name, &st, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW) == -1)
			fatal("%s: Cannot stat '%s/%s': %s.", argv0, path, de->d_name, ERRSTR);

		if (!S_ISREG(st.st_mode))
			continue;

		/* Remove from the environment if the file is empty. */
		if (st.st_size == 0) {
			env_del(de->d_name, strlen(de->d_name));
			continue;
		}

		int fd = safe_openat(dirfd(d), de->d_name, O_RDONLY);
		if (fd < 0)
			fatal("%s: Cannot open '%s/%s': %s.\n", argv0, path, de->d_name, ERRSTR);

		struct dbuf linebuf = DBUF_INIT;
		struct dbuf overflow = DBUF_INIT;

		dbuf_addstr(&linebuf, de->d_name);
		dbuf_addch(&linebuf, '=');

		ssize_t bytes = freadline(fd, &linebuf, &overflow, 0);
		if (bytes < 1)
			fatal("%s: Cannot read '%s': %s.\n", argv0, de->d_name, ERRSTR);

		/* Chomp spaces around the value. */
		char* entry = dbuf_str(&linebuf);
		while (--bytes && is_trim_char(entry[bytes]))
			/* empty */;

		if (entry[bytes++] == '=')
		    env_del(entry, bytes - 1);
		else
		    env_add(strndup(entry, bytes));

		dbuf_clear(&overflow);
		dbuf_clear(&linebuf);

		if (safe_close(fd) == -1)
			clog_warning("Cannot close '%s/%s: %s (ignored).", path, de->d_name, ERRSTR);
	}
	closedir(d);
}

static enum cflag_status
opt_envdir(const struct cflag *spec, const char *arg)
{
	if (!spec)
		return CFLAG_NEEDS_ARG;

	env_envdir(arg);
	return CFLAG_OK;
}

static enum cflag_status
opt_file(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    int fd = safe_openat(AT_FDCWD, arg, O_RDONLY);
    if (fd < 0)
        fatal("%s: Cannot open '%s': %s.\n", argv0, arg, ERRSTR);

    struct dbuf linebuf = DBUF_INIT;
    struct dbuf overflow = DBUF_INIT;

    for (;;) {
        ssize_t bytes = freadline(fd, &linebuf, &overflow, 0);
        if (bytes == 0)
            break; /* EOF */

        if (bytes < 0)
            fatal("%s: Cannot read '%s': %s.\n", argv0, arg, ERRSTR);

        /* Chomp spaces around the entry. */
        char *entry = dbuf_str(&linebuf);

        while (bytes && is_trim_char(entry[0]))
            bytes--, entry++;
        while (bytes && is_trim_char(entry[bytes]))
            bytes--;

        if (bytes-- && entry[0] != '#') {
            const char *equalsign = strchr(entry, '=');
            assert(equalsign);
            if (equalsign - entry + 1 == bytes)
                env_del(entry, equalsign - entry);
            else
                env_add(strndup(entry, bytes));
        }

        dbuf_clear(&linebuf);
    }

    dbuf_clear(&overflow);
    dbuf_clear(&linebuf);
	if (safe_close(fd) == -1)
		clog_warning("Cannot close '%s: %s (ignored).\n", arg, ERRSTR);
    return CFLAG_OK;
}

static const struct cflag denv_options[] = {
	{
		.name = "inherit-env", .letter = 'I',
		.func = opt_inherit_env,
		.help = "Inherit all environment variables of the calling process.",
	},
	{
		.name = "inherit", .letter = 'i',
		.func = opt_inherit,
		.help = "Inherit an environment variable of the calling process.",
	},
    {
        .name = "environ", .letter = 'E',
        .func = opt_environ,
        .help =
            "Define an environment variable, or if no value is given, "
            "delete it. This option can be specified multiple times.",
    },
	{
		.name = "envdir", .letter = 'd',
		.func = opt_envdir,
		.help =
			"Add environment variables from the contents of files in "
			"a directory.",
	},
	{
		.name = "file", .letter = 'f',
		.func = opt_file,
		.help =
			"Add environment variables from a file in the environment.d(5)"
			" format. Note: $VARIABLE expansions are not supported.",
	},
	CFLAG_HELP,
	CFLAG_END
};

int
denv_main(int argc, char **argv)
{
	clog_init(NULL);

	/* XXX: It would be neat to have e.g. a cflag_argv0() function that did this. */
	argv0 = strrchr(argv[0], '/');
	if (argv0 == NULL)
		argv0 = argv[0];
	else
		argv0++;

	if (strcmp(argv0, "envdir") == 0) {
		if (argc < 3)
			fatal("%s: usage: %s d child\n", argv0, argv0);

		env_inherit_all();
		env_envdir(argv[1]);

		/* Adjust directly because replace_args_shift() does not touch argv[0]. */
		argv += 2;
		argc -= 2;
	} else {
		cflag_apply(denv_options, "[path] command [command-options...]", &argc, &argv);
		if (!argc)
			fatal("%s: No command specified.\n", argv0);
	}

	execvpe(argv[0], argv, env);
	fatal("%s: Cannot execute '%s': %s.\n", argv0, argv[0], ERRSTR);
}
