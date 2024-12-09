/*
 * Copyright (C) 2024 Adrian Perez de Castro <aperez@igalia.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE

#include "deps/cflag/cflag.h"
#include "deps/clog/clog.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !(defined(MULTICALL) && MULTICALL)
# define denv_main main
#endif /* MULTICALL */

static char** env = NULL;
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
		if (!env) {
			fputs("cannot allocate memory\n", stderr);
			_exit(111);
		}
	}
	assert(env_a > (env_n + 1));

	env_del(entry, equalsign - entry);
	env[env_n++] = entry;
	env[env_n] = NULL;
}

static enum cflag_status
_environ_inherit(const struct cflag *spec, const char *arg)
{
	(void) arg;
	if (!spec)
		return CFLAG_OK;

	for (size_t i = 0; environ[i]; i++) {
		assert(strchr(environ[i], '='));
		env_add(environ[i]);
	}

	return CFLAG_OK;
}

static enum cflag_status
_environ_inherit_var(const struct cflag *spec, const char *arg)
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
_environ_option(const struct cflag *spec, const char *arg)
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

static const struct cflag denv_options[] = {
	{
		.name = "inherit-env", .letter = 'I',
		.func = _environ_inherit,
		.help = "Inherit all environment variables of the calling process.",
	},
	{
		.name = "inherit", .letter = 'i',
		.func = _environ_inherit_var,
		.help = "Inherit an environment variable of the calling process.",
	},
    {
        .name = "environ", .letter = 'E',
        .func = _environ_option,
        .help =
            "Define an environment variable, or if no value is given, "
            "delete it. This option can be specified multiple times.",
    },
	CFLAG_HELP,
	CFLAG_END
};

int
denv_main(int argc, char **argv)
{
	clog_init(NULL);

	const char *argv0 = cflag_apply(denv_options, "[path] command [command-options...]", &argc, &argv);
	if (!argc) {
		fprintf(stderr, "%s: No command specified.\n", argv0);
		return EXIT_FAILURE;
	}

	execvpe(argv[0], argv, env);
	_exit(111);

	assert(!"unreachable");
	return EXIT_FAILURE;
}
