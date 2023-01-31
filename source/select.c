/**
 * @file select_engine.c
 * @author Zhong Qiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#endif

#include "hash.h"
#include "pri_queue.h"
#include "select.h"


#define PRI_QUEUE_SIZE      50


typedef enum {
    ENGINE_EVENT_RESTART,
    ENGINE_EVENT_STOP,
} engine_manage_event_t;

typedef struct {
    select_schedule_cb  cb;
    void*               context;
    struct timeval      time;
} engine_event_t;

typedef struct {
    fd_t             fd;
    select_fd_cb     cb;
    Bool           temporary;
    void*               context;
} engine_fd_t;

struct select_engine_t {
    pri_queue_t      *event_queue;
    hash_t           *fd_poll;
    fd_set              read_fds;
#ifndef WIN32
    fd_t             max_fd;
#endif
    Bool           need_continue;
    Bool           has_reset;
    pthread_mutex_t     running_flag;   /* 是否在运行的标志位 */
    fd_t             manage_pipe[2];
};


static void __engine_destroy(select_engine_t* engine);
static uint32_t __fd_poll_hash_func(const char *key);
static Bool __event_queue_pri_comp(void* event1, void* event2);
static Bool __fd_set_foreach(const char *key, const void* value, void* context);
static Bool __fd_isset_foreach(const char *key, const void* value, void* context);
static void __manage_fd_callback(fd_t manage_fd, void* context);
static void __engine_reload(select_engine_t* engine);
static int __engine_fd_add(select_engine_t* engine, fd_t fd, select_fd_cb callback, void* context, Bool temporary);
int __engine_fd_del(select_engine_t* engine, fd_t fd);
#ifdef WIN32
#include <ws2tcpip.h>

static int __stream_socketpair(struct addrinfo* addr_info, SOCKET sock[2])
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

static int __dgram_socketpair(struct addrinfo* addr_info,SOCKET sock[2])
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

static int _socketpair(int family, int type, int protocol, SOCKET* recv)
{
    const char* address;
    struct addrinfo addr_info,*p_addrinfo;
    int result = -1;

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
#endif


blive_errno_t select_engine_create(select_engine_t **engine)
{
    int          retval = BLIVE_ERR_OK;
    select_engine_t  *new_engine = NULL;

    if (engine == NULL) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    new_engine = zero_alloc(sizeof(select_engine_t));
    if (new_engine == NULL) {
        retval = BLIVE_ERR_OUTOFMEM;
        goto _out;
    }
    memset(new_engine, 0, sizeof(select_engine_t));


    /* 创建优先级队列作为事件队列 */
    new_engine->event_queue = pri_queue_create(PRI_QUEUE_SIZE, True, __event_queue_pri_comp);
    if (new_engine->event_queue == NULL) {
        retval = BLIVE_ERR_UNKNOWN;
        goto _destroy;
    }

    /* 创建哈希表作为fd池 */
    retval = hash_create(&new_engine->fd_poll, 100, __fd_poll_hash_func);
    if (retval != BLIVE_ERR_OK) {
        goto _destroy;
    }

    new_engine->need_continue = True;
    pthread_mutex_init(&new_engine->running_flag, NULL);

#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;

    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return ERROR;
    }
    _socketpair(AF_INET, SOCK_STREAM, 0, new_engine->manage_pipe);
     // printf("pair socket is [%d, %d]", new_engine->manage_pipe[0], new_engine->manage_pipe[1]);
#else
    pipe(new_engine->manage_pipe);
#endif
    __engine_fd_add(new_engine, new_engine->manage_pipe[0], __manage_fd_callback, new_engine, False);

    *engine = new_engine;
_out:
    return retval;

_destroy:
    __engine_destroy(new_engine);
    goto _out;
}

