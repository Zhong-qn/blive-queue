/**
 * @file queue_list.c
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 排队姬的排队队列实现
 * @version 0.1
 * @date 2023-02-12
 * 
 * @copyright Copyright (c) 2023
 */

#include "bliveq_internal.h"
#include "qlist.h"


#define GET_PREV_UNIT(unit)     list_entry((unit)->list_node.prev, qlist_unit, list_node)
#define GET_NEXT_UNIT(unit)     list_entry((unit)->list_node.next, qlist_unit, list_node)

#define UNIT_SUBTRACT(dst_unit) LIST_SUBTRACT(&(dst_unit)->list_node)

#define UNIT_APPEND_REAR(dst_unit, append_unit)     LIST_APPEND_REAR(&(dst_unit)->list_node, &(append_unit)->list_node)
#define UNIT_APPEND_AHEAD(dst_unit, append_unit)    LIST_APPEND_AHEAD(&(dst_unit)->list_node, &(append_unit)->list_node)


typedef struct {
    uint32_t        anchorage;  /*唯一的锚定值，用于区分不同的单元*/
    qlist_unit_data data;       /*节点的数据*/
    list            list_node;  /*链表控制节点*/
} qlist_unit;   /*队列链表中存放的最基础的单元*/

struct blive_qlist {
    uint32_t        elem_num;       /*qlist中的单元数量*/
    pthread_mutex_t lock;           /*多线程下安全锁*/
    list            list_head;      /*存储单元的环形链表*/
    qlist_unit*     tmp_unit;       /*暂存的单元*/
};


static qlist_unit* qlist_search(blive_qlist* qlist, uint32_t anchorage)
{
    list*       list_ptr = &qlist->list_head;
    qlist_unit* final = NULL;

    /*qlist为空，直接返回*/
    if (!qlist->elem_num) {
        return NULL;
    }
    if (qlist->tmp_unit != NULL && qlist->tmp_unit->anchorage == anchorage) {
        return qlist->tmp_unit;
    }

    /*qlist不为空，遍历链表查找锚定值是否有相同的，如果有返回找到的单元*/
    for (int count = 0; count < qlist->elem_num; count++) {
        list_ptr = list_ptr->prev;
        final = list_entry(list_ptr, qlist_unit, list_node);
        if (anchorage == final->anchorage) {
            /**
             * 如果查询时找到了锚定值相同的单元，那么先暂存起来，目的是为了避免频繁的
             * 查询，如果再次查询时锚定值与上一次查询的相同，则直接返回上次查询的结果
             * 
             */
            qlist->tmp_unit = final;
            return final;
        }
    }
    return NULL;
}

static blive_errno_t qlist_append(blive_qlist* qlist, qlist_unit* append_unit)
{
    qlist_unit* tail_unit = NULL;
    qlist_unit* front_unit = NULL;
    qlist_unit* begin_unit = NULL;
    qlist_unit* end_unit = NULL;
    qlist_unit* search_unit = NULL;

    /*如果链表为空，直接插入到链表尾部*/
    if (!qlist->elem_num) {
        LIST_APPEND_REAR(&qlist->list_head, &append_unit->list_node);
        return BLIVE_ERR_OK;
    }
    tail_unit = list_entry(qlist->list_head.prev, qlist_unit, list_node);
    front_unit = list_entry(qlist->list_head.next, qlist_unit, list_node);

    /*如果头节点的权重小于添加节点的权重，则将添加节点挂在原头节点的前面*/
    if (front_unit->data.weight < append_unit->data.weight) {
        UNIT_APPEND_AHEAD(front_unit, append_unit);
        return BLIVE_ERR_OK;
    }
    /*如果尾节点大于等于添加节点的权重，则将添加节点挂在原尾节点的后面*/
    if (tail_unit->data.weight >= append_unit->data.weight) {
        UNIT_APPEND_REAR(tail_unit, append_unit);
        return BLIVE_ERR_OK;
    }

    /**
     * @brief 确认插入单元应该插入链表的哪个位置。计算链表的遍历范围，
     * 暂存的临时单元肯定在链表的头与尾之间，增加临时单元与添加单元的
     * 比较，在链表中数据较多时能提升一定的链表查询效率
     */
    begin_unit = tail_unit;
    end_unit = front_unit;
    if (qlist->tmp_unit != NULL) {
        if (qlist->tmp_unit->data.weight > append_unit->data.weight) {
            end_unit = qlist->tmp_unit;
        } else if (qlist->tmp_unit->data.weight < append_unit->data.weight) {
            begin_unit = qlist->tmp_unit;
        }
    }

    /*从小到大开始，检索[begin_unit, end_unit)的左开右闭区间*/
    search_unit = begin_unit;
    while (search_unit != end_unit) {
        if (search_unit->data.weight >= append_unit->data.weight) {
            UNIT_APPEND_REAR(search_unit, append_unit);
            return BLIVE_ERR_OK;
        }
        search_unit = GET_PREV_UNIT(search_unit);
    }
    
    /*如果在[begin_unit, end_unit)的区间内未找到可以插入的点，检测是否可以插入end_unit后方*/
    if (end_unit->data.weight > append_unit->data.weight) {
        UNIT_APPEND_REAR(end_unit, append_unit);
        return BLIVE_ERR_OK;
    }
    return BLIVE_ERR_UNKNOWN;
}


