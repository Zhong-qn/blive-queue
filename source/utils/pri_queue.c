/**
 * @file pri_queue.c
 * @author Zhong Qiaoning (691365572@qq.com)
 * @brief 优先级队列。
 * @attention 需要注意到的是，该优先级队列为了保证在插入、删除时的快速进
 * 行，以及消耗的资源最少化，选用的是哈夫曼树型结构的上浮、下沉，因此这只
 * 能保证每次移除的根节点一定是优先级最高的，并不能保证整个数组中按顺序下
 * 去是符合优先级的从大到小的顺序，因此该优先级队列并不提供foreach遍历的
 * 接口。
 * @note 仅支持posix标准
 * @version 1.0
 * @date 2022-07-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"
#include "pri_queue.h"


typedef struct heap_element {
    void*               data;               /* 堆元素保存的数据 */
} heap_element_t;

typedef struct internal_heap {
    uint32_t            cur_size;           /* 堆当前的大小 */
    uint32_t            max_size;           /* 堆最大的大小 */
    pri_comp_func       elem_compare_cb;    /* 堆元素比较 */
    heap_element_t      *heap_mem[0];       /* 堆内存起始指针 */
} inn_heap_t;

struct pri_queue {
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    Bool           adaption;       /* 堆大小自适应 */
    inn_heap_t          *heap;
};


static int __queue_pop(pri_queue_t* pri_queue, void** pdata, int32_t timeout);

static void __heap_sort(inn_heap_t *heap, int32_t index);
static void* __heap_remove(inn_heap_t *heap, int32_t index);
static int __heap_push(inn_heap_t *heap, void* data);
static void* __heap_pop(inn_heap_t *heap);
static void* __heap_peek(inn_heap_t *heap);




pri_queue_t *pri_queue_create(int32_t initial_size, Bool size_adaption, pri_comp_func cb)
{
    pri_queue_t *new_queue = NULL;

    if (initial_size <= 0 || cb == NULL || cb == NULL) {
        goto _out;
    }

    new_queue = (pri_queue_t*)zero_alloc(sizeof(pri_queue_t));
    if (new_queue == NULL) {
        goto _out;
    }

    memset(new_queue, 0, sizeof(pri_queue_t));
    pthread_mutex_init(&new_queue->mutex, NULL);
    pthread_cond_init(&new_queue->cond, NULL);

    /* 申请堆内存大小+1是因为堆首元素不使用，这样能够进行快速的上浮、下沉排序算法 */
    new_queue->heap = (inn_heap_t*)zero_alloc(sizeof(inn_heap_t) + ((initial_size + 1) * sizeof(heap_element_t)));
    if (new_queue == NULL) {
        goto _free;
    }
    new_queue->heap->max_size = initial_size;
    new_queue->heap->elem_compare_cb = cb;
    new_queue->adaption = size_adaption;

_out:
    return new_queue;

_free:
    if (new_queue != NULL) {
        if (new_queue->heap != NULL) {
            free(new_queue->heap);
        }
        free(new_queue);
        new_queue = NULL;
    }
    goto _out;
}

int pri_queue_destroy(pri_queue_t *pri_queue)
{
    int     retval = BLIVE_ERR_INVALID;

    if (pri_queue != NULL) {
        pthread_mutex_destroy(&pri_queue->mutex);
        pthread_cond_destroy(&pri_queue->cond);

        if (pri_queue->heap != NULL) {
            for (int i = 1; i <= pri_queue->heap->cur_size; i++) {
                free(pri_queue->heap->heap_mem[i]);
            }
            free(pri_queue->heap);
        }
        free(pri_queue);
        retval = BLIVE_ERR_OK;
    }

    return retval;
}


int pri_queue_peek(pri_queue_t * pri_queue, void** pdata)
{
    int     retval = BLIVE_ERR_OK;

    if (pri_queue == NULL || pdata == NULL) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    pthread_mutex_lock(&pri_queue->mutex);
    if (pri_queue->heap->cur_size > 0) {
        heap_element_t*     elem = NULL;

        elem = __heap_peek(pri_queue->heap);
        *pdata = elem->data;
        retval = BLIVE_ERR_OK;
    } else {
        *pdata = NULL;
        retval = BLIVE_ERR_RESOURCE;
    }
    pthread_mutex_unlock(&pri_queue->mutex);
    blive_logd("peek data address %p\n", *pdata);

_out:
    return retval;
}

