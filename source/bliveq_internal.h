/**
 * @file bliveq_internal.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-01
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __BLIVE_QUEUE_INTERNAL_H__
#define __BLIVE_QUEUE_INTERNAL_H__

#include "config.h"
#include "pri_queue.h"
#include "qlist.h"
#include "select.h"
#include "blive_api/blive_api.h"


typedef struct {
    blive_ext_cfg       conf;
    fd_t                http_fd;            /*HTTP服务端使用的socket文件描述符*/
    fd_t                qlist_fd[2];
    select_engine_t*    engine;
    blive_qlist*        qlist;
    pri_queue_t*        queue;
} blive_queue;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif
#endif