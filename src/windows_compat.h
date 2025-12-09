#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif

    // Rename SNB to Windows_SNB to prevent conflict with objidl.h typedef
    #define SNB Windows_SNB

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <io.h>
    #include <process.h>
    #include <iphlpapi.h> // For GetAdaptersAddresses

    // Undefine macros from windows.h/wingdi.h that conflict with application enums
    #undef RELATIVE
    #undef ABSOLUTE
    #undef SNB

    // Struct utsname for uname()
    struct utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
    };

    int uname(struct utsname *n);
    void windows_socket_init(void);
    void windows_socket_cleanup(void);



    // Map usleep to Sleep (which takes milliseconds)
    // usleep takes microseconds, so we divide by 1000

    #define usleep(x) Sleep((x)/1000)

    // Stub for Windows compilation to handle missing ifaddrs.h
    struct ifaddrs {
        struct ifaddrs  *ifa_next;
        char            *ifa_name;
        unsigned int     ifa_flags;
        struct sockaddr *ifa_addr;
        struct sockaddr *ifa_netmask;
        struct sockaddr *ifa_broadaddr; /* Broadcast address */
        void            *ifa_data;
    };

    int getifaddrs(struct ifaddrs **ifap);
    void freeifaddrs(struct ifaddrs *ifa);

    #ifndef IFF_UP
    #define IFF_UP 0x1
    #endif
    #ifndef IFF_RUNNING
    #define IFF_RUNNING 0x40
    #endif
    #ifndef IFF_LOOPBACK
    #define IFF_LOOPBACK 0x8
    #endif
    #ifndef IFF_MULTICAST
    #define IFF_MULTICAST 0x1000
    #endif



    // Macro for setsockopt casting
    #define setsockopt(s, level, optname, optval, optlen) setsockopt(s, level, optname, (const char *)(optval), optlen)
    
    // Macro for getsockopt casting
    #define getsockopt(s, level, optname, optval, optlen) getsockopt(s, level, optname, (char *)(optval), optlen)

    // Shim for inet_aton
    // inet_aton returns non-zero if valid, 0 if invalid
    // inet_pton returns 1 if valid, 0 if invalid (or -1 on error)
    #define inet_aton(cp, addr) (inet_pton(AF_INET, cp, addr) == 1)

    // gettimeofday shim
    int gettimeofday(struct timeval *tp, void *tzp);

    // Set socket non-blocking mode
    void set_nonblocking(SOCKET sock, int enable);

    #ifndef min
    #define min(x,y) ((x)<(y)?(x):(y))
    #endif

    #ifndef SO_REUSEPORT
    #define SO_REUSEPORT SO_REUSEADDR
    #endif

    // Map bzero to memset
    #define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

    // Shim for realpath
    #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)

    // Shim for sync
    #define sync() _flushall()

    // Baud rate constants
    #define B4800   4800
    #define B9600   9600
    #define B19200  19200
    #define B38400  38400

    #ifndef SOL_TCP
    #define SOL_TCP IPPROTO_TCP
    #endif
#else
    // POSIX systems
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <sys/utsname.h>
    #include <fcntl.h>
    
    #define windows_socket_init()
    #define windows_socket_cleanup()

    // Abstract fcntl usage for non-blocking
    static inline void set_nonblocking(int sock, int enable) {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) return;
        if (enable) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
#endif

#endif // WINDOWS_COMPAT_H
