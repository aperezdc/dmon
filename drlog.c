/*
 * drlog.c
 * Copyright © 2010-2014 Adrián Pérez <aperez@igalia.com>
 * Copyright © 2008-2010 Adrián Pérez <aperez@connectical.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel/wheel.h"
#include "util.h"
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#ifdef NO_MULTICALL
# define drlog_main main
#endif /* NO_MULTICALL */

#ifndef LOGDIR_PERMS
#define LOGDIR_PERMS 0750
#endif /* !LOGDIR_PERMS */

#ifndef LOGDIR_TSTAMP
#define LOGDIR_TSTAMP ".timestamp"
#endif /* !LOGDIR_TSTAMP */

#ifndef LOGFILE_PERMS
#define LOGFILE_PERMS 0640
#endif /* !LOGFILE_PERMS */

#ifndef LOGFILE_PREFIX
#define LOGFILE_PREFIX "log-"
#endif /* !LOGFILE_PREFIX */

#ifndef LOGFILE_CURRENT
#define LOGFILE_CURRENT "current"
#endif /* !LOGFILE_CURRENT */

#ifndef LOGFILE_DEFMAX
#define LOGFILE_DEFMAX 10
#endif /* !LOGFILE_DEFMAX */

#ifndef LOGFILE_DEFSIZE
#define LOGFILE_DEFSIZE (150 * 1024) /* 150 kB */
#endif /* !LOGFILE_DEFSIZE */

#ifndef LOGFILE_DEFTIME
#define LOGFILE_DEFTIME (60 * 60 * 24 * 5) /* Five days */
#endif /* !LOGFILE_DEFTIME */

#ifndef TSTAMP_FMT
#define TSTAMP_FMT "%Y-%m-%d/%H:%M:%S "
#endif /* !TSTAMP_FMT */

#ifndef TSTAMP_LEN
#define TSTAMP_LEN (5 + 3 + 3 + 3 + 3 + 3)
#endif /* !TSTAMP_LEN */


static char              *directory  = NULL;
static w_io_t            *out_io     = NULL;
static int                in_fd      = -1;
static unsigned           maxfiles   = LOGFILE_DEFMAX;
static unsigned long long maxtime    = LOGFILE_DEFTIME;
static unsigned long long maxsize    = LOGFILE_DEFSIZE;
static unsigned long long curtime    = 0;
static unsigned long long cursize    = 0;
static bool               timestamp  = false;
static bool               buffered   = false;
static int                returncode = 0;
static w_buf_t            line       = W_BUF;
static w_buf_t            overflow   = W_BUF;


static int
rotate_log (void)
{
    char old_name[MAXPATHLEN];
    char path[MAXPATHLEN];
    const char *name;
    DIR *dir;
    struct dirent *dirent;
    unsigned foundlogs;
    int year, mon, mday, hour, min, sec;
    int older_year, older_mon = INT_MAX, older_mday = INT_MAX,
            older_hour = INT_MAX, older_min = INT_MAX, older_sec = INT_MAX;

rescan:
    foundlogs = 0;
    *old_name = 0;
    older_year = INT_MAX;

    if ((dir = opendir (directory)) == NULL) {
        w_printerr ("Unable to open directory '$s' for rotation ($E).\n",
                    directory);
        return -1;
    }

    while ((dirent = readdir (dir)) != NULL) {
        name = dirent->d_name;
        if (!strncmp (name, LOGFILE_PREFIX, sizeof (LOGFILE_PREFIX) - 1U)) {
            if (sscanf (name, LOGFILE_PREFIX "%d-%d-%d-%d:%d:%d",
                        &year, &mon, &mday, &hour, &min, &sec) != 6)
                continue;

            foundlogs++;
            if (year < older_year || (year == older_year &&
               (mon  < older_mon  || (mon  == older_mon  &&
               (mday < older_mday || (mday == older_mday &&
               (hour < older_hour || (hour == older_hour &&
               (min  < older_min  || (min  == older_min  &&
               (sec  < older_sec))))))))))) /* Oh yeah! */
            {
                older_year = year;
                older_mon  = mon ;
                older_mday = mday;
                older_hour = hour;
                older_min  = min ;
                older_sec  = sec ;
                strncpy (old_name, name, sizeof (old_name));
                old_name[sizeof (old_name) - 1] = 0;
            }
        }
    }

    closedir (dir);

    if (foundlogs >= maxfiles) {
        if (*old_name == 0) {
            return -3;
        }
        if (snprintf (path, sizeof (path), "%s/%s", directory, old_name) < 0) {
            w_printerr ("Path too long to unlink: $s/$s\n",
                        directory, old_name);
            return -4;
        }
        if (unlink (path) < 0) {
            return -2;
        }
        foundlogs--;
        if (foundlogs >= maxfiles) {
            goto rescan;
        }
    }
    return 0;
}


