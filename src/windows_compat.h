/* Copyright (C)
* 2025 - piHPSDR Windows Porting Team
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

/**
 * @file windows_compat.h
 * @brief Windows compatibility layer for piHPSDR
 *
 * This header provides compatibility between Windows and POSIX systems.
 * It abstracts differences in:
 * - Network/socket headers and APIs (Winsock2 vs POSIX sockets)
 * - File paths (backslash vs forward slash)
 * - Sleep functions
 * - Other platform-specific APIs
 *
 * Usage:
 *   #include "windows_compat.h"
 *
 * This should be included BEFORE any network-related headers in Windows builds.
 */

#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

/*
 * Platform detection
 */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__) || defined(WINDOWS)
  #define PLATFORM_WINDOWS 1
#else
  #define PLATFORM_WINDOWS 0
#endif

/*
 * =============================================================================
 * WINDOWS-SPECIFIC INCLUDES AND DEFINITIONS
 * =============================================================================
 */
#if PLATFORM_WINDOWS

/*
 * Prevent Windows headers from defining symbols that conflict with application enums
 * We define these as macros BEFORE including Windows headers, preventing typedef/define.
 * These will be undefined immediately after the includes.
 */
#define SNB _SKIP_SNB_TYPEDEF       // Prevent objidl.h from defining SNB typedef
#define ABSOLUTE _SKIP_ABSOLUTE     // Prevent wingdi.h from defining ABSOLUTE
#define RELATIVE _SKIP_RELATIVE     // Prevent wingdi.h from defining RELATIVE
#define PRESSED _SKIP_PRESSED       // Prevent potential Windows PRESSED macro
#define RELEASED _SKIP_RELEASED     // Prevent potential Windows RELEASED macro

/*
 * Windows Socket API (Winsock2)
 * IMPORTANT: winsock2.h must be included BEFORE windows.h to avoid conflicts
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

/*
 * Now undefine the macros we used to block Windows typedefs/macros
 * This allows our application enums (in actions.h) to use these names
 */
#undef SNB
#undef ABSOLUTE
#undef RELATIVE
#undef PRESSED
#undef RELEASED
#include <string.h>
#include <stdio.h>     // For snprintf()
#include <io.h>        // For _close(), _open(), etc.
#include <stdarg.h>    // For va_list, va_start, va_end

/*
 * Network database compatibility (netdb.h)
 * Windows has these functions in winsock2.h, not netdb.h
 * Provide a compatibility header guard so code can include <netdb.h>
 */
#define _NETDB_H_  // Prevent any potential netdb.h include
// struct hostent, gethostbyname, etc. are already in winsock2.h

/*
 * ARPA/inet compatibility (arpa/inet.h)
 * Windows has these functions in winsock2.h and ws2tcpip.h, not arpa/inet.h
 */
#define _ARPA_INET_H  // Prevent any potential arpa/inet.h include
// inet_ntoa, inet_addr, etc. are already in winsock2.h

/*
 * Socket type compatibility
 * On Windows: SOCKET is UINT_PTR (unsigned integer)
 * On POSIX: socket is int
 * We use SOCKET type everywhere and define it as int on POSIX
 */
// SOCKET is already defined by winsock2.h on Windows

/*
 * Socket function compatibility
 * Provide wrapper functions instead of macros to avoid conflicts
 */
#define ioctl(s, cmd, arg) ioctlsocket(s, cmd, arg)

// Wrapper for close() - handles both sockets (positive values) and files
// On Windows, socket() returns SOCKET (unsigned), but code uses int
// We'll try closesocket first, if it fails try _close for file descriptors
static inline int close_compat(int fd) {
    // Attempt socket close first (most common in this codebase)
    int result = closesocket((SOCKET)fd);
    if (result == SOCKET_ERROR) {
        // If that failed, might be a file descriptor
        // Try _close (but this might also fail if it was actually a bad socket)
        return _close(fd);
    }
    return result;
}
#define close(fd) close_compat(fd)

