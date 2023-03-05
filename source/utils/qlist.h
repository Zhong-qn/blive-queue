/**
 * @file pri_queue.h
 * @author Zhong Qiaoning (691365572@qq.com)
 * @brief 优先级队列。
 * @attention 需要注意到的是，该优先级队列为了保证在插入、删除时的快速进
 * 行，以及消耗的资源最少化，选用的是树型结构的上浮、下沉，因此这只能保证
 * 每次移除的根节点一定是优先级最高的，并不能保证整个数组中按顺序下去是符
 * 合优先级的从大到小的顺序，因此该优先级队列并不提供foreach遍历的接口。
 * @note 仅支持posix标准
 * @version 1.0
 * @date 2022-04-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __UTILS_QLIST_H__
#define __UTILS_QLIST_H__

#include "utils.h"
#include "blive_api/blive_def.h"


typedef enum {
    FLEET_LV_NONE = 0,          /*无*/
    FLEET_LV_GOVERNOR = 1,      /*总督*/
    FLEET_LV_SUPERVISOR = 2,    /*提督*/
    FLEET_LV_CAPTAIN = 3,       /*舰长*/
    FLEET_LV_MAX,
} blive_fleet_level;

typedef struct {
    uint32_t            weight;                                 /*权重*/
    char                danmu_sender_name[DEFAULT_NAME_LEN];    /*昵称*/
    uint32_t            danmu_sender_uid;                       /*uid*/
    blive_fleet_level   fleet_lv;                               /*舰队等级*/
    Bool                is_hostoom_manager;                     /*是否是房管*/
    Bool                fans_price_is_cur_liveroom;             /*粉丝牌是当前直播间的*/
    uint32_t            fans_price_level;                       /*粉丝牌等级*/
    char                fans_price_name[DEFAULT_NAME_LEN];      /*粉丝牌名称*/
} qlist_unit_data;

typedef Bool (*qlist_foreach_cb)(uint32_t anchorage, const qlist_unit_data* data, void* context);

typedef struct blive_qlist blive_qlist;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个权重值实时排队队列
 * 
 * @param [out] qlist 传出队列实体
 * @return blive_errno_t 
 */
blive_errno_t qlist_create(blive_qlist** qlist);

/**
 * @brief 销毁一个权重值实时排队队列
 * 
 * @param [in] qlist 权重值实时排队队列实体 
 * @return blive_errno_t 
 */
blive_errno_t qlist_destroy(blive_qlist* qlist);

/**
 * @brief 在权重值实时排队队列中检索锚定值anchorage是否已在队列中
 * 
 * @param [in] qlist 权重值实时排队队列实体 
 * @param [in] anchorage 锚定值
 * @return Bool 
 */
Bool qlist_anchorage_existence(blive_qlist* qlist, uint32_t anchorage);

/**
 * @brief 向权重值实时排队队列中插入或刷新锚定值对应的权重值
 * 
 * @param [in] qlist 权重值实时排队队列实体
 * @param [in] anchorage 锚定值
 * @param [in] data 权重值
 * @return blive_errno_t 
 */
blive_errno_t qlist_append_update(blive_qlist* qlist, uint32_t anchorage, const qlist_unit_data* data);

/**
 * @brief 在权重值实时排队队列中移除锚定值对应的权重值
 * 
 * @param [in] qlist 权重值实时排队队列实体
 * @param [in] anchorage 锚定值
 * @return blive_errno_t 
 */
blive_errno_t qlist_subtract(blive_qlist* qlist, uint32_t anchorage);

/**
 * @brief qlist遍历处理
 * 
 * @param [in] qlist 权重值实时排队队列实体 
 * @param [in] invert_seq 是否倒序遍历
 * @param [in] cb 回调函数
 * @param [in] context 回调函数的上下文
 * @return blive_errno_t 
 */
blive_errno_t qlist_foreach(blive_qlist* qlist, Bool invert_seq, qlist_foreach_cb cb, void* context);

#ifdef __cplusplus
}
#endif
#endif