static void
flush_line (void)
{
    time_t now = time (NULL);
    w_buf_t out = W_BUF;

    if (!out_io) {
        /* We need to open the output file. */
        char path[MAXPATHLEN];
        struct stat st;
        time_t ts;
        w_io_t *ts_io;

testdir:
        if (stat (directory, &st) < 0 || !S_ISDIR (st.st_mode))
            die ((errno == ENOENT)
                     ? "Output directory does not exist: %s\n"
                     : "Output path is not a directory: %s\n",
                  directory);

        if (snprintf (path, sizeof (path), "%s/" LOGFILE_CURRENT, directory) < 0)
            die ("Path name too long: %s\n", directory);

        out_io = w_io_unix_open (path,
                                 O_APPEND | O_CREAT | O_WRONLY,
                                 LOGFILE_PERMS);

        if (!out_io)
            die ("Cannot open '%s': %s\n", path, ERRSTR);

        if (snprintf (path, sizeof (path), "%s/" LOGDIR_TSTAMP, directory) < 0) {
            w_obj_unref (out_io);
            die ("Path name too long: %s\n", directory);
        }

        if ((ts_io = w_io_unix_open (path, O_RDONLY, 0)) == NULL) {
recreate_ts:
            ts = time (NULL);
            ts_io = w_io_unix_open (path, O_WRONLY | O_CREAT | O_TRUNC,
                                    LOGFILE_PERMS);
            if (!ts_io) {
                w_obj_unref (out_io);
                die ("Unable to write timestamp to '%s', %s\n", directory, ERRSTR);
            }
            w_io_result_t r = w_io_format (ts_io, "$I\n", (unsigned long) ts);
            if (w_io_failed (r))
                die ("Unable to write to '%s': %s\n", path, ERRSTR);
        }
        else {
            unsigned long long ts_;
            w_buf_t tsb = W_BUF;

            if (w_io_failed (w_io_read_line (ts_io, &tsb, &overflow, 0)) ||
                (sscanf (w_buf_str (&tsb), "%llu", &ts_) <= 0))
            {
                w_obj_unref (ts_io);
                w_buf_clear (&tsb);
                goto recreate_ts;
            }
            ts = ts_;
            w_buf_clear (&tsb);
        }
        w_obj_unref (ts_io);
        curtime = ts;
        if (maxtime) {
            curtime -= (curtime % maxtime);
        }
        cursize = (unsigned long long) lseek (w_io_get_fd ((w_io_unix_t*) out_io),
                                              0, SEEK_CUR);
    }

    if ((maxsize != 0) && (maxtime != 0)) {
        if (cursize >= maxsize || (unsigned long long) now > (curtime + maxtime)) {
            struct tm *time_gm;
            char path[MAXPATHLEN];
            char newpath[MAXPATHLEN];

            if (!out_io) {
                die ("Internal inconsistency at %s:%i\n", __FILE__, __LINE__);
                assert (!"unreachable");
            }

            if ((time_gm = gmtime(&now)) == NULL)
                die ("Unable to get current date: %s\n", ERRSTR);

            if (snprintf (newpath, sizeof (newpath), "%s/" LOGFILE_PREFIX, directory) < 0)
                die ("Path name too long: %s\n", directory);

            if (strftime (newpath + strlen (newpath),
                          sizeof (newpath) - strlen(newpath),
                          "%Y-%m-%d-%H:%M:%S",
                          time_gm) == 0)
                die ("Path name too long: '%s'\n", directory);

            if (snprintf(path, sizeof (path), "%s/" LOGFILE_CURRENT, directory) < 0)
                die ("Path name too long: %s\n", directory);

            rotate_log ();

            w_obj_unref (out_io);
            out_io = NULL;

            if (rename (path, newpath) < 0 && unlink (path) < 0)
                die ("Unable to rename '%s' to '%s'\n", path, newpath);

            if (snprintf (path, sizeof (path), "%s/" LOGDIR_TSTAMP, directory) < 0)
                die ("Path name too long: %s\n", directory);

            unlink (path);
            goto testdir;
        }
    }

    if (w_buf_is_empty (&line)) {
        w_buf_clear (&line);
        return;
    }

    if (timestamp) {
        char timebuf[TSTAMP_LEN+1];
        struct tm *time_gm = gmtime (&now);

        if (strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm) == 0)
            die ("Cannot format timestamp\n");

        w_buf_append_mem (&out, timebuf, strlen (timebuf));
    }

    w_buf_append_buf (&out, &line);
    w_buf_append_char (&out, '\n');
    w_buf_clear (&line);

    for (;;) {
        w_io_result_t r = w_io_write (out_io,
                                      w_buf_const_data (&out),
                                      w_buf_size (&out));
        if (!w_io_failed (r)) {
            if (!buffered) {
                W_IO_NORESULT (w_io_flush (out_io));
            }
            cursize += w_buf_size (&out);
            break;
        }
        w_printerr ("Cannot write to logfile: $E.\n");
        safe_sleep (5);
    }
    w_buf_clear (&out);
}


