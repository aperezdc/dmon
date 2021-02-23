/*
 * drlog.c
 * Copyright © 2010-2020 Adrián Pérez <aperez@igalia.com>
 * Copyright © 2008-2010 Adrián Pérez <aperez@connectical.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "deps/cflag/cflag.h"
#include "deps/dbuf/dbuf.h"
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

#if !(defined(MULTICALL) && MULTICALL)
# define drlog_main main
#endif /* MULTICALL */

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
static int                out_fd     = -1;
static int                in_fd      = -1;
static unsigned           maxfiles   = LOGFILE_DEFMAX;
static unsigned long long maxtime    = LOGFILE_DEFTIME;
static size_t             maxsize    = LOGFILE_DEFSIZE;
static unsigned long long curtime    = 0;
static unsigned long long cursize    = 0;
static bool               timestamp  = false;
static bool               buffered   = false;
static bool               skip_empty = false;
static int                returncode = 0;
static struct dbuf        line       = DBUF_INIT;
static struct dbuf        overflow   = DBUF_INIT;


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
        fprintf (stderr, "Unable to open directory '%s' for rotation (%s).\n",
                 directory, ERRSTR);
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
            fprintf (stderr, "Path too long to unlink: %s/%s\n", directory, old_name);
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

    if (out_fd < 0) {
        /* We need to open the output file. */
        char path[MAXPATHLEN];
        struct stat st;
        time_t ts;
        FILE *ts_file;

testdir:
        if (stat (directory, &st) < 0 || !S_ISDIR (st.st_mode))
            die ((errno == ENOENT)
                     ? "Output directory does not exist: %s\n"
                     : "Output path is not a directory: %s\n",
                  directory);

        if (snprintf (path, sizeof (path), "%s/" LOGFILE_CURRENT, directory) < 0)
            die ("Path name too long: %s\n", directory);

        if ((out_fd = open (path, O_APPEND | O_CREAT | O_WRONLY, LOGFILE_PERMS)) < 0)
            die ("Cannot open '%s': %s\n", path, ERRSTR);

        if (snprintf (path, sizeof (path), "%s/" LOGDIR_TSTAMP, directory) < 0) {
            close (out_fd);
            die ("Path name too long: %s\n", directory);
        }

        if ((ts_file = fopen (path, "r")) == NULL) {
            int ts_fd;
recreate_ts:
            ts = time (NULL);
            if ((ts_fd = open (path, O_WRONLY | O_CREAT | O_TRUNC, LOGFILE_PERMS)) < 0) {
                close (out_fd);
                die ("Unable to write timestamp to '%s', %s\n", directory, ERRSTR);
            }
            ts_file = fdopen (ts_fd, "w");
            assert (ts_file != NULL);

            if (fprintf (ts_file, "%lu\n", (unsigned long) ts) < 0)
                die ("Unable to write to '%s': %s\n", path, ERRSTR);
        }
        else {
            unsigned long long ts_;

            if (fscanf (ts_file, "%llu", &ts_) != 1 || ferror (ts_file)) {
                fclose (ts_file);
                goto recreate_ts;
            }
            ts = ts_;
        }
        fclose (ts_file);
        ts_file = NULL;

        curtime = ts;
        if (maxtime) {
            curtime -= (curtime % maxtime);
        }
        cursize = (unsigned long long) lseek (out_fd, 0, SEEK_CUR);
    }

    if ((maxsize != 0) && (maxtime != 0)) {
        if (cursize >= maxsize || (unsigned long long) now > (curtime + maxtime)) {
            struct tm *time_gm;
            char path[MAXPATHLEN];
            char newpath[MAXPATHLEN];

            if (out_fd < 0) {
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

            close (out_fd);
            out_fd = -1;

            if (rename (path, newpath) < 0 && unlink (path) < 0)
                die ("Unable to rename '%s' to '%s'\n", path, newpath);

            if (snprintf (path, sizeof (path), "%s/" LOGDIR_TSTAMP, directory) < 0)
                die ("Path name too long: %s\n", directory);

            unlink (path);
            goto testdir;
        }
    }

    if (dbuf_empty(&line) || (skip_empty && dbuf_size(&line) == 1))
        return;

    char timebuf[TSTAMP_LEN+1];
    struct iovec iov[2];
    int n_iov = 0;

    if (timestamp) {
        struct tm *time_gm = gmtime (&now);
        size_t timebuf_len;

        if ((timebuf_len = strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm)) == 0)
            die ("Cannot format timestamp\n");

        iov[n_iov++] = iov_from_data (timebuf, timebuf_len);
    }

    iov[n_iov++] = iov_from_buffer (&line);
    assert ((unsigned) n_iov <= (sizeof (iov) / sizeof (iov[0])));

    for (;;) {
        ssize_t r = writev (out_fd, iov, n_iov);
        if (r > 0) {
            cursize += r;
            break;
        }

        fprintf (stderr, "Cannot write to logfile: %s.\n", ERRSTR);
        safe_sleep (5);
    }

    if (!buffered)
        fsync (out_fd);

    dbuf_clear(&line);
}