int pri_queue_push(pri_queue_t * pri_queue, void* data)
{
    int     retval = BLIVE_ERR_OK;

    if(pri_queue == NULL || data == NULL) {
        retval = BLIVE_ERR_INVALID;
        goto _out;
    }

    pthread_mutex_lock(&pri_queue->mutex);

    retval = __heap_push(pri_queue->heap, data);
    if (retval == BLIVE_ERR_RESOURCE) {
        /* 资源不足，原因为队列已满，如果开启大小自适应，将会进行自动扩容 */
        if (pri_queue->adaption) {
            int32_t  realloc_size = 0;

            /* 扩容大小为原来的2倍 */
            realloc_size = sizeof(inn_heap_t) + (pri_queue->heap->max_size * 2) + 1;
            pri_queue->heap = realloc(pri_queue->heap, realloc_size);
        }
        retval = __heap_push(pri_queue->heap, data);   /* 重新进行一次数据入堆 */
    }

    pthread_cond_broadcast(&pri_queue->cond);
    pthread_mutex_unlock(&pri_queue->mutex);
    blive_logd("push data address %p\n", data);

_out:
    return retval;
}

int pri_queue_pop_wait(pri_queue_t *pri_queue, void* *pdata)
{
    if (pri_queue == NULL || pdata == NULL) {
        return BLIVE_ERR_INVALID;
    }

    return __queue_pop(pri_queue, pdata, -1);
}

int pri_queue_pop_trywait(pri_queue_t *pri_queue, void* *pdata)
{
    if (pri_queue == NULL || pdata == NULL) {
        return BLIVE_ERR_INVALID;
    }

    return __queue_pop(pri_queue, pdata, 0);
}

int pri_queue_pop_timedwait(pri_queue_t *pri_queue, void* *pdata, int32_t timeout)
{
    if (pri_queue == NULL || pdata == NULL || timeout < 0) {
        return BLIVE_ERR_INVALID;
    }

    return __queue_pop(pri_queue, pdata, timeout);
}


int32_t pri_queue_get_size(const pri_queue_t *pri_queue)
{
    if(pri_queue == NULL)
        return -1;
    return pri_queue->heap->cur_size;
}




/**
 * 
 * @param [in] pri_queue 优先级队列
 * @param [out] pdata 二级指针，取出数据
 * @param [in] timeout 超时时间，毫秒（ms）
 * @retval int 
 */
static int __queue_pop(pri_queue_t *pri_queue, void* *pdata, int32_t timeout)
{
    int32_t         retval = BLIVE_ERR_OK;
    struct timespec tm;

    pthread_mutex_lock(&pri_queue->mutex);

    /* 队列中没有元素，根据传入的超时时间进行等待处理 */
    if (pri_queue->heap->cur_size == 0) {
        if (timeout == -1) {
            pthread_cond_wait(&pri_queue->cond, &pri_queue->mutex);
        } else if (timeout > 0) {
            tm.tv_sec = timeout / 1000;
            tm.tv_nsec = (timeout % 1000) * 1000 * 1000;
            pthread_cond_timedwait(&pri_queue->cond, &pri_queue->mutex, &tm);
        } else {
            retval = BLIVE_ERR_RESOURCE;
        }

    /* 队列中有元素，取出 */
    } else if (pri_queue->heap->cur_size > 0) {
        *pdata = __heap_pop(pri_queue->heap);
    } else {
        *pdata = NULL;
        retval = BLIVE_ERR_UNKNOWN;
    }

    pthread_mutex_unlock(&pri_queue->mutex);

    return retval;
}

/**
 * 将数据存入堆中
 * @param [in] heap 堆指针
 * @param [in] data 存入的数据
 * @retval int 
 */
static int __heap_push(inn_heap_t *heap, void* data)
{
    int     retval = BLIVE_ERR_OK;
    heap_element_t  *new_elem = NULL;

    if (heap->cur_size >= heap->max_size) {
        retval = BLIVE_ERR_RESOURCE;
        goto _out;
    }

    /* 将数据放入堆，只能放在堆的底部。第0个不使用 */
    new_elem = zero_alloc(sizeof(heap_element_t));
    if (new_elem == NULL) {
        retval = BLIVE_ERR_OUTOFMEM;
        goto _out;
    }
    new_elem->data = data;
    heap->cur_size++;   /* 先让堆当前大小+1保证首个元素不使用 */
    heap->heap_mem[heap->cur_size] = new_elem;  /* 将新增的元素放在堆底部 */

    /* 对新放入在堆底部的元素进行上浮排序 */
    __heap_sort(heap, heap->cur_size);

_out:
    return retval;
}

/**
 * 查看堆顶部元素
 * @param [in] heap 堆指针
 * @return 存入堆时存放的数据
 */
static void* __heap_peek(inn_heap_t *heap)
{
    return heap->heap_mem[1];
}

/**
 * 移除堆顶部的元素
 * @param [in] heap 堆指针
 * @return 存入堆时存放的数据
 */
