/* sys/resource.h replacement for Windows */
#ifndef _WINDOWS_SYS_RESOURCE_H
#define _WINDOWS_SYS_RESOURCE_H

#ifdef _WIN32

/* Windows doesn't have getrlimit/setrlimit */
/* Define minimal stubs for compatibility */

#define RLIMIT_STACK 3
#define PRIO_PROCESS 0

struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};

static inline int getrlimit(int resource, struct rlimit *rlim) {
    (void)resource;
    rlim->rlim_cur = 8 * 1024 * 1024;  /* 8MB default stack */
    rlim->rlim_max = 8 * 1024 * 1024;
    return 0;
}

static inline int setrlimit(int resource, const struct rlimit *rlim) {
    (void)resource;
    (void)rlim;
    return 0;  /* Silently ignore on Windows */
}

static inline int getpriority(int which, int who) {
    (void)which;
    (void)who;
    return 0;
}

static inline int setpriority(int which, int who, int prio) {
    (void)which;
    (void)who;
    (void)prio;
    return 0;
}

#else
#include_next <sys/resource.h>
#endif

#endif
