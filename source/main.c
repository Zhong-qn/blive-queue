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
#include <unistd.h>
#include <pthread.h>

#include "blive_api/blive_api.h"

#include "select.h"
#include "cJSON.h"

#include "config.h"
#include "bliveq_internal.h"
#include "callbacks.h"
#include "httpd.h"


#define BLIVE_QUEUE_CFG_PATH        "./config/pdjcfg.json"


static int schedule_set_func(void *sched_entity, size_t millisec, blive_schedule_cb cb, void* cb_context)
{
    return select_engine_schedule_add((select_engine_t*)sched_entity, cb, cb_context, millisec * 1000);
}

/**
 * @brief 定时器功能模块线程
 * 
 * @param arg 
 * @return void* 
 */
static void* select_engine_thread(void* arg)
{
    select_engine_t*    engine = (select_engine_t*)arg;

    select_engine_perform(engine);
    blive_loge("select_engine_thread end\n");
    return NULL;
}

/**
 * @brief bilibili直播间数据解析模块线程
 * 
 * @param arg 
 * @return void* 
 */
static void* blive_thread(void* arg)
{
    blive*              entity = (blive*)arg;

    blive_perform(entity, -1);
    blive_loge("blive_thread end\n");
    return NULL;
}

/**
 * @brief 解析、生成配置
 * 
 * @param config_path 配置文件路径
 * @param config 配置结构体
 */
static int parse_config(const char* config_path, blive_ext_cfg* config)
{
    cJSON*  json_obj = NULL;
    cJSON*  json_main = NULL;
    char*   config_buffer = NULL;
    FILE*   fp = NULL;
    size_t  txt_size = 0;
    size_t  read_size = 0;
    char*   tmp_buffer = NULL;

    fp = fopen(config_path, "r+");
    fseek(fp, 0, SEEK_END);
    txt_size = ftell(fp);     /*获取文件大小*/
    config_buffer = zero_alloc(txt_size + 1);
    fseek(fp, 0, SEEK_SET);
    read_size = fread(config_buffer, 1, txt_size, fp);
    if (txt_size != read_size) {
        free(config_buffer);
        return BLIVE_ERR_UNKNOWN;
    }

    /*解析json格式的配置文件*/
    json_main = cJSON_Parse(config_buffer);
    if (json_main == NULL) {
        free(config_buffer);
        return BLIVE_ERR_INVALID;
    }
    free(config_buffer);

    /*加载监听的直播间*/
    json_obj = cJSON_GetObjectItem(json_main, "监听的直播间");
    if (json_obj == NULL) {
        cJSON_Delete(json_main);
        return BLIVE_ERR_INVALID;
    }
    config->room_id = cJSON_GetArrayItem(json_obj, 0)->valueint;

    /*加载排队规则*/
    json_obj = cJSON_GetObjectItem(json_main, "排队规则");
    if (json_obj != NULL) {
        strncpy(config->queue_up_config.host_name, cJSON_GetObjectItem(json_obj, "主播名称")->valuestring, DEFAULT_NAME_LEN - 1);
        config->queue_up_config.capt_first = cJSON_GetObjectItem(json_obj, "舰队优先")->type == cJSON_True ? True : False;
        config->queue_up_config.allow_danmu_queueup = cJSON_GetObjectItem(json_obj, "允许弹幕排队")->type == cJSON_True ? True : False;
        config->queue_up_config.allow_gift_queueup = cJSON_GetObjectItem(json_obj, "允许送礼物排队")->type == cJSON_True ? True : False;
        config->queue_up_config.minvalue_gift_queueup = cJSON_GetObjectItem(json_obj, "礼物排队最低送出礼物价值")->valueint;
    }

    /*加载颜色配置*/
    json_obj = cJSON_GetObjectItem(json_main, "颜色配置");
    if (json_obj != NULL) {
        strncpy(config->color_config.title_color, cJSON_GetObjectItem(json_obj, "标题颜色")->valuestring, 7);
        strncpy(config->color_config.capt_color, cJSON_GetObjectItem(json_obj, "舰队颜色")->valuestring, 7);
        strncpy(config->color_config.fans_color, cJSON_GetObjectItem(json_obj, "粉丝牌颜色")->valuestring, 7);
        strncpy(config->color_config.others_color, cJSON_GetObjectItem(json_obj, "白嫖颜色")->valuestring, 7);
    }

    /*加载过号重排规则*/
    json_obj = cJSON_GetObjectItem(json_main, "过号重排规则");
    if (json_obj != NULL) {
        config->rearrange_config.restore_last_queue = cJSON_GetObjectItem(json_obj, "重启软件保留原先的排队列表")->type == cJSON_True ? True : False;
        config->rearrange_config.allow_rearrange = cJSON_GetObjectItem(json_obj, "允许过号重排")->type == cJSON_True ? True : False;
        config->rearrange_config.max_kept_rearranger = cJSON_GetObjectItem(json_obj, "过号列表最多保留几位")->valueint;
        config->rearrange_config.reaward_per_pass = cJSON_GetObjectItem(json_obj, "过号1位排队向后调整几位")->valueint;
        config->rearrange_config.add_blklst_freq_rearr = cJSON_GetObjectItem(json_obj, "频繁过号加入黑名单")->type == cJSON_True ? True : False;
        config->rearrange_config.add_blklst_rearr_cnt = cJSON_GetObjectItem(json_obj, "单次启动期间连续过号几次加入黑名单")->valueint;
    }

    /*过滤规则*/
    json_obj = cJSON_GetObjectItem(json_main, "过号重排规则");
    if (json_obj != NULL) {
        tmp_buffer = cJSON_PrintBuffered(cJSON_GetObjectItem(json_obj, "黑名单"), 512, 0);
        config->filter_config.blacklist = cJSON_Parse(tmp_buffer);
        cJSON_free(tmp_buffer);
        tmp_buffer = cJSON_PrintBuffered(cJSON_GetObjectItem(json_obj, "白名单"), 512, 0);
        config->filter_config.whitelist = cJSON_Parse(tmp_buffer);
        cJSON_free(tmp_buffer);
    }

    /*删除临时数据*/
    cJSON_Delete(json_main);

    return BLIVE_ERR_OK;
}