// Wrappers for setsockopt/getsockopt - Windows expects char* instead of void*
#define setsockopt(s, level, optname, optval, optlen) \
    setsockopt(s, level, optname, (const char*)(optval), optlen)
#define getsockopt(s, level, optname, optval, optlen) \
    getsockopt(s, level, optname, (char*)(optval), optlen)

/*
 * Error code compatibility
 * Windows uses WSAGetLastError() instead of errno
 * Note: MinGW already defines some of these in errno.h, so use #ifndef guards
 */
#define ERRNO           WSAGetLastError()
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
#define EINPROGRESS     WSAEINPROGRESS
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED    WSAECONNREFUSED
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT       WSAETIMEDOUT
#endif
#ifndef ECONNRESET
#define ECONNRESET      WSAECONNRESET
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH    WSAEHOSTUNREACH
#endif
#ifndef ENETUNREACH
#define ENETUNREACH     WSAENETUNREACH
#endif

/*
 * SOL_TCP is not defined on Windows - use IPPROTO_TCP instead
 */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

/*
 * socklen_t type (not defined in older Windows headers)
 */
#ifndef socklen_t
typedef int socklen_t;
#endif

/*
 * Network address conversion
 * inet_pton and inet_ntop compatibility
 */
// These are available in ws2tcpip.h on Windows Vista+
// If targeting older Windows, we'd need shims here

/*
 * File path separator
 */
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"

/*
 * Sleep functions
 * Windows: Sleep(milliseconds)
 * POSIX: sleep(seconds), usleep(microseconds)
 */
static inline void sleep_compat(unsigned int seconds) {
    Sleep(seconds * 1000);
}
static inline int usleep_compat(unsigned int microseconds) {
    Sleep(microseconds / 1000);
    return 0;
}
// Only define macros if not already defined
#ifndef sleep
#define sleep(seconds)      sleep_compat(seconds)
#endif
#ifndef usleep
#define usleep(microseconds) usleep_compat(microseconds)
#endif

/*
 * Directory separator for paths
 * Windows accepts both / and \ but native is \
 * GLib's G_DIR_SEPARATOR should be used in GTK code
 */

/*
 * Byte-order conversion functions
 * Windows doesn't have htobe64/be64toh, provide equivalents
 */
#include <stdlib.h>
#ifndef htobe64
#define htobe64(x) _byteswap_uint64(x)
#endif
#ifndef be64toh
#define be64toh(x) _byteswap_uint64(x)
#endif
#ifndef htobe32
#define htobe32(x) _byteswap_ulong(x)
#endif
#ifndef be32toh
#define be32toh(x) _byteswap_ulong(x)
#endif
#ifndef htobe16
#define htobe16(x) _byteswap_ushort(x)
#endif
#ifndef be16toh
#define be16toh(x) _byteswap_ushort(x)
#endif

/*
 * Socket options compatibility
 */
// SO_REUSEPORT doesn't exist on Windows, map to SO_REUSEADDR
#ifndef SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

/*
 * File flags - define before fcntl
 */
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0800
#endif

/*
 * POSIX headers not available on Windows - provide minimal compatibility
 */
// poll.h - MinGW provides pollfd in winsock2.h, no need to redefine
// Just provide poll() function if not available
#ifndef HAVE_POLL
#define poll(fds, nfds, timeout) WSAPoll(fds, nfds, timeout)
#endif

// sched.h - MinGW provides sched_yield in pthread.h
// Don't redefine if pthread.h is included

// pthread.h - MinGW provides pthread, but ensure pthread_t is available
// (should be included by source files that need it)

// sys/utsname.h - system information
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static inline int uname(struct utsname *buf) {
    if (!buf) return -1;

    // Fill in Windows system information
    strcpy(buf->sysname, "Windows");

    // Get computer name
    DWORD size = sizeof(buf->nodename);
    if (!GetComputerNameA(buf->nodename, &size)) {
        strcpy(buf->nodename, "unknown");
    }

    // Get Windows version (simplified)
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetVersionExA(&osvi);
    #pragma GCC diagnostic pop
    snprintf(buf->release, sizeof(buf->release), "%lu.%lu",
             osvi.dwMajorVersion, osvi.dwMinorVersion);
    snprintf(buf->version, sizeof(buf->version), "%lu", osvi.dwBuildNumber);

    // Machine architecture
    #if defined(_WIN64)
    strcpy(buf->machine, "x86_64");
    #else
    strcpy(buf->machine, "i686");
    #endif

    return 0;
}

