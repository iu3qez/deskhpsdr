#include "windows_compat.h"
#include <stdio.h>

#ifdef _WIN32
int uname(struct utsname *n) {
    if (n == NULL) return -1;
    snprintf(n->sysname, sizeof(n->sysname), "Windows");
    snprintf(n->release, sizeof(n->release), "10.0"); 
    snprintf(n->version, sizeof(n->version), "10");
    snprintf(n->machine, sizeof(n->machine), "x86_64");
    snprintf(n->nodename, sizeof(n->nodename), "PC");
    return 0;
}

void windows_socket_init(void) {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        exit(1);
    }
}

void windows_socket_cleanup(void) {
    WSACleanup();
}
#endif
