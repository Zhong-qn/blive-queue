/**
 * @file httpd.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 简易的http服务端，使用html注入来允许修改html页面内容
 * @version 0.1
 * @date 2023-03-05
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

/**
 * @brief 在指定的IP、端口创建http服务端处理
 * 
 * @param [out] handler http服务端实体
 * @param [in] ip 使用的IP
 * @param [in] port 使用的端口
 * @return blive_errno_t 
 */
blive_errno_t http_create(httpd_handler** handler, const char* ip, uint16_t port);

/**
 * @brief 删除一个http服务端实体
 * 
 * @param [in] handler http服务端实体
 * @return blive_errno_t 
 */
blive_errno_t http_destroy(httpd_handler* handler);

/**
 * @brief 运行http服务端
 * 
 * @param [in] handler http服务端实体 
 * @return blive_errno_t 
 */
blive_errno_t http_perform(httpd_handler* handler);

/**
 * @brief 在html页面文件中出现关键字的时候注入指定内容
 * 
 * @param [in] handler http服务端实体  
 * @param [in] inject_word 注入的关键字
 * @param [in] callback 注入触发时的回调函数
 * @param [in] context 注入触发时的回调函数的参数
 * @return blive_errno_t 
 */
blive_errno_t http_html_injection(httpd_handler* handler, const char* inject_word, void (*callback)(char*, void*), void* context);

#ifdef __cplusplus
}
#endif
#endif