// sys/resource.h - process priority
#define PRIO_PROCESS 0
#define PRIO_PGRP    1
#define PRIO_USER    2

static inline int getpriority(int which, int who) {
    // Windows priority is inverted: higher number = higher priority
    // POSIX: lower number = higher priority
    // Return a value compatible with POSIX semantics
    HANDLE hProcess = GetCurrentProcess();
    int priority = GetPriorityClass(hProcess);

    // Map Windows priority to POSIX-like range
    switch (priority) {
        case REALTIME_PRIORITY_CLASS: return -20;
        case HIGH_PRIORITY_CLASS: return -10;
        case ABOVE_NORMAL_PRIORITY_CLASS: return -5;
        case NORMAL_PRIORITY_CLASS: return 0;
        case BELOW_NORMAL_PRIORITY_CLASS: return 5;
        case IDLE_PRIORITY_CLASS: return 19;
        default: return 0;
    }
}

static inline int setpriority(int which, int who, int prio) {
    // Map POSIX priority to Windows priority class
    DWORD priority_class;
    if (prio <= -15) priority_class = REALTIME_PRIORITY_CLASS;
    else if (prio <= -10) priority_class = HIGH_PRIORITY_CLASS;
    else if (prio <= -5) priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
    else if (prio <= 5) priority_class = NORMAL_PRIORITY_CLASS;
    else if (prio <= 10) priority_class = BELOW_NORMAL_PRIORITY_CLASS;
    else priority_class = IDLE_PRIORITY_CLASS;

    HANDLE hProcess = GetCurrentProcess();
    return SetPriorityClass(hProcess, priority_class) ? 0 : -1;
}

// unistd.h - getpid
// MinGW provides getpid() declaration in process.h
#include <process.h>

// fcntl.h - file control operations
// MinGW provides fcntl.h with O_* constants
#include <fcntl.h>

// sys/stat.h macros - S_ISLNK not available on Windows
#ifndef S_ISLNK
#define S_ISLNK(m) 0  // Windows doesn't have symbolic links in traditional sense
#endif

// sys/mman.h - memory mapping, not critical for Windows
// Provide no-ops
#define mlock(addr, len) 0
#define munlock(addr, len) 0
#define mlockall(flags) 0
#define munlockall() 0
#define MCL_CURRENT 1
#define MCL_FUTURE 2

/*
 * Note: MinGW provides semaphore.h via pthreads-win32, so we don't need to
 * redefine sem_t and related functions. Simply include <semaphore.h> in files
 * that need it.
 */

/*
 * termios baud rate constants - not defined on Windows
 * These are defined for compatibility but serial port functions are not implemented
 */
#ifndef B4800
#define B4800   4800
#endif
#ifndef B9600
#define B9600   9600
#endif
#ifndef B19200
#define B19200  19200
#endif
#ifndef B38400
#define B38400  38400
#endif
#ifndef B115200
#define B115200 115200
#endif

/*
 * inet_aton() compatibility - BSD function not in Windows
 */
static inline int inet_aton(const char *cp, struct in_addr *inp) {
    inp->s_addr = inet_addr(cp);
    return (inp->s_addr == INADDR_NONE) ? 0 : 1;
}

/*
 * realpath() compatibility - POSIX function not in Windows
 * Windows equivalent is _fullpath()
 */
static inline char* realpath(const char *path, char *resolved_path) {
    if (!path) return NULL;

    // If resolved_path is NULL, we need to allocate
    if (!resolved_path) {
        resolved_path = (char*)malloc(_MAX_PATH);
        if (!resolved_path) return NULL;
    }

    // Use _fullpath to resolve the path
    char* result = _fullpath(resolved_path, path, _MAX_PATH);

    // Check if file exists using _access
    if (result && _access(result, 0) != 0) {
        // File doesn't exist
        if (!resolved_path) free(result);
        return NULL;
    }

    return result;
}

