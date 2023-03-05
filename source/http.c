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
#include "http_server.h"
#include "bliveq_internal.h"


#define MAX_INJECTION_NUM   20

typedef enum {
    HTTP_HOME,
    HTTP_NOTFOUND,
} http_file;

static struct {
    char*   request_path;
    char*   filepath;
    char*   content_type;
    char*   status_line;
} httpfile_map[] = {
    {"/",       "./config/htdocs/index.html",      "text/html",     "HTTP/1.0 200 OK\r\n"},
    {"",        "./config/htdocs/404.html",        "text/html",     "HTTP/1.0 404 Not Found\r\n"},
};

static int                  html_cur_inject_num = 0;
static http_inject_unit     injection_list[MAX_INJECTION_NUM];


static int get_filename(char* buf);
static blive_errno_t http_sendfile(fd_t fd, http_file file);

static void do_html_inject(char* dst);
static void refresh_inject(char* dst, void* context);
static blive_errno_t html_injection(const char* inject_word, void (*callback)(char*, void*), void* context);


blive_errno_t http_init(blive_queue* entity)
{
    struct sockaddr_in saddr;
    int res = 0;
#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return ERROR;
    }
#endif
    entity->http_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8899);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    res = bind(entity->http_fd, (struct sockaddr*)&saddr, sizeof(saddr));
    if (res == -1) {
        blive_loge("bind error\n");
        return BLIVE_ERR_UNKNOWN;
    }
    res = listen(entity->http_fd, 5);
    if (res == -1) {
        blive_loge("listen error\n");
        return BLIVE_ERR_UNKNOWN;
    }

    // html_injection("<queuelist_injection>", NULL, entity);
    html_injection("__refresh_injection__", refresh_inject, NULL);

    return BLIVE_ERR_OK;
}

blive_errno_t http_perform(blive_queue* entity)
{
    struct sockaddr_in  addr;
    socklen_t   socklen = sizeof(struct sockaddr);
    fd_t        conn_fd = FD_NULL;
    int         buffer_len = 0;
    char        buffer[1024] = {0};
#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return ERROR;
    }
#endif

    while (1) {
        int   file = HTTP_NOTFOUND;

        conn_fd = accept(entity->http_fd, (struct sockaddr*)&addr, &socklen);
        blive_logd("recv socket: %d, %d", conn_fd, errno);

        buffer_len = fd_read(conn_fd, buffer, sizeof(buffer) - 1);
        if (buffer_len < 0) {
            blive_loge("read %d(%s)!", buffer_len, strerror(errno));
            break;
        }
        blive_logd("%s", buffer);
        file = get_filename(buffer);
        if (file < 0) {
            blive_loge("remote closed");
            break;
        }
        blive_logd("get file %d", file);
        http_sendfile(conn_fd, file);
        memset(buffer, 0, sizeof(buffer));
        shutdown(conn_fd, SHUT_RDWR);
    }

    return BLIVE_ERR_OK;

}

blive_errno_t http_html_injection(const char* inject_word, void (*callback)(char*, void*), void* context)
{
    if (inject_word == NULL || callback == NULL) {
        return BLIVE_ERR_NULLPTR;
    }

    return html_injection(inject_word, callback, context);
}

static blive_errno_t html_injection(const char* inject_word, void (*callback)(char*, void*), void* context)
{
    if (html_cur_inject_num == MAX_INJECTION_NUM) {
        return BLIVE_ERR_RESOURCE;
    }

    injection_list[html_cur_inject_num].html_label_name = inject_word;
    injection_list[html_cur_inject_num].callback = callback;
    injection_list[html_cur_inject_num].context = context;
    html_cur_inject_num++;
    return BLIVE_ERR_OK;
}

static int get_filename(char* buf)
{
    char*       s = strtok(buf, " ");
    http_file   file = HTTP_HOME;

    if (s == NULL) {
        blive_loge("request message invalid");
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

static blive_errno_t http_sendfile(fd_t fd, http_file file)
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

    fgets(data, sizeof(data), fp);
    while (!feof(fp)) {
        /*如果配置了http注入，进行http注入部分的替换*/
        do_html_inject(data);
        fd_write(fd, data, strlen(data));
        fgets(data, sizeof(data), fp);
    }

    fclose(fp);
    
    return BLIVE_ERR_OK;
}

static inline void do_html_inject(char* dst)
{
    if (html_cur_inject_num) {
        for (int count = 0; count < html_cur_inject_num; count++) {
            if (strstr(dst, injection_list[count].html_label_name) != NULL) {
                memset(dst, 0, strlen(dst));
                injection_list[count].callback(dst, injection_list[count].context);
            }
        }
    }
    return ;
}

static void refresh_inject(char* dst, void* context)
{
    if (context != NULL) {
        return ;
    }
    strcat(dst, "<script>function aoto_refresh(){window.location.reload();};setTimeout('aoto_refresh()',2000);</script>\r\n");
    return ;
}

