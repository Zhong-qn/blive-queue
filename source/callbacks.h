/**
 * @file callbacks.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 声明定义排队姬向BLIVE模块注册的回调函数
 * @version 0.1
 * @date 2023-01-31
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __BLIVE_CALLBACK_H__
#define __BLIVE_CALLBACK_H__

#include "bliveq_internal.h"
#include "blive_api/blive_api.h"

#ifdef __cplusplus
extern "C" {
#endif

blive_errno_t callbacks_init(blive_queue* queue_entity);

/**
 * @brief 在blive-api模块接收到弹幕消息之后的回调函数
 * 
 * @param [in] entity blive-api模块实体
 * @param [in] msg json格式的数据
 * @param [in] queue_entity 运行的排队姬实体
 */
void danmu_callback(blive* entity, const cJSON* msg, void* queue_entity);

/**
 * @brief 在blive-api模块接收到赠送礼物的消息之后的回调函数
 * 
 * @param [in] entity blive-api模块实体
 * @param [in] msg json格式的数据
 * @param [in] queue_entity 运行的排队姬实体
 */
void send_gift_callback(blive* entity, const cJSON* msg, void* queue_entity);

#ifdef __cplusplus
}
#endif
#endif