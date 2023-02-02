/**
 * @file blive_queue.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-01
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __BLIVE_QUEUE_H__
#define __BLIVE_QUEUE_H__

#include "config.h"
#include "pri_queue.h"

typedef struct {
    blive_ext_cfg   conf;
    fd_t            queue_handler_fd[2];
    pri_queue_t*    queue;
} blive_queue;

#endif