/*
 * gettimeofday() - MinGW provides this, no need for compatibility wrapper
 * Note: struct timeval is already defined in winsock2.h
 */
// MinGW provides gettimeofday in sys/time.h, include it
#include <sys/time.h>

/*
 * fcntl() compatibility
 * Windows doesn't have fcntl, provide minimal implementation for socket flags
 */
#define F_GETFL 3
#define F_SETFL 4
static inline int fcntl(int fd, int cmd, ...) {
    // Only support getting/setting socket flags for O_NONBLOCK
    // For F_GETFL, return 0 (assume blocking by default)
    // For F_SETFL with O_NONBLOCK, use ioctlsocket
    if (cmd == F_GETFL) {
        return 0;  // Return 0, actual flags unknown
    }
    else if (cmd == F_SETFL) {
        // Extract flags from varargs
        va_list args;
        va_start(args, cmd);
        int flags = va_arg(args, int);
        va_end(args);

        // If O_NONBLOCK is set, make socket non-blocking
        if (flags & O_NONBLOCK) {
            u_long mode = 1;
            return ioctlsocket((SOCKET)fd, FIONBIO, &mode);
        }
    }
    return 0;
}

/*
 * BSD compatibility functions
 */
// bcopy is obsolete, use memmove
#define bcopy(src, dst, len) memmove(dst, src, len)
// bzero is obsolete, use memset
#define bzero(ptr, len) memset(ptr, 0, len)
// index is obsolete, use strchr
#define index(s, c) strchr(s, c)
// rindex is obsolete, use strrchr
#define rindex(s, c) strrchr(s, c)

/*
 * Other Windows-specific compatibility
 */
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

/*
 * Windows Winsock initialization
 * This MUST be called before using any socket functions
 */
static inline int winsock_init(void) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        return -1;
    }
    return 0;
}

static inline void winsock_cleanup(void) {
    WSACleanup();
}

// Aliases for compatibility with code that uses windows_socket_* naming
#define windows_socket_init winsock_init
#define windows_socket_cleanup winsock_cleanup

// sync() - force filesystem writes (not applicable on Windows, no-op)
#define sync() ((void)0)

/*
 * Signal compatibility
 * SIGPIPE doesn't exist on Windows (no broken pipes in same way)
 */
#ifndef SIGPIPE
  #define SIGPIPE 13  // Dummy value, signal() will ignore it anyway
#endif

/*
 * POSIX file I/O compatibility
 */
// MinGW provides these in io.h and fcntl.h, but ensure they're available
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// getpid() is in process.h (already included)
// open(), close() are provided by MinGW as _open(), _close()
// unlink() is provided as _unlink()
#define unlink _unlink

/*
 * POSIX directory functions
 */
#include <direct.h>

// mkdir on Windows takes only 1 argument (no mode), provide wrapper
static inline int posix_mkdir(const char *path, int mode) {
    (void)mode;  // Ignore mode on Windows
    return _mkdir(path);
}
// Only define mkdir macro if not already defined
#ifndef mkdir
  #define mkdir posix_mkdir
#endif

// chdir is provided by MinGW as _chdir
#define chdir _chdir

/*
 * User/password database (not available on Windows)
 * Provide minimal stub for pwd.h functions
 */
// Define struct passwd (minimal, just what's needed)
#ifndef _PWD_H_
#define _PWD_H_
struct passwd {
    char *pw_name;   // user name
    char *pw_dir;    // home directory
    char *pw_shell;  // shell program
    unsigned int pw_uid;  // user ID
    unsigned int pw_gid;  // group ID
};

static inline unsigned int getuid(void) {
    return 0;  // Always return 0 on Windows (no real UID concept)
}

static inline struct passwd* getpwuid(unsigned int uid) {
    (void)uid;  // Unused
    return NULL;  // Not implemented on Windows
}
#endif /* _PWD_H_ */

