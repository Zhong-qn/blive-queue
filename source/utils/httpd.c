/**
 * @file http.c
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-26
 * 
 * @copyright Copyright (c) 2023
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#endif
#include "httpd.h"
#include "bliveq_internal.h"


#define MAX_INJECTION_NUM   20

typedef enum {
    HTTP_HOME,
    HTTP_NOTFOUND,
} http_file;

typedef struct {
    const char*   html_label_name;
    void    (*callback)(char* dst, void* context);
    void*   context;
} http_inject_unit;

struct httpd_handler {
    fd_t                httpd_socket;
    uint32_t            html_cur_inject_num;
    http_inject_unit    injection_list[MAX_INJECTION_NUM];
};


static const struct {
    char*   request_path;
    char*   filepath;
    char*   content_type;
    char*   status_line;
} httpfile_map[] = {
    {"/",       "./config/htdocs/index.html",      "text/html",     "HTTP/1.0 200 OK\r\n"},
    {"",        "./config/htdocs/404.html",        "text/html",     "HTTP/1.0 404 Not Found\r\n"},
};


static int get_filename(char* buf);
static blive_errno_t http_sendfile(fd_t fd, http_file file, httpd_handler* handler);
static void do_html_inject(char* dst, httpd_handler* handler);



blive_errno_t http_create(httpd_handler** handler, const char* ip, uint16_t port)
{
    httpd_handler*      new_httpd = NULL;
    struct sockaddr_in  saddr;
    int                 res = 0;

    if (handler == NULL) {
        return BLIVE_ERR_NULLPTR;
    }
    new_httpd = zero_alloc(sizeof(httpd_handler));
    if (new_httpd == NULL) {
        return BLIVE_ERR_OUTOFMEM;
    }

#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return BLIVE_ERR_UNKNOWN;
    }
#endif
    new_httpd->httpd_socket = socket(AF_INET, SOCK_STREAM, 0);

    /*bind*/
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ip);
    res = bind(new_httpd->httpd_socket, (struct sockaddr*)&saddr, sizeof(saddr));
    if (res == -1) {
        blive_loge("bind error\n");
        return BLIVE_ERR_UNKNOWN;
    }
    /*listen*/
    res = listen(new_httpd->httpd_socket, 10);
    if (res == -1) {
        blive_loge("listen error\n");
        return BLIVE_ERR_UNKNOWN;
    }

    *handler = new_httpd;
    return BLIVE_ERR_OK;
}

blive_errno_t http_destroy(httpd_handler* handler)
{
    if (handler == NULL) {
        return BLIVE_ERR_NULLPTR;
    }

    if (handler->httpd_socket) {
        shutdown(handler->httpd_socket, SHUT_RDWR);
    }
    free(handler);

    return BLIVE_ERR_OK;
}

blive_errno_t http_perform(httpd_handler* handler)
{
    struct sockaddr_in  addr;
    socklen_t           socklen = sizeof(struct sockaddr);
    fd_t                conn_fd = FD_NULL;
    int                 buffer_len = 0;
    char                buffer[1024] = {0};
    int                 file = HTTP_NOTFOUND;

#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return ERROR;
    }
#endif

    while (1) {
        /*接受tcp连接*/
        conn_fd = accept(handler->httpd_socket, (struct sockaddr*)&addr, &socklen);
        blive_logd("recv socket: %d, %d", conn_fd, errno);

        /*读取http请求*/
        buffer_len = fd_read(conn_fd, buffer, sizeof(buffer) - 1);
        if (buffer_len < 0) {
            blive_loge("read %d(%s)!", buffer_len, strerror(errno));
            shutdown(conn_fd, SHUT_RDWR);
            continue;
        }
        blive_logd("%s", buffer);

        /*解析http请求*/
        file = get_filename(buffer);
        if (file < 0) {
            blive_loge("remote closed");
            shutdown(conn_fd, SHUT_RDWR);
            continue;
        }
        blive_logd("get file %d", file);

        /*发送http响应*/
        http_sendfile(conn_fd, file, handler);
        memset(buffer, 0, sizeof(buffer));
        shutdown(conn_fd, SHUT_RDWR);
    }

    return BLIVE_ERR_OK;
}

blive_errno_t http_html_injection(httpd_handler* handler, const char* inject_word, void (*callback)(char*, void*), void* context)
{
    if (handler == NULL || inject_word == NULL || callback == NULL) {
        return BLIVE_ERR_NULLPTR;
    }
    if (handler->html_cur_inject_num == MAX_INJECTION_NUM) {
        return BLIVE_ERR_RESOURCE;
    }

    handler->injection_list[handler->html_cur_inject_num].html_label_name = inject_word;
    handler->injection_list[handler->html_cur_inject_num].callback = callback;
    handler->injection_list[handler->html_cur_inject_num].context = context;
    handler->html_cur_inject_num++;

    return BLIVE_ERR_OK;
}



static int get_filename(char* buf)
{
    char*       s = strtok(buf, " ");
    http_file   file = HTTP_HOME;

    if (s == NULL) {
        blive_loge("request message invalid:\r\n%s", buf);
        return BLIVE_ERR_INVALID;
    }

    blive_logd("request method: %s\n", s);
    s = strtok(NULL," ");
    if (s == NULL) {
        blive_loge("request not include resource name");
        return BLIVE_ERR_NOTEXSIT;
    }

    while (file < HTTP_NOTFOUND) {
        if (!strcmp(httpfile_map[file].request_path, s)) {
            blive_logd("request resource %s found", s);
            return file;
        }
        file++;
    }

    blive_loge("request resource not found [%s]", s);
    return HTTP_NOTFOUND;
}

static blive_errno_t http_sendfile(fd_t fd, http_file file, httpd_handler* handler)
{
    FILE*   fp = NULL;
    char    data[20480] = {0};
    char*   status_line = NULL;
    char*   server_field = "Server: zqn httpd/0.1.0\r\n";
    char    content_type[256] = {0};

    fp = fopen(httpfile_map[file].filepath, "r");
    if (fp == NULL) {
        file = HTTP_NOTFOUND;
        fp = fopen(httpfile_map[HTTP_NOTFOUND].filepath, "r");
    }
    status_line = httpfile_map[file].status_line;
    snprintf(content_type, 255, "Content-Type: %s\r\n", httpfile_map[file].content_type);

    fd_write(fd, status_line, strlen(status_line));
    fd_write(fd, server_field, strlen(server_field));
    fd_write(fd, content_type, strlen(content_type));
    fd_write(fd, "\r\n", 2);

    /*发送html文件，并在其中进行html标签的注入*/
    fgets(data, sizeof(data), fp);
    while (!feof(fp)) {
        /*如果配置了http注入，进行http注入部分的替换*/
        do_html_inject(data, handler);
        fd_write(fd, data, strlen(data));
        fgets(data, sizeof(data), fp);
    }
    do_html_inject(data, handler);
    fd_write(fd, data, strlen(data));

    fclose(fp);
    return BLIVE_ERR_OK;
}

static inline void do_html_inject(char* dst, httpd_handler* handler)
{
    if (!handler->html_cur_inject_num) {
        return ;
    }
    for (int count = 0; count < handler->html_cur_inject_num; count++) {
        if (strstr(dst, handler->injection_list[count].html_label_name) != NULL) {
            memset(dst, 0, strlen(dst));
            handler->injection_list[count].callback(dst, handler->injection_list[count].context);
        }
    }
    return ;
}

