/**
 * @file bliveq_internal.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-01
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include "utils.h"


typedef struct httpd_handler httpd_handler;


#ifdef __cplusplus
extern "C" {
#endif

blive_errno_t http_create(httpd_handler** handler, const char* ip, uint16_t port);
blive_errno_t http_destroy(httpd_handler* handler);
blive_errno_t http_perform(httpd_handler* handler);
blive_errno_t http_html_injection(httpd_handler* handler, const char* inject_word, void (*callback)(char*, void*), void* context);

#ifdef __cplusplus
}
#endif
#endif