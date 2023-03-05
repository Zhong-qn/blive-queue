/**
 * @file hash.h
 * @author Zhong Qiaoning (691365572@qq.com)
 * @brief 哈希表
 * @version 0.1
 * @date 2022-07-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __UTILS_HASH_H__
#define __UTILS_HASH_H__

#include "blive_api/blive_def.h"

/**
 * @brief 哈希表的描述结构
 * 
 */
typedef struct hash_t hash_t;


/**
 * @brief 计算哈希值的哈希函数
 * 
 * @param [in] key 元素的键
 * @return 返回哈希值
 */
typedef uint32_t (*hash_func)(const char *key);

/**
 * @brief 遍历哈希表所有元素使用的回调函数，通过调用hash_foreach来遍历整个哈希表，
 *        并依次将哈希表内元素传递给该回调函数
 * 
 * @param [in] key 存入哈希表的元素的键
 * @param [in] value 存入哈希表的元素的值
 * @param [in] context 回调者依赖的上下文
 * @return 返回CR_UT_FALSE将会停止遍历立即结束
 */
typedef Bool (*hash_cb)(const char *key, const void* value, void* context);


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief 创建一个哈希表
 * 
 * @param [out] pht 传出参数
 * @param [in] size 创建的哈希表的bucket大小
 * @param [in] hash_func 计算哈希值的哈希函数，如果传入NULL则会使用内置的默认哈希函数
 * @return int 
 */
int hash_create(hash_t **pht, uint32_t size, hash_func hash_func);


/**
 * @brief 销毁一个哈希表
 * 
 * @param [in] ht 待销毁的哈希表
 */
int hash_destroy(hash_t *ht);

/**
 * @brief 将一个数据以及与他相关联的键组成一个键值对放进哈希表中
 * 
 * @param [in] ht 哈希表的描述结构
 * @param [in] key 存入哈希表的元素的键
 * @param [in] val 存入哈希表的元素的值
 * @return void* 如果发生碰撞，将会返回碰撞的元素的值，否则返回NULL
 */
void* hash_push(hash_t *ht, const char *key, const void* val);

/**
 * @brief 获取在哈希表中与键相关联的元素的值
 * 
 * @param [in] ht 哈希表的描述结构
 * @param [in] key 哈希表中元素的键
 * @return void* 返回哈希表键元素对应的值
 */
void* hash_peek(hash_t *ht, const char *key);

/**
 * @brief 从哈希表中移除与键相关联的元素
 * 
 * @param [in] ht 哈希表的描述结构
 * @param [in] key 哈希表中元素的键
 * @return void* 返回哈希表键元素对应的值
 */
void* hash_pop(hash_t *ht, const char *key);

/**
 * @brief 获取哈希表中保存的元素的数量
 * 
 * @param [in] ht 哈希表的描述结构
 * @return int32_t 返回哈希表中键值对元素的数量
 */
int32_t hash_count(hash_t *ht);

/**
 * @brief 遍历整个哈希表，并依次将哈希表内元素传递给该回调函数，迭代
 * 
 * @param [in] ht 哈希表的描述结构
 * @param [in] callback 遍历哈希表所有元素使用的回调函数（迭代器）
 * @param [in] context 回调者依赖的上下文
 */
void hash_foreach(hash_t *ht, hash_cb callback, void* context);

#ifdef __cplusplus
}
#endif
#endif