blive_errno_t select_engine_fd_add_forever(select_engine_t* engine, fd_t fd, select_fd_cb callback, void* context)
{
    int              retval = BLIVE_ERR_OK;

    if (engine == NULL || callback == NULL || fd < 0) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }
     // printf("select engine add a fd=%d\n", fd);
    retval = __engine_fd_add(engine, fd, callback, context, False);
    __engine_reload(engine);    /* 通知engine重新进行select */

_out:
    return retval;
}

blive_errno_t select_engine_fd_add_once(select_engine_t* engine, fd_t fd, select_fd_cb callback, void* context)
{
    int              retval = BLIVE_ERR_OK;

    if (engine == NULL || callback == NULL || fd < 0) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }
    retval = __engine_fd_add(engine, fd, callback, context, True);
    __engine_reload(engine);    /* 通知engine重新进行select */

_out:
    return retval;
}

blive_errno_t select_engine_fd_del(select_engine_t* engine, fd_t fd)
{
    int              retval = BLIVE_ERR_OK;

    if (engine == NULL || fd < 0) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    __engine_fd_del(engine, fd);
    __engine_reload(engine);    /* 通知engine重新进行select */

_out:
    return retval;
}

blive_errno_t select_engine_schedule_add(select_engine_t* engine, select_schedule_cb callback, void* context, int64_t timeous)
{
    int      retval = BLIVE_ERR_OK;
    engine_event_t  *event = NULL;

    if (engine == NULL || callback == NULL || timeous < 0) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    event = zero_alloc(sizeof(engine_event_t));
    event->cb = callback;
    event->context = context;
    gettimeofday(&event->time, NULL);
     // printf("current time:  %lds:%ldus\n", event->time.tv_sec, event->time.tv_usec);

    event->time.tv_sec += timeous / (1000 * 1000);
    event->time.tv_usec += timeous % (1000 * 1000);
    if (event->time.tv_usec < 0) {
        event->time.tv_usec += 1000 * 1000;
        event->time.tv_sec -= 1;
    }
    if (event->time.tv_sec < 0) {
        event->time.tv_sec = 0;
        event->time.tv_usec = 0;
    }

     // printf("set timeout event %lds:%ldus\n", event->time.tv_sec, event->time.tv_usec);;
    pri_queue_push(engine->event_queue, event);
    __engine_reload(engine);    /* 通知engine重新进行select */

_out:
    return retval;
}

blive_errno_t select_engine_run(select_engine_t* engine)
{
    struct timeval  tm_wait;
    struct timeval* select_tm = NULL;
    engine_event_t  *event = NULL;
    int      retval = BLIVE_ERR_OK;
    int32_t         select_ret = 0;

    if (engine == NULL) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    while (engine->need_continue) {
        /*
            上锁的目的只是为了标志此时select引擎正在运行。此时任何调整select引擎的动作都
            必须进行引擎的reload，以保证该调整会立即生效
         */
        pthread_mutex_lock(&engine->running_flag);

        /* 初始化需要监听的文件描述符 */
        FD_ZERO(&engine->read_fds);
        hash_foreach(engine->fd_poll, __fd_set_foreach, engine);

        /* 获取等待的时间 */
        retval = pri_queue_peek(engine->event_queue, (void**)&event);
        if (retval == BLIVE_ERR_RESOURCE) {   /* 说明此时没有事件需要处理 */
            select_tm = NULL;
             // printf("no need to wait.\n");
        } else if (retval != BLIVE_ERR_OK) {
            retval = BLIVE_ERR_UNKNOWN;       /* 出错了 */
             // printf("error occured!\n");
            goto _out;
        } else {                            /* 说明此时有事件需要处理 */
            gettimeofday(&tm_wait, NULL);
             // printf("current time:  %lds:%ldus\n", tm_wait.tv_sec, tm_wait.tv_usec);
             // printf("event time:  %lds:%ldus\n", event->time.tv_sec, event->time.tv_usec);
            tm_wait.tv_sec = event->time.tv_sec - tm_wait.tv_sec;
            tm_wait.tv_usec = event->time.tv_usec - tm_wait.tv_usec;
            if (tm_wait.tv_usec < 0) {
                tm_wait.tv_usec += 1000 * 1000;
                tm_wait.tv_sec -= 1;
            }
            if (tm_wait.tv_sec < 0) {
                tm_wait.tv_sec = 0;
                tm_wait.tv_usec = 0;
            }
             // printf("waiting for timeout...%lds:%ldus\n", tm_wait.tv_sec, tm_wait.tv_usec);
            select_tm = &tm_wait;
        }

#ifdef WIN32
        select_ret = select(0, &engine->read_fds, NULL, NULL, select_tm);
#else
        select_ret = select(engine->max_fd + 1, &engine->read_fds, NULL, NULL, select_tm);
#endif
         // printf("select_ret=%d\n", select_ret);

        /* 解锁，此后将会执行回调函数。此时调整select引擎，则不需要进行reload */
        pthread_mutex_unlock(&engine->running_flag);

        /* 定时器事件处理 */
        if (!select_ret) {  
            event->cb(event->context);
            retval = pri_queue_pop_trywait(engine->event_queue, (void**)&event);
            if (retval != BLIVE_ERR_OK) {
                break;
            }
        /* fd可读 */
        } else if (select_ret > 0) {
            hash_foreach(engine->fd_poll, __fd_isset_foreach, engine);
        /* 被中断程序打断 */
        } else {
             // printf("select has been interrupted by system call.(%s)\n", strerror(errno));
        }
    }

    engine->need_continue = True;
_out:
    return retval;
}

