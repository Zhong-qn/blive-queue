/**
 * @file utils.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 工具集
 * @version 0.1
 * @date 2023-01-30
 * 
 * @copyright Copyright (c) 2023
 */

#include <string.h>
#include <stdlib.h>
#include "blive_api/blive_def.h"

#ifndef __UTILS_H__
#define __UTILS_H__


#ifdef max
#undef max
#endif
#define max(a, b) ({ \
    __typeof(a) _a = a;\
    __typeof(b) _b = b;\
    _a > _b ? _a : _b;\
})

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET   fd_t;

#define fd_read(fd, buf, size)      recv((fd), (char*)(buf), (size), 0)
#define fd_write(fd, buf, size)     send((fd), (char*)(buf), (size), 0)
#else
typedef int fd_t;

#define fd_read                     read
#define fd_write                    write
#endif
#define WR_FD(pair_fd)              ((pair_fd)[1])
#define RD_FD(pair_fd)              ((pair_fd)[0])

typedef enum blive_standard_errno_e {
    BLIVE_ERR_NOTEXSIT = -6,   /* 所请求的资源不存在 */
    BLIVE_ERR_RESOURCE = -5,   /* 资源不足，如队列满、数组满等 */
    BLIVE_ERR_OUTOFMEM = -4,   /* 内存不足 */
    BLIVE_ERR_INVALID = -3,    /* 非法的参数或操作 */
    BLIVE_ERR_NULLPTR = -2,    /* 空指针错误 */
    BLIVE_ERR_UNKNOWN = -1,    /* 未知的错误 */
    BLIVE_ERR_OK = 0,          /* 无错误发生 */
} blive_errno_t;


#ifdef __cplusplus
extern "C" {
#endif


static inline void* zero_alloc(size_t size)
{
    void*   mem = malloc(size);

    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
}

#ifdef WIN32
static inline int __stream_socketpair(struct addrinfo* addr_info, SOCKET sock[2])
{
    SOCKET listener, client, server;
    int opt = 1;

    listener = server = client = INVALID_SOCKET;
    listener = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol); //创建服务器socket并进行绑定监听等
    if (INVALID_SOCKET == listener)
        goto fail;

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,(const char*)&opt, sizeof(opt));

    if(SOCKET_ERROR == bind(listener, addr_info->ai_addr, addr_info->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == getsockname(listener, addr_info->ai_addr, (int*)&addr_info->ai_addrlen))
        goto fail;

    if(SOCKET_ERROR == listen(listener, 5))
        goto fail;

    client = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol); //创建客户端socket，并连接服务器

    if (INVALID_SOCKET == client)
        goto fail;

    if (SOCKET_ERROR == connect(client,addr_info->ai_addr,addr_info->ai_addrlen))
        goto fail;

    server = accept(listener, 0, 0);

    if (INVALID_SOCKET == server)
        goto fail;

    closesocket(listener);

    sock[0] = client;
    sock[1] = server;

    return 0;
fail:
    if(INVALID_SOCKET!=listener)
        closesocket(listener);
    if (INVALID_SOCKET!=client)
        closesocket(client);
    return -1;
}

static inline int __dgram_socketpair(struct addrinfo* addr_info,SOCKET sock[2])
{
    SOCKET client, server;
    struct addrinfo addr, *result = NULL;
    const char* address;
    int opt = 1;

    server = client = INVALID_SOCKET;

    server = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol);  
    if (INVALID_SOCKET == server)
        goto fail;

    setsockopt(server, SOL_SOCKET,SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if(SOCKET_ERROR == bind(server, addr_info->ai_addr, addr_info->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == getsockname(server, addr_info->ai_addr, (int*)&addr_info->ai_addrlen))
        goto fail;

    client = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol); 
    if (INVALID_SOCKET == client)
        goto fail;

    memset(&addr,0,sizeof(addr));
    addr.ai_family = addr_info->ai_family;
    addr.ai_socktype = addr_info->ai_socktype;
    addr.ai_protocol = addr_info->ai_protocol;

    if (AF_INET6==addr.ai_family)
        address = "0:0:0:0:0:0:0:1";
    else
        address = "127.0.0.1";

    if (getaddrinfo(address, "0", &addr, &result))
        goto fail;

    setsockopt(client,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt, sizeof(opt));
    if(SOCKET_ERROR == bind(client, result->ai_addr, result->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == getsockname(client, result->ai_addr, (int*)&result->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == connect(server, result->ai_addr, result->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == connect(client, addr_info->ai_addr, addr_info->ai_addrlen))
        goto fail;

    freeaddrinfo(result);
    sock[0] = client;
    sock[1] = server;
    return 0;

fail:
    if (INVALID_SOCKET!=client)
        closesocket(client);
    if (INVALID_SOCKET!=server)
        closesocket(server);
    if (result)
        freeaddrinfo(result);
    return -1;
}

static inline int __socketpair(int family, int type, int protocol, SOCKET* recv)
{
    const char* address;
    struct addrinfo addr_info,*p_addrinfo;
    int result = -1;
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;

    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return ERROR;
    }

    memset(&addr_info, 0, sizeof(addr_info));
    addr_info.ai_family = family;
    addr_info.ai_socktype = type;
    addr_info.ai_protocol = protocol;
    if (AF_INET6==family)
        address = "0:0:0:0:0:0:0:1";
    else
        address = "127.0.0.1";

    if (0 == getaddrinfo(address, "0", &addr_info, &p_addrinfo)){
        if (SOCK_STREAM == type)
            result = __stream_socketpair(p_addrinfo, recv);   //use for tcp
        else if(SOCK_DGRAM == type)
            result = __dgram_socketpair(p_addrinfo, recv);    //use for udp
        freeaddrinfo(p_addrinfo);
    }
    return result;
}

#define _socketpair(pair_fd) __socketpair(AF_INET, SOCK_STREAM, 0, (pair_fd))
#else
#include <sys/socket.h>
#define _socketpair(pair_fd) socketpair(PF_UNIX, SOCK_STREAM, 0, (pair_fd))
#endif

#ifdef __cplusplus
}
#endif
#endif