/*
 * =============================================================================
 * POSIX (Linux/macOS) INCLUDES AND DEFINITIONS
 * =============================================================================
 */
#else  /* PLATFORM_WINDOWS */

/*
 * Standard POSIX headers
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/*
 * SOCKET type compatibility (define SOCKET as int on POSIX)
 */
typedef int SOCKET;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)

/*
 * Error code compatibility
 */
#define ERRNO errno

/*
 * File path separator
 */
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"

/*
 * No-op Winsock initialization on POSIX
 */
#define winsock_init()    (0)
#define winsock_cleanup() ((void)0)

#endif /* PLATFORM_WINDOWS */

/*
 * =============================================================================
 * COMMON UTILITY FUNCTIONS (all platforms)
 * =============================================================================
 */

/**
 * @brief Convert path to native format (Windows: \, POSIX: /)
 * @param path Path string to convert (modified in-place)
 * @return Pointer to the modified string
 */
static inline char *path_to_native(char *path) {
#if PLATFORM_WINDOWS
    // Convert forward slashes to backslashes
    for (char *p = path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    // Convert backslashes to forward slashes (if any)
    for (char *p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#endif
    return path;
}

/**
 * @brief Get last socket error as string
 * @return Human-readable error message
 */
static inline const char *socket_error_string(void) {
#if PLATFORM_WINDOWS
    static char buf[256];
    int err = WSAGetLastError();

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf,
        sizeof(buf),
        NULL
    );

    return buf;
#else
    return strerror(errno);
#endif
}

/**
 * @brief Set socket to non-blocking mode
 * @param sockfd Socket file descriptor
 * @return 0 on success, -1 on error
 */
static inline int set_socket_nonblocking(SOCKET sockfd) {
#if PLATFORM_WINDOWS
    u_long mode = 1;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
}

/**
 * @brief Set socket to blocking mode
 * @param sockfd Socket file descriptor
 * @return 0 on success, -1 on error
 */
static inline int set_socket_blocking(SOCKET sockfd) {
#if PLATFORM_WINDOWS
    u_long mode = 0;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

/**
 * @brief Set socket non-blocking mode (legacy function name)
 * @param sockfd Socket file descriptor
 * @param enabled 1 to enable non-blocking, 0 to disable
 * @return 0 on success, -1 on error
 */
static inline int set_nonblocking(SOCKET sockfd, int enabled) {
    return enabled ? set_socket_nonblocking(sockfd) : set_socket_blocking(sockfd);
}

/*
 * =============================================================================
 * NETWORK INTERFACE ENUMERATION (ifaddrs compatibility)
 * =============================================================================
 */

#if PLATFORM_WINDOWS

/**
 * @brief POSIX-compatible ifaddrs structure for Windows
 *
 * This struct provides a Windows implementation of the POSIX ifaddrs structure
 * used for network interface enumeration. On Windows, we use GetAdaptersAddresses()
 * to populate this structure.
 */
struct ifaddrs {
    struct ifaddrs  *ifa_next;       /* Next item in list */
    char            *ifa_name;       /* Name of interface */
    unsigned int     ifa_flags;      /* Flags from SIOCGIFFLAGS */
    struct sockaddr *ifa_addr;       /* Address of interface */
    struct sockaddr *ifa_netmask;    /* Netmask of interface */
    struct sockaddr *ifa_broadaddr;  /* Broadcast address (or dstaddr for P2P) */
    void            *ifa_data;       /* Address-specific data */
};

/* Interface flags - map to Windows equivalents */
#ifndef IFF_UP
#define IFF_UP          0x1     /* Interface is up */
#endif
#ifndef IFF_BROADCAST
#define IFF_BROADCAST   0x2     /* Broadcast address valid */
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK    0x8     /* Is a loopback net */
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING     0x40    /* Resources allocated */
#endif
#ifndef IFF_MULTICAST
#define IFF_MULTICAST   0x1000  /* Supports multicast */
#endif

/**
 * @brief Get network interface addresses (Windows implementation)
 * @param ifap Pointer to store the linked list of interfaces
 * @return 0 on success, -1 on error
 */
static inline int getifaddrs(struct ifaddrs **ifap) {
    DWORD rv, size;
    IP_ADAPTER_ADDRESSES *adapter_addresses = NULL, *aa;
    struct ifaddrs *ifa_head = NULL, *ifa_tail = NULL;

    if (!ifap) return -1;
    *ifap = NULL;

    /* Get adapter addresses - first call to get size */
    size = 16384; /* Initial buffer size */
    adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!adapter_addresses) return -1;

    rv = GetAdaptersAddresses(AF_INET,
                             GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                             NULL, adapter_addresses, &size);

    if (rv == ERROR_BUFFER_OVERFLOW) {
        free(adapter_addresses);
        adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(size);
        if (!adapter_addresses) return -1;
        rv = GetAdaptersAddresses(AF_INET,
                                 GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                                 NULL, adapter_addresses, &size);
    }

    if (rv != NO_ERROR) {
        free(adapter_addresses);
        return -1;
    }

    /* Convert Windows adapter list to ifaddrs linked list */
    for (aa = adapter_addresses; aa != NULL; aa = aa->Next) {
        IP_ADAPTER_UNICAST_ADDRESS *ua;

        /* Process each unicast address for this adapter */
        for (ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            struct ifaddrs *ifa;
            struct sockaddr_in *addr4, *mask4;

            /* Only handle IPv4 for now */
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;

            /* Allocate ifaddrs structure */
            ifa = (struct ifaddrs*)calloc(1, sizeof(struct ifaddrs));
            if (!ifa) goto error_cleanup;

            /* Convert adapter name from wide string to ASCII */
            size_t name_len = wcslen(aa->FriendlyName) + 1;
            ifa->ifa_name = (char*)malloc(name_len);
            if (!ifa->ifa_name) {
                free(ifa);
                goto error_cleanup;
            }
            wcstombs(ifa->ifa_name, aa->FriendlyName, name_len);

            /* Set interface flags */
            ifa->ifa_flags = 0;
            if (aa->OperStatus == IfOperStatusUp) ifa->ifa_flags |= IFF_UP | IFF_RUNNING;
            if (aa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) ifa->ifa_flags |= IFF_LOOPBACK;
            if (aa->Flags & IP_ADAPTER_NO_MULTICAST) ; else ifa->ifa_flags |= IFF_MULTICAST;

            /* Copy address */
            ifa->ifa_addr = (struct sockaddr*)malloc(sizeof(struct sockaddr_in));
            if (!ifa->ifa_addr) {
                free(ifa->ifa_name);
                free(ifa);
                goto error_cleanup;
            }
            memcpy(ifa->ifa_addr, ua->Address.lpSockaddr, sizeof(struct sockaddr_in));

            /* Calculate netmask from prefix length */
            ifa->ifa_netmask = (struct sockaddr*)calloc(1, sizeof(struct sockaddr_in));
            if (!ifa->ifa_netmask) {
                free(ifa->ifa_addr);
                free(ifa->ifa_name);
                free(ifa);
                goto error_cleanup;
            }
            mask4 = (struct sockaddr_in*)ifa->ifa_netmask;
            mask4->sin_family = AF_INET;
            if (ua->OnLinkPrefixLength > 0 && ua->OnLinkPrefixLength <= 32) {
                mask4->sin_addr.s_addr = htonl(~((1 << (32 - ua->OnLinkPrefixLength)) - 1));
            }

            /* Calculate broadcast address */
            ifa->ifa_broadaddr = (struct sockaddr*)calloc(1, sizeof(struct sockaddr_in));
            if (ifa->ifa_broadaddr) {
                struct sockaddr_in *bcast4 = (struct sockaddr_in*)ifa->ifa_broadaddr;
                addr4 = (struct sockaddr_in*)ifa->ifa_addr;
                bcast4->sin_family = AF_INET;
                bcast4->sin_addr.s_addr = addr4->sin_addr.s_addr | ~mask4->sin_addr.s_addr;
            }

            /* Add to linked list */
            ifa->ifa_next = NULL;
            if (ifa_tail) {
                ifa_tail->ifa_next = ifa;
            } else {
                ifa_head = ifa;
            }
            ifa_tail = ifa;
        }
    }

    free(adapter_addresses);
    *ifap = ifa_head;
    return 0;

error_cleanup:
    free(adapter_addresses);
    if (ifa_head) {
        struct ifaddrs *ifa = ifa_head;
        while (ifa) {
            struct ifaddrs *next = ifa->ifa_next;
            free(ifa->ifa_name);
            free(ifa->ifa_addr);
            free(ifa->ifa_netmask);
            free(ifa->ifa_broadaddr);
            free(ifa);
            ifa = next;
        }
    }
    return -1;
}

/**
 * @brief Free ifaddrs structure
 * @param ifa Pointer to ifaddrs linked list
 */
static inline void freeifaddrs(struct ifaddrs *ifa) {
    while (ifa) {
        struct ifaddrs *next = ifa->ifa_next;
        free(ifa->ifa_name);
        free(ifa->ifa_addr);
        free(ifa->ifa_netmask);
        free(ifa->ifa_broadaddr);
        free(ifa);
        ifa = next;
    }
}


/*
 * Scatter/Gather I/O support (struct iovec, struct msghdr)
 * These are POSIX standard but often missing from generic Windows headers
 */
#if !defined(_IOVEC_DEFINED)
#define _IOVEC_DEFINED
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
#endif

#if !defined(_MSGHDR_DEFINED)
#define _MSGHDR_DEFINED
struct msghdr {
    void         *msg_name;       /* optional address */
    socklen_t     msg_namelen;    /* size of address */
    struct iovec *msg_iov;        /* scatter/gather array */
    size_t        msg_iovlen;     /* # elements in msg_iov */
    void         *msg_control;    /* ancillary data, see below */
    size_t        msg_controllen; /* ancillary data buffer len */
    int           msg_flags;      /* flags on received message */
};
#endif

/*
 * sendto/recvfrom wrappers to handle char* casting automatically
 */
static inline int sendto_compat(SOCKET s, const void *buf, int len, int flags, const struct sockaddr *to, int tolen) {
    return sendto(s, (const char *)buf, len, flags, to, tolen);
}

static inline int recvfrom_compat(SOCKET s, void *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
    return recvfrom(s, (char *)buf, len, flags, from, fromlen);
}

/*
 * recvmsg/sendmsg compatibility
 * These map to recvfrom/sendto, assuming a single iovec (common case)
 * This allows code using these POSIX functions to work on Windows without rewriting.
 */
static inline int recvmsg_compat(SOCKET s, struct msghdr *msg, int flags) {
    if (msg->msg_iovlen != 1) {
        // We only support exactly 1 iovec in this shim
        errno = EINVAL;
        return -1;
    }
    int res = recvfrom_compat(s, msg->msg_iov[0].iov_base, (int)msg->msg_iov[0].iov_len, flags, 
                              (struct sockaddr *)msg->msg_name, (int *)&msg->msg_namelen);
    if (res >= 0) {
        msg->msg_iov[0].iov_len = res; // Update length? POSIX recvmsg doesn't usually update iov_len but returns bytes read
    }
    return res;
}

static inline int sendmsg_compat(SOCKET s, const struct msghdr *msg, int flags) {
    if (msg->msg_iovlen != 1) {
        errno = EINVAL;
        return -1;
    }
    // Note: sendto takes const sockaddr*, but msghdr has void* msg_name
    return sendto_compat(s, msg->msg_iov[0].iov_base, (int)msg->msg_iov[0].iov_len, flags, 
                         (const struct sockaddr *)msg->msg_name, (int)msg->msg_namelen);
}

/*
 * Define macros to map standard function names to our compat versions
 * Use #undef first to be safe, though not strictly necessary if they aren't macros.
 */
#define sendto sendto_compat
#define recvfrom recvfrom_compat
#define recvmsg recvmsg_compat
#define sendmsg sendmsg_compat

#endif /* PLATFORM_WINDOWS */

#endif /* WINDOWS_COMPAT_H */
