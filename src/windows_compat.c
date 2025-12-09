#include "windows_compat.h"
#include <stdio.h>
#include <stdlib.h>

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

void freeifaddrs(struct ifaddrs *ifa) {
    struct ifaddrs *next;
    while (ifa != NULL) {
        next = ifa->ifa_next;
        if (ifa->ifa_name) free(ifa->ifa_name);
        if (ifa->ifa_addr) free(ifa->ifa_addr);
        if (ifa->ifa_netmask) free(ifa->ifa_netmask);
        if (ifa->ifa_broadaddr) free(ifa->ifa_broadaddr);
        free(ifa);
        ifa = next;
    }
}

int getifaddrs(struct ifaddrs **ifap) {
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
            if (!(aa->Flags & IP_ADAPTER_NO_MULTICAST)) ifa->ifa_flags |= IFF_MULTICAST;

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
                struct sockaddr_in *addr4_ptr = (struct sockaddr_in*)ifa->ifa_addr;
                bcast4->sin_family = AF_INET;
                bcast4->sin_addr.s_addr = addr4_ptr->sin_addr.s_addr | ~mask4->sin_addr.s_addr;
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
    if (adapter_addresses) free(adapter_addresses);
    freeifaddrs(ifa_head);
    return -1;
}
#endif
