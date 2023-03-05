/**
 * @file bliveq_internal.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-01
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include "bliveq_internal.h"

typedef struct {
    const char*   html_label_name;
    void    (*callback)(char* dst, void* context);
    void*   context;
} http_inject_unit;


#ifdef __cplusplus
extern "C" {
#endif

blive_errno_t http_perform(blive_queue* entity);
blive_errno_t http_init(blive_queue* entity);
blive_errno_t http_html_injection(const char* inject_word, void (*callback)(char*, void*), void* context);

#ifdef __cplusplus
}
#endif
#endif