static void
close_log (void)
{
    flush_line ();

    for (;;) {
        if (close (out_fd) == 0) {
            out_fd = -1;
            break;
        }

        fprintf (stderr, "Unable to close logfile at directory '%s', %s\n",
                 directory, ERRSTR);
        safe_sleep (5);
    }
}


static void
roll_handler (int signum)
{
    (void) signum;
    close_log ();
}


__attribute__((noreturn))
static void
quit_handler (int signum)
{
    (void) signum;

    flush_line ();
    dbuf_addbuf(&line, &overflow);
    close_log ();
    exit (returncode);
}


static const struct cflag drlog_options[] = {
    CFLAG(uint, "max-files", 'm', &maxfiles,
          "Maximum number of log files to keep."),
    CFLAG(timei, "max-time", 'T', &maxtime,
          "Maximum time to use a log file (suffixes: mhdwMy)."),
    CFLAG(bytes, "max-size", 's', &maxsize,
          "Maximum size of each log file (suffixes: kmg)."),
    CFLAG(int, "input-fd", 'i', &in_fd,
          "File descriptor to read input from (default: stdin)."),
    CFLAG(bool, "buffered", 'b', &buffered,
          "Buffered operation, do not flush to disk after each line."),
    CFLAG(bool, "timestamp", 't', &timestamp,
          "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line."),
    CFLAG(bool, "skip-empty", 'e', &skip_empty,
          "Ignore empty lines with no characters."),
    CFLAG_HELP,
    CFLAG_END
};

int drlog_main (int argc, char **argv)
{
    int in_fd = STDIN_FILENO;
    char *env_opts = NULL;
    struct sigaction sa;

    if ((env_opts = getenv ("DRLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    const char *argv0 = cflag_apply(drlog_options, "[options] logdir-path", &argc, &argv);

    if (!argc)
        die ("%s: No log directory path was specified.\n", argv0);

    directory = argv[0];

    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = roll_handler;
    safe_sigaction ("HUP", SIGHUP, &sa);

    sa.sa_handler = quit_handler;
    safe_sigaction ("INT", SIGINT, &sa);

    sa.sa_handler = quit_handler;
    safe_sigaction ("TERM", SIGTERM, &sa);

    for (;;) {
        ssize_t bytes = freadline (in_fd, &line, &overflow, 0);
        if (bytes == 0)
            break; /* EOF */

        if (bytes < 0) {
            fprintf (stderr, "Unable to read from standard input: %s.\n", ERRSTR);
            returncode = 1;
            quit_handler (0);
        }

        flush_line ();
    }

    quit_handler (0);
    return 0; /* Keep compiler happy */
}