blive_errno_t select_engine_stop(select_engine_t* engine)
{
    int              retval = BLIVE_ERR_OK;
    engine_manage_event_t   event = ENGINE_EVENT_STOP;

    if (engine == NULL) {
        retval = BLIVE_ERR_NULLPTR;
        goto _out;
    }

    write(engine->manage_pipe[1], &event, sizeof(engine_manage_event_t));

_out:
    return retval;
}

blive_errno_t select_engine_destroy(select_engine_t* engine)
{
    int              retval = BLIVE_ERR_OK;

    if (engine == NULL) {
        retval = BLIVE_ERR_NULLPTR;
        goto _out;
    }

    if (engine->need_continue) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    __engine_destroy(engine);

_out:
    return retval;
}




static void __engine_reload(select_engine_t* engine)
{
    engine_manage_event_t   mgt_event = ENGINE_EVENT_RESTART;

    if (!pthread_mutex_trylock(&engine->running_flag)) {
        /* 如果能拿到锁，这说明此时程序不在运行，解锁 */
        pthread_mutex_unlock(&engine->running_flag);
    } else {
        /* 如果拿不到锁，这说明此时程序正在运行，通知engine重新进行select */
        write(engine->manage_pipe[1], &mgt_event, sizeof(engine_manage_event_t));
    }

    return ;
}

static blive_errno_t __engine_fd_add(select_engine_t* engine, fd_t fd, select_fd_cb callback, void* context, Bool temporary)
{
    blive_errno_t         retval = BLIVE_ERR_OK;
    char            buffer[32] = {0};
    engine_fd_t*    engine_fd = NULL;

    engine_fd = zero_alloc(sizeof(engine_fd_t));
    if (engine_fd == NULL) {
        retval = BLIVE_ERR_OUTOFMEM;
        goto _out;
    }

    engine_fd->fd = fd;
    engine_fd->cb = callback;
    engine_fd->temporary = temporary;
    engine_fd->context = context;

#ifdef WIN32 
    snprintf(buffer, 31, "%I64d", fd);
#else
    snprintf(buffer, 31, "%d", fd);
#endif
    hash_push(engine->fd_poll, buffer, engine_fd);

_out:
    return retval;
}