static void
close_log (void)
{
    flush_line ();

    for (;;) {
        if (!w_io_failed (w_io_close (out_io))) {
            w_obj_unref (out_io);
            out_io = NULL;
            break;
        }

        w_printerr ("Unable to close logfile at directory '$s\n",
                    directory);
        safe_sleep (5);
    }
}


static void
roll_handler (int signum)
{
    w_unused (signum);
    close_log ();
}


static void
quit_handler (int signum)
{
    w_unused (signum);

    flush_line ();
    w_buf_append_buf (&line, &overflow);
    close_log ();
    exit (returncode);
}


#define HELP_MESSAGE \
    "Usage: rotlog [OPTIONS] logdir\n"                                 \
    "Log standard input to a directory of timestamped files\n"         \
    "which are automatically rotated as-needed.\n"                     \


static const w_opt_t drlog_options[] = {
    { 1, 'm', "max-files", W_OPT_UINT, &maxfiles,
        "Maximum number of log files to keep." },

    { 1, 'T', "max-time", W_OPT_TIME_PERIOD, &maxtime,
        "Maximum time to use a log file (suffixes: mhdwMy)." },

    { 1, 's', "max-size", W_OPT_DATA_SIZE, &maxsize,
        "Maximum size of each log file (suffixes: kmg)." },

    { 1, 'i', "input-fd", W_OPT_INT, &in_fd,
        "File descriptor to read input from (default: stdin)." },

    { 1, 'b', "buffered", W_OPT_BOOL, &buffered,
        "Buffered operation, do not flush to disk after each line." },

    { 0, 't', "timestamp", W_OPT_BOOL, &timestamp,
        "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line." },

    W_OPT_END
};


int drlog_main (int argc, char **argv)
{
    struct sigaction sa;
    w_io_t *in_io = NULL;
    char *env_opts = NULL;
    unsigned consumed;

    if ((env_opts = getenv ("DRLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    consumed = w_opt_parse (drlog_options, NULL, NULL, "logdir-path", argc, argv);

    if (consumed >= (unsigned) argc)
        die ("%s: No log directory path was specified.\n", argv[0]);

    directory = argv[consumed];

    in_io = (in_fd >= 0) ? w_io_unix_open_fd (in_fd) : w_stdin;
    if (!in_io)
        die ("%s: Cannot open input: %s.\n", argv[0], ERRSTR);

    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = roll_handler;
    safe_sigaction ("HUP", SIGHUP, &sa);

    sa.sa_handler = quit_handler;
    safe_sigaction ("INT", SIGINT, &sa);

    sa.sa_handler = quit_handler;
    safe_sigaction ("TERM", SIGTERM, &sa);

    for (;;) {
        w_io_result_t ret = w_io_read_line (in_io, &line, &overflow, 0);

        if (w_io_failed (ret)) {
            w_printerr ("Unable to read from standard input: $E.\n");
            returncode = 1;
            quit_handler (0);
        }
        if (w_io_eof (ret))
            break;
        flush_line ();
    }

    quit_handler (0);
    return 0; /* Keep compiler happy */
}