int main(void)
{
    pthread_t           timer_pid;
    void*               thrd_ret = NULL;
    blive_queue         queue_entity;

    /*加载配置文件*/
    if (parse_config(BLIVE_QUEUE_CFG_PATH, &queue_entity.conf) != BLIVE_ERR_OK) {
        blive_loge("解析配置文件失败！");
        return ERROR;
    }

    /*启动select_engine来实现定时器功能模块*/
    select_engine_create(&queue_entity.engine);
    pthread_create(&timer_pid, NULL, select_engine_thread, queue_entity.engine);

    /*http服务器初始化*/
    http_create(&queue_entity.httpd, "127.0.0.1", 8899);

    /*callbacks初始化*/
    if (callbacks_init(&queue_entity)) {
        blive_loge("callbacks初始化失败！");
        return ERROR;
    }

    /*初始化bilibili直播间解析模块*/
    blive_api_init();

    /*初始化blive*/
    blive_create(&queue_entity.conf.room_entity, 0, queue_entity.conf.room_id);
    blive_establish_connection(queue_entity.conf.room_entity, schedule_set_func, queue_entity.engine);

    /*设置接收消息的回调函数*/
    blive_set_command_callback(queue_entity.conf.room_entity, BLIVE_INFO_DANMU_MSG, danmu_callback, &queue_entity);
    blive_set_command_callback(queue_entity.conf.room_entity, BLIVE_INFO_SEND_GIFT, send_gift_callback, &queue_entity);

    /*启动监听*/
    pthread_create(&queue_entity.conf.thread_id, NULL, blive_thread, queue_entity.conf.room_entity);

    http_perform(queue_entity.httpd);

    /*结束bilibili直播间解析模块*/
    blive_force_stop(queue_entity.conf.room_entity);
    pthread_join(queue_entity.conf.thread_id, &thrd_ret);
    blive_close_connection(queue_entity.conf.room_entity);
    blive_destroy(queue_entity.conf.room_entity);
    blive_api_deinit();

    /*结束定时器功能模块*/
    select_engine_stop(queue_entity.engine);
    pthread_join(timer_pid, &thrd_ret);
    select_engine_destroy(queue_entity.engine);

    return 0;
}