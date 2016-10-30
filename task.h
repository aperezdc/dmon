/*
 * task.h
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __task_h__
#define __task_h__

#include "util.h"
#include <sys/types.h>

typedef enum {
    A_NONE = 0,
    A_START,
    A_STOP,
    A_SIGNAL,
} action_t;


W_OBJ (task_t)
{
    w_obj_t  parent;
    pid_t    pid;
    action_t action;
    int      argc;
    char   **argv;
    int      write_fd;
    int      read_fd;
    int      signal;
    time_t   started;
    uidgid_t user;
    unsigned redir_errfd;
};

#define NO_PID    (-1)
#define NO_SIGNAL (-1)
#define TASK      { W_OBJ_STATIC (NULL), \
                    NO_PID,    \
                    A_START,   \
                    0,         \
                    NULL,      \
                    -1,        \
                    -1,        \
                    NO_SIGNAL, \
                    0,         \
                    UIDGID,    \
                    0 }

task_t* task_new             (int argc, const char **argv);
void    task_start           (task_t *task);
void    task_signal_dispatch (task_t *task);
void    task_action_dispatch (task_t *task);
void    task_action_queue    (task_t *task, action_t action);
void    task_signal          (task_t *task, int signum);
void    task_signal_queue    (task_t *task, int signum);
void    task_action          (task_t *task, action_t action);

#endif /* !__task_h__ */

/* vim: expandtab tabstop=4 shiftwidth=4
 */