blive_errno_t qlist_create(blive_qlist** qlist)
{
    if (qlist == NULL) {
        return BLIVE_ERR_NULLPTR;
    }

    *qlist = zero_alloc(sizeof(blive_qlist));
    if (*qlist == NULL) {
        return BLIVE_ERR_OUTOFMEM;
    }

    pthread_mutex_init(&(*qlist)->lock, NULL);
    LIST_NODE_INIT(&(*qlist)->list_head);
    return BLIVE_ERR_OK;
}

blive_errno_t qlist_destroy(blive_qlist* qlist)
{
    list*       list_ptr = NULL;
    qlist_unit* foreach_unit = NULL;

    if (qlist == NULL) {
        return BLIVE_ERR_NULLPTR;
    }
    list_ptr = qlist->list_head.prev;

    /*qlist不为空，遍历链表释放内存*/
    for (int count = 0; count < qlist->elem_num; count++) {
        foreach_unit = list_entry(list_ptr, qlist_unit, list_node);
        list_ptr = list_ptr->prev;
        free(foreach_unit);
    }

    pthread_mutex_destroy(&qlist->lock);
    free(qlist);
    return BLIVE_ERR_OK;
}

Bool qlist_anchorage_existence(blive_qlist* qlist, uint32_t anchorage)
{
    Bool    res = False;

    /*检索读取的时候加锁避免竞争*/
    pthread_mutex_lock(&qlist->lock);
    res = qlist_search(qlist, anchorage) != NULL ? True : False;
    pthread_mutex_unlock(&qlist->lock);
    return res;
}

blive_errno_t qlist_append_update(blive_qlist* qlist, uint32_t anchorage, const qlist_unit_data* data)
{
    qlist_unit*     unit = NULL;
    blive_errno_t   retval = BLIVE_ERR_UNKNOWN;

    pthread_mutex_lock(&qlist->lock);
    unit = qlist_search(qlist, anchorage);
    /*链表中已存在锚定值对应的单元，则更新他的数据*/
    if (unit != NULL) {
        blive_logd("update qlist anchorage %u's weight from %u to %u", anchorage, unit->weight, weight);
        memcpy(&unit->data, data, sizeof(qlist_unit_data));
        retval = BLIVE_ERR_OK;
    /*链表中不存在锚定值对应的单元，则创建一个新单元用于存储*/
    } else {
        unit = zero_alloc(sizeof(qlist_unit));
        if (unit == NULL) {
            blive_loge("out of mem!");
            retval = BLIVE_ERR_OUTOFMEM;
        } else {
            blive_logd("create qlist new unit: anchorage %u, weight %u", anchorage, weight);
            LIST_NODE_INIT(&unit->list_node);
            unit->anchorage = anchorage;
            memcpy(&unit->data, data, sizeof(qlist_unit_data));
            retval = qlist_append(qlist, unit);
            if (!retval) {
                qlist->elem_num++;
            }
        }
    }

    pthread_mutex_unlock(&qlist->lock);
    return retval;
}

blive_errno_t qlist_subtract(blive_qlist* qlist, uint32_t anchorage)
{
    qlist_unit*     unit = NULL;

    pthread_mutex_lock(&qlist->lock);
    unit = qlist_search(qlist, anchorage);
    if (unit == NULL) {
        blive_logi("subtract failed: not found anchorage %u", anchorage);
        return BLIVE_ERR_RESOURCE;
    }
    UNIT_SUBTRACT(unit);
    if (qlist->tmp_unit == unit) {
        qlist->tmp_unit = NULL;
    }
    qlist->elem_num--;
    blive_logi("subtract unit: anchorage %u", anchorage);
    free(unit);
    pthread_mutex_unlock(&qlist->lock);

    return BLIVE_ERR_OK;
}

blive_errno_t qlist_foreach(blive_qlist* qlist, qlist_foreach_cb cb, void* context)
{
    list*       list_ptr = &qlist->list_head;
    qlist_unit* each_unit = NULL;

    if (qlist == NULL || cb == NULL) {
        return BLIVE_ERR_NULLPTR;
    }
    if (!qlist->elem_num) {
        return BLIVE_ERR_RESOURCE;
    }
    pthread_mutex_lock(&qlist->lock);

    /*qlist不为空，遍历链表查找锚定值是否有相同的，如果有返回找到的单元*/
    for (int count = 0; count < qlist->elem_num; count++) {
        list_ptr = list_ptr->prev;
        each_unit = list_entry(list_ptr, qlist_unit, list_node);
        if (cb(each_unit->anchorage, &each_unit->data, context) == False) {
            pthread_mutex_unlock(&qlist->lock);
            return BLIVE_ERR_TERMINATE;
        }
    }

    pthread_mutex_unlock(&qlist->lock);
    return BLIVE_ERR_OK;
}
