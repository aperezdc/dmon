/*
 * dinit-util.c
 * Copyright (C) 2011 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "util.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


static const struct {
    const char   *fstype;
    const char   *mountpoint;
    const char   *options;
    unsigned long flags;
} s_mounts[] = {
    { "proc",     "/proc",    NULL,        MS_NOSUID | MS_NOEXEC | MS_NODEV },
    { "sysfs",    "/sys",     NULL,        MS_NOSUID | MS_NOEXEC | MS_NODEV },
    { "devtmpfs", "/dev",     "mode=755",  MS_NOSUID                        },
    { "tmpfs",    "/run",     "mode=755",  MS_NOSUID | MS_NOEXEC            },
    { "tmpfs",    "/dev/shm", "mode=1777", MS_NOSUID | MS_NODEV             },
    { "devpts",   "/dev/pts", "mode=620",  MS_NOSUID | MS_NOEXEC            },
};


static const struct {
    const char *src;
    const char *dst;
} s_symlinks[] = {
    { "/proc/kcore",     "/dev/core"   },
    { "/proc/self/fd",   "/dev/fd"     },
    { "/proc/self/fd/0", "/dev/stdin"  },
    { "/proc/self/fd/1", "/dev/stdout" },
    { "/proc/self/fd/2", "/dev/stderr" },
    { "/run",            "/var/run"    },
};


static const char *s_mkdirs[] = {
    "/tmp",
    "/var",       /* Ensure that /var exists so /var/run can be created */
    "/run/dinit", /* This is used by dinit itself, has to exist */
};


int
dinit_do_mounts (void)
{
    static w_bool_t done = W_NO;
    unsigned i;

    if (done) return 0;

    for (i = 0; i < w_lengthof (s_mounts); i++) {
        if (!mkdir_p (s_mounts[i].mountpoint, 0777) ||
            mount (s_mounts[i].fstype,
                   s_mounts[i].mountpoint,
                   s_mounts[i].fstype,
                   s_mounts[i].flags,
                   s_mounts[i].options) < 0)
        {
            return errno;
        }
    }

    for (i = 0; i < w_lengthof (s_mkdirs); i++) {
        if (!mkdir_p (s_mkdirs[i], 0777))
            return errno;
    }

    for (i = 0; i < w_lengthof (s_symlinks); i++) {
        if (symlink (s_symlinks[i].dst, s_symlinks[i].src) < 0)
            return errno;
    }

    done = W_YES;
    return 0;
}


#include <sys/reboot.h>

#if defined(__linux)
# define REBOOT      reboot
# define D_HALT      RB_HALT_SYSTEM
# define D_POWEROFF  RB_POWER_OFF
# define D_REBOOT    RB_AUTOBOOT
#elif defined(__FreeBSD__)
# define REBOOT      reboot
# define D_HALT      RB_HALT
# define D_POWEROFF  RB_POWEROFF
# define D_REBOOT    RB_AUTOBOOT
#elif defined(__NetBSD__)
# define REBOOT(x)   reboot (x, NULL)
# define D_HALT      RB_HALT
# define D_REBOOT    RB_AUTOBOOT
# define D_POWEROFF (RB_HALT | RB_POWERDOWN)
#elif defined(__OpenBSD__)
# define REBOOT      reboot
# define D_HALT      RB_HALT
# define D_REBOOT    RB_AUTOBOOT
# define D_POWEROFF (RB_HALT | RB_POWERDOWN)
#else
# define REBOOT(x)  ((void)0)
#endif


void
dinit_halt (void)
{
    REBOOT (D_HALT);
}

void
dinit_poweroff (void)
{
    REBOOT (D_POWEROFF);
}

void
dinit_reboot (void)
{
    REBOOT (D_REBOOT);
}

