/**
 * @file main.c
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-01-30
 * 
 * @copyright Copyright (c) 2023
 */

#include <stdio.h>
#include <pthread.h>

#include "blive_api/blive_api.h"
#include "select.h"

static int __blive_schedule_func(void *sched_entity, size_t millisec, blive_schedule_cb cb, void* cb_context)
{
    return select_engine_schedule_add((select_engine_t*)sched_entity, cb, cb_context, millisec * 1000);
}

void* thrd(void* arg)
{
    select_engine_t*    engine = (select_engine_t*)arg;

    select_engine_run(engine);
    printf("thread end\n");
    return NULL;
}

int main(void)
{
    select_engine_t*    engine = NULL;
    blive*      entity = NULL;
    blive_schedule_func sched_func = __blive_schedule_func;
    pthread_t   pid;
    void*       thrd_ret;

    select_engine_create(&engine);
    pthread_create(&pid, NULL, thrd, engine);

    blive_api_init();
    blive_create(&entity, 0, 25348832);
    blive_establish_connection(entity, sched_func, engine);
    blive_perform(entity, 20);
    blive_close_connection(entity);
    blive_destroy(entity);
    blive_api_deinit();

    select_engine_stop(engine);
    pthread_join(pid, &thrd_ret);

    return 0;
}