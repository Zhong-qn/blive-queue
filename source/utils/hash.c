/**
 * @file hash.c
 * @author Zhong Qiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <string.h>
#include "hash.h"
#include "utils.h"

#define INITIAL_MAX 15 /* 2^n - 1 */

#define HASH_DEFAULT_MULTIPLER 33

typedef struct hash_entry {
    int32_t hash;               /* 哈希值 */
    char key[32];               /* 键 */
    void* value;                /* 值 */
    struct hash_entry* next;    /* 哈希链表下一个 */
} hash_entry_t;


struct hash_t {
    hash_entry_t **array;       /* 哈希bucket */
    uint32_t count;             /* 当前哈希表内的数据 */
    uint32_t max;               /* 哈希表最大存储数据 */
    hash_func hash_func;        /* 哈希函数 */
    hash_entry_t *free;         /* 避免频繁free使用 */
};

/**
 * @brief 默认的哈希函数
 * 
 * @param char_key 
 * @return uint32_t 
 */
static uint32_t hashfunc_default(const char* char_key)
{
    const char*     key = (const char *)char_key;
    const char*     cur_pos = NULL;
    uint32_t        hash = 0;

    for (cur_pos = key; *cur_pos; cur_pos++) {
        hash = hash * HASH_DEFAULT_MULTIPLER + *cur_pos;
    }
    return hash;
}

/**
 * @brief 分配哈希bucket
 * 
 * @param hash_table 哈希表描述结构体
 * @param max 哈希表最大值
 * @return hash_entry_t** 
 */
static hash_entry_t** __alloc_array(hash_t *hash_table, uint32_t max)
{
    return zero_alloc(sizeof(*hash_table->array) * (max + 1));
}

/**
 * @brief 进行哈希表的扩容
 * 
 * @param hash_table 哈希表描述结构体
 */
static void __expand_array(hash_t *hash_table)
{
    hash_entry_t**   new_array = NULL;
    uint32_t            new_max;

    new_max = hash_table->max * 2 + 1;
    new_array = __alloc_array(hash_table, new_max);

    for (uint32_t i = 0; i <= hash_table->max; i++) {
        hash_entry_t *entry = hash_table->array[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            uint32_t index = entry->hash & new_max;
            entry->next = new_array[index];
            new_array[index] = entry;
            entry = next;
        }
    }
    free(hash_table->array);
    hash_table->array = new_array;
    hash_table->max = new_max;
}


blive_errno_t hash_create(hash_t **out, uint32_t size, hash_func hash_func)
{
    uint32_t    max_size = 0;
    hash_t*  hash_table = NULL;
    blive_errno_t  retval = BLIVE_ERR_OK;

    if (out == NULL) {
        return BLIVE_ERR_NULLPTR;
    }
    if (size < 0) {
        return BLIVE_ERR_INVALID;
    }

    if (size == 0) {
        max_size = INITIAL_MAX;
    } else {
        // 0.75 load factor.
        size = size * 4 / 3;
        max_size = 1;
        while (max_size < size) {
            max_size <<= 1;
        }
        max_size -= 1;
    }

    if (hash_func == NULL)
        hash_func = hashfunc_default;

    hash_table = zero_alloc(sizeof(hash_t));
    *out = hash_table;
    hash_table->count = 0;
    hash_table->max = max_size;
    hash_table->hash_func = hash_func;
    hash_table->array = __alloc_array(hash_table, hash_table->max);
    hash_table->free = NULL;

    return retval;
}


blive_errno_t hash_destroy(hash_t *hash_table)
{
    blive_errno_t          retval = BLIVE_ERR_OK;
    hash_entry_t*    entry = NULL;

    if (hash_table == NULL) {
        return BLIVE_ERR_NULLPTR;
    }

    for (uint32_t i = 0; i <= hash_table->max; i++) {
        entry = hash_table->array[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    entry = hash_table->free;
    while(entry) {
        hash_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }

    free(hash_table->array);
    free(hash_table);

    return retval;
}

static hash_entry_t **find_entry(hash_t *hash_table, const char *key, const void* value)
{
    hash_entry_t**  retval = NULL;
    hash_entry_t*   hash_entry = NULL;
    uint32_t        hash = 0;
    Bool            found = False;

    if (hash_table == NULL) {
        return NULL;
    }

    hash = hash_table->hash_func(key);
    retval = &hash_table->array[hash & hash_table->max];

    for (hash_entry = *retval; hash_entry; retval = &hash_entry->next, hash_entry = *retval) {
        if (hash_entry->hash == hash && strcmp(hash_entry->key, key) == 0) {
            found = True;
            break;
        }
    }

    if (value == NULL && !found) {
        return retval;
    }
    if (hash_entry != NULL || value == NULL) {
        return retval;
    }

    if ((hash_entry = hash_table->free) != NULL) {
        hash_table->free = hash_entry->next;
    } else {
        hash_entry = zero_alloc(sizeof(*hash_entry));
    }
    hash_entry->next = NULL;
    hash_entry->hash = hash;
    strncpy(hash_entry->key, key, sizeof(hash_entry->key) - 2);
    *retval = hash_entry;
    hash_table->count++;

    return retval;
}


void* hash_push(hash_t *hash_table, const char *key, const void* value)
{
    void* old_value = NULL;
    hash_entry_t **hash_entry_addr;

    if (key == NULL) {
        return NULL;
    }

    hash_entry_addr = find_entry(hash_table, key, value);
    if (hash_entry_addr == NULL) {
        /* 没有存储过却要删除key */
        return old_value;
    } else if (*hash_entry_addr) {
        if (!value) {
            /* delete entry */
            hash_entry_t *old = *hash_entry_addr;
            *hash_entry_addr = (*hash_entry_addr)->next;
            old->next = hash_table->free;
            old_value = old->value;
            old->value = NULL;
            //free指针指向被移除的节点，该节点将提供给下一个写入节点使用，避免频繁malloc
            hash_table->free = old;
            --hash_table->count;
        } else {
            /* replace entry */
            old_value = (*hash_entry_addr)->value;
            (*hash_entry_addr)->value = (void*)value;
            /* check that the collision rate isn't too high */
            if (hash_table->count > hash_table->max) {
                __expand_array(hash_table);
            }
        }
    }
    return (void*)old_value;
}


void* hash_pop(hash_t* hash_table, const char* key)
{
    return hash_push(hash_table, key, NULL);
}


void* hash_peek(hash_t *hash_table, const char *key)
{
    hash_entry_t**   hash_entry_addr = NULL;

    hash_entry_addr = find_entry(hash_table, key, NULL);
    if (hash_entry_addr)
        return (void*)((*hash_entry_addr)->value);
    else
        return NULL;
}


int32_t hash_count(hash_t *hash_table)
{
    return hash_table->count;
}

void hash_foreach(hash_t *hash_table, hash_cb callback, void* context)
{
    uint32_t    i = 0;

    for (i = 0; i <= hash_table->max; i++) {
        hash_entry_t* entry = hash_table->array[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            if (!callback(entry->key, entry->value, context)) {
                return;
            }
            entry = next;
        }
    }
}

