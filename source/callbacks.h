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


#include "blive_api/blive_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在blive-api模块接收到弹幕消息之后的回调函数
 * 
 * @param [in] entity blive-api模块实体
 * @param [in] msg json格式的数据
 * @param [in] config 运行的配置结构体
 */
void danmu_callbacks(blive* entity, const cJSON* msg, void* config);

#ifdef __cplusplus
}
#endif
#endif