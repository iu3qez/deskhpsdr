#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <io.h>
    #include <process.h>

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
#endif

#endif // WINDOWS_COMPAT_H