static void* __heap_pop(inn_heap_t *heap)
{
    /* pop就是移除堆的第一个数据 */
    return __heap_remove(heap, 1);
}

/**
 * 移除堆中指定位置的元素
 * @param [in] heap 堆指针
 * @param [in] index 移除堆中数据的序号
 * @return 
 */
static void* __heap_remove(inn_heap_t *heap, int32_t index)
{
    void*        data = NULL;
    heap_element_t  *removed_elem = NULL;   /* 要被移除掉的元素 */
    heap_element_t  *tail_elem = NULL;      /* 尾部的元素 */
    int32_t      cur_pos_index = 0;      /* 当前节点的序号 */
    int32_t      tail_parent_index = 0;  /* 尾部元素的父节点的序号 */
    int32_t      left_child_index = 0;   /* 左子节点的序号 */
    int32_t      right_child_index = 0;  /* 右子节点的序号 */
    int32_t      winner_index = 0;       /* 左右子节点中大者的序号 */

    if (!heap->cur_size || index > heap->cur_size || index <= 0) {  /* 堆空间的首个内存不使用，为空！ */
        goto _out;
    }

    removed_elem = heap->heap_mem[index];       /* 保存记录堆中指定序号的元素 */
    tail_elem = heap->heap_mem[heap->cur_size]; /* 保存记录堆中最后一个元素 */
    heap->heap_mem[heap->cur_size] = NULL;      /* 删除堆中最后一个元素 */
    heap->cur_size--;
    tail_parent_index = heap->cur_size / 2;
    cur_pos_index = index;

    /* 循环运行到当前节点已经到达二叉树的最后一层 */
    while (cur_pos_index <= tail_parent_index) {
        left_child_index = cur_pos_index * 2;
        right_child_index = left_child_index + 1;

        /* 获取左右子节点中的大者 */
        winner_index = left_child_index;
        if (left_child_index != heap->cur_size) {   /* 检查左子节点是不是堆中的最后一个 */
            /* 左子节点不是堆中的最后一个，那么右子节点一定存在，比较两者大小 */
            if (heap->elem_compare_cb(heap->heap_mem[right_child_index]->data, heap->heap_mem[left_child_index]->data)) {
                winner_index = right_child_index;
            }
        }

        /* 将左右子节点中的大者与堆尾元素进行比较大小，如果大者并不比堆尾元素大，那就说明此时左右子节点已经是完全二叉树的最后一层，退出 */
        if (heap->elem_compare_cb(tail_elem->data, heap->heap_mem[winner_index]->data)) {
            break;
        }

        /* 如果左右子节点未到达完全二叉树的最后一层，将两者中的大者与当前位置即父节点进行位置交换。
           因为父节点已经保存在select_elem中，因此只要将左右子节点中大者复制到父节点内即可。 */
        heap->heap_mem[cur_pos_index] = heap->heap_mem[winner_index];

        /* 经过一趟循环，相当于将需要移除的节点在二叉树中将往下移动了一层，继续进行这种操作 */
        cur_pos_index = winner_index;
    }

    /* 此时需要删除的节点已经下沉到最末端的节点相同/相邻的层，已经不需要再进行下沉操作了，此时将尾部节点存入当前下沉到的位置 */
    heap->heap_mem[cur_pos_index] = tail_elem;
    __heap_sort(heap, cur_pos_index);   /* 对存入的节点进行一次上浮排序 */

    data = removed_elem->data;  /* 取出数据 */
    free(removed_elem);      /* 释放内存！！该内存是在push的时候申请的！！ */

_out:
    return data;
}

/**
 * 对堆中index位置的数据进行一次上浮排序，直到不能上浮或上浮到根节点为止
 * @param [in] heap 堆指针
 * @param [in] index 数据在堆中的序号
 * @return 成功返回
 */
static void __heap_sort(inn_heap_t *heap, int32_t index)
{
    int32_t      parent_index = 0;
    heap_element_t* tmp_element = NULL;

    tmp_element = heap->heap_mem[index];    /* 保存当前节点信息 */

    /* 上浮排序，比较第index节点和其父节点的大小，如果index节点大于其父节点，交换两者位置，循环进行直到根节点或index小于其父节点 */
    while (index > 1) {
        parent_index = index / 2;
        if (heap->elem_compare_cb(heap->heap_mem[parent_index]->data, heap->heap_mem[index]->data)) {
            break;    /* index元素小于父节点元素，排序停止 */
        }

        /* 由于已经保存了最初节点信息，仅需将父节点信息移动下来即可 */
        heap->heap_mem[index] = heap->heap_mem[parent_index];
        index = parent_index;
    }

    /* 上浮结束，将初始节点放置下来 */
    heap->heap_mem[index] = tmp_element;

    return ;
}