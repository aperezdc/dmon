/*
 * dinit-service.h
 * Copyright (C) 2012 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __dinit_service_h__
#define __dinit_service_h__

#include "wheel.h"

#ifndef DINIT_SERVICES
#define DINIT_SERVICES "/etc/dinit"
#endif /* !DINIT_SERVICES */

typedef enum {
    SERVICE_ONCE,
    SERVICE_RESPAWN,
} service_mode_t;

typedef enum {
    SERVICE_LOG_NONE,
    SERVICE_LOG_SYSLOG,
    SERVICE_LOG_APPEND,
    SERVICE_LOG_ROTATE,
} service_log_mode_t;

typedef struct {
    int      facility;
    int      priority;
    w_bool_t console;
} service_log_syslog_t;

typedef struct {
    unsigned max_files;
    unsigned long long max_time;
    unsigned long long max_size;
} service_log_file_rotate_t;

typedef struct {
    char    *path;
    w_bool_t buffered;
    w_bool_t timestamp;
    union {
        service_log_file_rotate_t rotate;
    };
} service_log_filesystem_t;

typedef struct {
    service_log_mode_t mode;
    char              *prefix;
    union {
        service_log_syslog_t     syslog;
        service_log_filesystem_t filesystem;
    };
} service_log_t;

typedef struct {
    char *shell;
    char *script;
} service_script_t;

W_OBJ (service_t)
{
    w_obj_t          parent;
    char            *name;
    service_mode_t   mode;
    service_log_t    log;
    w_bool_t         enabled;
    char            *command;
    char            *pidfile;
    char            *environ;
    service_script_t before;
    service_script_t after;
};


void      _service_free  (void*); /* internal */
service_t* service_parse (const char *name, char **errmsg);


/*!
 * Format of service files
 */

#endif /* !__dinit_service_h__ */

