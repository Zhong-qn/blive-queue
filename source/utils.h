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
typedef SOCKET   fd_t;
#else
typedef int fd_t;
#endif

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

#ifdef __cplusplus
}
#endif
#endif