blive_errno_t __engine_fd_del(select_engine_t* engine, fd_t fd)
{
    blive_errno_t                 retval = BLIVE_ERR_OK;
    char                    buffer[32] = {0};

#ifdef WIN32 
    snprintf(buffer, 31, "%I64d", fd);
#else
    snprintf(buffer, 31, "%d", fd); 
#endif

    if (hash_pop(engine->fd_poll, buffer) == NULL) {
        retval = BLIVE_ERR_NOTEXSIT;
    }

    return retval;
}

static inline void __engine_destroy(select_engine_t* engine)
{
    if (engine != NULL) {
        if (engine->event_queue != NULL) {
            pri_queue_destroy(engine->event_queue);
            engine->event_queue = NULL;
        }
        if (engine->fd_poll != NULL) {
            hash_destroy(engine->fd_poll);
        }
        pthread_mutex_destroy(&engine->running_flag);
        free(engine);
    }
}

static Bool __event_queue_pri_comp(void* event1, void* event2)
{
    int64_t    tmp_s = 0;
    int64_t    tmp_us = 0;

    tmp_s = ((engine_event_t*)event1)->time.tv_sec - ((engine_event_t*)event2)->time.tv_sec;
    tmp_us = ((engine_event_t*)event1)->time.tv_usec - ((engine_event_t*)event2)->time.tv_usec;

    if ((tmp_s < 0) || (!tmp_s && tmp_us <= 0)) {
        return True;
    } else {
        return False;
    }
}

static uint32_t __fd_poll_hash_func(const char *key)
{
    return (uint32_t)atoi(key);
}

static int32_t __fd_readable(fd_t fd)
{
    unsigned long int read_len = 0;

    /* 获取输入缓冲区大小 */
#ifdef WIN32
    ioctlsocket(fd, FIONREAD, &read_len);
#else
    ioctl(fd, FIONREAD, &read_len);
#endif
    return (int32_t)read_len;
}

static Bool __fd_set_foreach(const char *key, const void* value, void* context)
{
    Bool               retval = True;
    select_engine_t*     engine = (select_engine_t*)context;
    engine_fd_t*            engine_fd = (engine_fd_t*)value;

    if (context == NULL || value == NULL) {
        retval = False;
        goto _out;
    }

#ifndef WIN32
    engine->max_fd = max(engine->max_fd, engine_fd->fd);
#endif
    FD_SET(engine_fd->fd, &engine->read_fds);

_out:
    return retval;
}

static void __manage_fd_callback(fd_t manage_fd, void* context)
{
    engine_manage_event_t       event;
    select_engine_t*         engine = (select_engine_t*)context;

    while (__fd_readable(manage_fd)) {
        read(manage_fd, &event, sizeof(engine_manage_event_t));
         // printf("process manage message %d\n", event);
        switch (event) {
            case ENGINE_EVENT_STOP:
                engine->need_continue = False;
                break;
            case ENGINE_EVENT_RESTART:
            default:
                break;
        }
    }
    return ;
}

static Bool __fd_isset_foreach(const char *key, const void* value, void* context)
{
    Bool               retval = True;
    select_engine_t*     engine = (select_engine_t*)context;
    engine_fd_t*            engine_fd = (engine_fd_t*)value;

    if (context == NULL || value == NULL) {
        retval = False;
        goto _out;
    }

    if (FD_ISSET(engine_fd->fd, &engine->read_fds)) {
        if (engine_fd->temporary) {             /* 如果fd是只执行一次的，则从哈希表中移除 */
            char            buffer[32] = {0};

#ifdef WIN32 
            snprintf(buffer, 31, "%I64d", engine_fd->fd);
#else
            snprintf(buffer, 31, "%d", engine_fd->fd);
#endif
            hash_pop(engine->fd_poll, buffer);
        }

        engine_fd->cb(engine_fd->fd, engine_fd->context);   /* 如果fd可读，则执行回调 */
    }

_out:
    return retval;
}
