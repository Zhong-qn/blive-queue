/**
 * @file callbacks.c
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 定义排队姬向BLIVE模块注册的回调函数
 * @version 0.1
 * @date 2023-01-31
 * 
 * @copyright Copyright (c) 2023
 */

#include <stdio.h>

#include "blive_api/blive_api.h"

#include "select.h"
#include "cJSON.h"

#include "config.h"
#include "bliveq_internal.h"
#include "qlist.h"
#include "http_server.h"


typedef struct {
    blive_info_type info_type;                              /*消息类型*/
    qlist_unit_data data;
} user_info;


static void liveroom_info_recv(fd_t fd, void* data);


static Bool search_user_in_list(const user_info* info, cJSON* list)
{
    int         arr_num = 0;
    cJSON*      json_obj = NULL;
    cJSON*      json_uid_in_list = NULL;
    cJSON*      json_name_in_list = NULL;

    while ((json_obj = cJSON_GetArrayItem(list, arr_num++)) != NULL) {
        json_uid_in_list = cJSON_GetObjectItem(json_obj, "uid");
        if ((json_uid_in_list != NULL) && (json_uid_in_list->valueint == info->data.danmu_sender_uid)) {
            json_name_in_list = cJSON_GetObjectItem(json_obj, "昵称");
            /*如果获取到的uid一致但是昵称不一致，更新配置文件中的昵称*/
            if (strcmp(json_name_in_list->valuestring, info->data.danmu_sender_name)) {
                cJSON_SetValuestring(json_name_in_list, info->data.danmu_sender_name);
            }
            return True;
        }
    }

    return False;
}

static inline blive_errno_t liveroom_info_send(blive_queue* queue_entity, const user_info* info)
{
    size_t  wr_size = 0;

    wr_size = fd_write(WR_FD(queue_entity->qlist_fd), info, sizeof(user_info));
    if (wr_size != sizeof(user_info)) {
        return BLIVE_ERR_UNKNOWN;
    }
    return BLIVE_ERR_OK;
}

static void liveroom_info_recv(fd_t fd, void* data)
{
    uint32_t        rd_size = 0;
    user_info       info = {0};
    blive_queue*    queue_entity = (blive_queue*)data;

    rd_size = fd_read(fd, &info, sizeof(user_info));
    if (rd_size != sizeof(user_info)) {
        blive_loge("liveroom info received failed(recv %d/%d)", rd_size, sizeof(user_info));
        return ;
    }

    switch (info.info_type) {
    case BLIVE_INFO_DANMU_MSG:  /*说明是观众发送了排队弹幕*/
    {
        int     weight = 1; /*权重值，用于在队列中进行排序，默认为1*/
        /**
         * @brief 权重共有8个等级：1、2、3、4、5、6、7、8，
         *          1    白嫖用户弹幕排队
         *          2    白嫖用户赠送礼物排队
         *          3    舰长弹幕排队
         *          4    提督弹幕排队
         *          5    总督弹幕排队
         *          6    舰长礼物排队
         *          7    提督礼物排队
         *          8    总督礼物排队
         * 
         */

        /*如果配置中关闭了弹幕排队，则直接退出*/
        if (!queue_entity->conf.queue_up_config.allow_danmu_queueup) {
            break;
        }
        /*如果配置了排队舰队优先，并且排队用户为舰队成员，则重新生成权重*/
        if (queue_entity->conf.queue_up_config.capt_first && info.data.fleet_lv) {
            weight = (FLEET_LV_MAX - info.data.fleet_lv) + 2;
        }
        info.data.weight = weight;
        // blive_loge("qlist_append_update");
        qlist_append_update(queue_entity->qlist, info.data.danmu_sender_uid, &info.data);
        break;
    }
    case BLIVE_INFO_SEND_GIFT:
    {
        int     weight = 2; /*权重值，用于在队列中进行排序，默认为1*/
        /**
         * @brief 权重共有8个等级：1、2、3、4、5、6、7、8，
         *          1    白嫖用户弹幕排队
         *          2    白嫖用户赠送礼物排队
         *          3    舰长弹幕排队
         *          4    提督弹幕排队
         *          5    总督弹幕排队
         *          6    舰长礼物排队
         *          7    提督礼物排队
         *          8    总督礼物排队
         * 
         */
        
        /*如果配置中关闭了礼物排队，则直接退出*/
        if (!queue_entity->conf.queue_up_config.allow_gift_queueup) {
            break;
        }
        /*如果配置了排队舰队优先，并且排队用户为舰队成员，则重新生成权重*/
        if (queue_entity->conf.queue_up_config.capt_first && info.data.fleet_lv) {
            weight = (FLEET_LV_MAX - info.data.fleet_lv) + 2 + 3;
        }
        info.data.weight = weight;
        // blive_loge("qlist_append_update");
        qlist_append_update(queue_entity->qlist, info.data.danmu_sender_uid, &info.data);
        break;
    }
    default:
        break;
    }
    return ;
}

static Bool qlist_foreach_make_text(uint32_t anchorage, const qlist_unit_data* data, void* context)
{
    char*   dst = (char*)context;
    int     prefix_datalen = strlen(dst);

    sprintf(dst + prefix_datalen, "<h2>%s\r\n", data->danmu_sender_name);
    return True;
}

static void liveroom_qlist_make_text(char* dst, void* context)
{
    blive_queue*    queue_entity = (blive_queue*)context;
    blive_errno_t   err = BLIVE_ERR_OK;

    if ((err = qlist_foreach(queue_entity->qlist, qlist_foreach_make_text, dst)) != BLIVE_ERR_OK) {
        blive_loge("foreach get qlist text file failed!(%d)", err);
    }

    return ;
}


blive_errno_t callbacks_init(blive_queue* queue_entity)
{
    blive_errno_t err = BLIVE_ERR_OK;

    if (_socketpair(queue_entity->qlist_fd)) {
        return BLIVE_ERR_UNKNOWN;
    }

    err = qlist_create(&queue_entity->qlist);
    if (err) {
        return err;
    }
    select_engine_fd_add_forever(queue_entity->engine, RD_FD(queue_entity->qlist_fd), liveroom_info_recv, queue_entity);

    /*在index.html中添加动态注入的排队列表*/
    http_html_injection("__queuelist_injection__", liveroom_qlist_make_text, queue_entity);

    return BLIVE_ERR_OK;
}

void danmu_callback(blive* entity, const cJSON* msg, blive_queue* queue_entity)
{
    cJSON*      json_info_obj = NULL;
    cJSON*      json_obj = NULL;
    user_info   info = {.info_type = BLIVE_INFO_DANMU_MSG};
    char*       danmu_body = NULL;

    /**
     * @brief 弹幕消息示例：
     * 
     *              {
     * 消息类型：弹幕    "cmd": "DANMU_MSG",
     *    ---           "info": [
     *     ↑                [
     *     |                    0, 1, 25, 16777215, 1673789362967 ,1673770572, 0, "81240bc1", 0, 0, 0, "", 0, "{}", "{}",
     *     |                    {    
     *     |                        "mode": 0,
     *   无用消息                    "show_player_type": 0,
     *     |                        "extra": "{\"send_from_me\":false,\"mode\":0,\"color\":16777215,\"dm_type\":0,\"font_size\":25,\"player_mode\":1,\"show_player_type\":0,\"content\":\"测试文本\",\"user_hash\":\"2166623169\",\"emoticon_unique\":\"\",\"bulge_display\":0,\"recommend_score\":8,\"main_state_dm_color\":\"\",\"objective_state_dm_color\":\"\",\"direction\":0,\"pk_direction\":0,\"quartet_direction\":0,\"anniversary_crowd\":0,\"yeah_space_type\":\"\",\"yeah_space_url\":\"\",\"jump_to_url\":\"\",\"space_type\":\"\",\"space_url\":\"\",\"animation\":{},\"emots\":null}"
     *     |                    },
     *     |                    {
     *     |                        "activity_identity": "",
     *     |                        "activity_source": 0,
     *     |                        "not_show": 0
     *     ↓                    }
     *    ---               ],
     * 弹幕内容 →            "测试文本",
     * 弹幕发送者信息 →      [
     *     uid                  50500335,
     *     昵称                 "属官一号",
     *   是否是房管              0,
     *                          0, 0, 
     *   是否是正式会员          10000, 
     *   是否绑定了手机          1, 
     *                          "" 
     *                      ],
     * 粉丝牌信息 →          [
     *     粉丝牌等级            5,
     *     粉丝牌名称            "小纸鱼",
     *     粉丝牌对应的主播      "薄海纸鱼", 
     *                          706667, 6126494, "", 0, 12632256, 12632256, 12632256, 0, 0, 1837617
     *                      ],
     *                      [0, 0, 9868950, ">50000", 2],
     *                      ["", "" ],
     *                      0,
     * 舰队成员等级          0,
     *    ---               null,
     *     ↑                {"ts": 1673789362, "ct": "A4721FE3"},
     *     |                0,
     *     |                0,
     *  无用的消息           null,
     *     |                null,
     *     |                0,
     *     |                21
     *     ↓            ]
     *    ---       }
     * 
     */

#ifdef BLIVE_API_DEBUG_DEBUG
    char*   print_buffer = cJSON_PrintBuffered(msg, 2048, 1);
    if (print_buffer != NULL) {
        blive_logd("json msg:\n%s\n", print_buffer);
        cJSON_free(print_buffer);
    }
#endif
    json_info_obj = cJSON_GetObjectItem(msg, "info");

    /*获取info的数组中，第2个元素，即为弹幕内容*/
    danmu_body = cJSON_GetArrayItem(json_info_obj, 1)->valuestring;

    /*获取info的数组中，第3个元素，即为弹幕发送者信息*/
    json_obj = cJSON_GetArrayItem(json_info_obj, 2);
    info.data.danmu_sender_uid = cJSON_GetArrayItem(json_obj, 0)->valueint;
    strncpy(info.data.danmu_sender_name, cJSON_GetArrayItem(json_obj, 1)->valuestring, DEFAULT_NAME_LEN - 1);
    info.data.is_hostoom_manager = cJSON_GetArrayItem(json_obj, 2)->valueint;

    /*获取info的数组中，第8个元素，即为舰队等级*/
    json_obj = cJSON_GetArrayItem(json_info_obj, 7);
    info.data.fleet_lv = json_obj->valueint;

    /*获取info的数组中，第4个元素，即为粉丝牌信息*/
    json_obj = cJSON_GetArrayItem(json_info_obj, 3);
    if (cJSON_GetArrayItem(json_obj, 0) != NULL) {
        info.data.fans_price_level = cJSON_GetArrayItem(json_obj, 0)->valueint;
        strncpy(info.data.fans_price_name, cJSON_GetArrayItem(json_obj, 1)->valuestring, DEFAULT_NAME_LEN - 1);
        if (strcmp(queue_entity->conf.queue_up_config.host_name, cJSON_GetArrayItem(json_obj, 2)->valuestring)) {
            info.data.fans_price_is_cur_liveroom = True;
        }
        blive_logi("[%s Lv.%d] %s(%d): %s\n", info.data.fans_price_name, info.data.fans_price_level, 
                info.data.danmu_sender_name, info.data.danmu_sender_uid, danmu_body);
    } else {
        blive_logi("%s(%d): %s\n", info.data.danmu_sender_name, info.data.danmu_sender_uid, danmu_body);
    }

    /*发送的不是排队，直接返回*/
    // if (strcmp(danmu_body, "排队")) {
    //     return ;
    // }

    /*如果用户在白名单内，直接进入排队列表*/
    if (search_user_in_list(&info, queue_entity->conf.filter_config.whitelist)) {
        blive_logd("user %s(%d) in whitelist\n", info.danmu_sender_name, info.danmu_sender_uid);
        goto ADD_LIST;
    }
    /*如果用户在黑名单内，直接返回*/
    if (search_user_in_list(&info, queue_entity->conf.filter_config.blacklist)) {
        blive_logd("user %s(%d) in blacklist, return\n", info.danmu_sender_name, info.danmu_sender_uid);
        return ;
    }

ADD_LIST:
    if (liveroom_info_send(queue_entity, &info)) {
        blive_loge("push msg to qlist failed!");
    }

    return ;
}

void send_gift_callback(blive* entity, const cJSON* msg, blive_queue* queue_entity)
{
    // cJSON*      json_info_obj = NULL;
    // cJSON*      json_obj = NULL;
    // user_info   info = {.info_type = BLIVE_INFO_SEND_GIFT};
    // char*       danmu_body = NULL;

    /**
     * @brief 赠送礼物消息示例：
     * 
     *              {
     * 消息类型：弹幕    "cmd": "SEND_GIFT",
     *    ---           "info": [
     *     ↑                [
     *     |                    0, 1, 25, 16777215, 1673789362967 ,1673770572, 0, "81240bc1", 0, 0, 0, "", 0, "{}", "{}",
     *     |                    {    
     *     |                        "mode": 0,
     *   无用消息                    "show_player_type": 0,
     *     |                        "extra": "{\"send_from_me\":false,\"mode\":0,\"color\":16777215,\"dm_type\":0,\"font_size\":25,\"player_mode\":1,\"show_player_type\":0,\"content\":\"测试文本\",\"user_hash\":\"2166623169\",\"emoticon_unique\":\"\",\"bulge_display\":0,\"recommend_score\":8,\"main_state_dm_color\":\"\",\"objective_state_dm_color\":\"\",\"direction\":0,\"pk_direction\":0,\"quartet_direction\":0,\"anniversary_crowd\":0,\"yeah_space_type\":\"\",\"yeah_space_url\":\"\",\"jump_to_url\":\"\",\"space_type\":\"\",\"space_url\":\"\",\"animation\":{},\"emots\":null}"
     *     |                    },
     *     |                    {
     *     |                        "activity_identity": "",
     *     |                        "activity_source": 0,
     *     |                        "not_show": 0
     *     ↓                    }
     *    ---               ],
     * 弹幕内容 →            "测试文本",
     * 弹幕发送者信息 →      [
     *     uid                  50500335,
     *     昵称                 "属官一号",
     *   是否是房管              0,
     *                          0, 0, 
     *   是否是正式会员          10000, 
     *   是否绑定了手机          1, 
     *                          "" 
     *                      ],
     * 粉丝牌信息 →          [
     *     粉丝牌等级            5,
     *     粉丝牌名称            "小纸鱼",
     *     粉丝牌对应的主播      "薄海纸鱼", 
     *                          706667, 6126494, "", 0, 12632256, 12632256, 12632256, 0, 0, 1837617
     *                      ],
     *                      [0, 0, 9868950, ">50000", 2],
     *                      ["", "" ],
     *                      0,
     * 舰队成员等级          0,
     *    ---               null,
     *     ↑                {"ts": 1673789362, "ct": "A4721FE3"},
     *     |                0,
     *     |                0,
     *  无用的消息           null,
     *     |                null,
     *     |                0,
     *     |                21
     *     ↓            ]
     *    ---       }
     * 
     */

#ifdef BLIVE_API_DEBUG_DEBUG
    char*   print_buffer = cJSON_PrintBuffered(msg, 1024, 1);
    if (print_buffer != NULL) {
        blive_logd("json msg:\n%s\n", print_buffer);
        cJSON_free(print_buffer);
    }
#endif

    // if (liveroom_info_send(queue_entity, &info)) {
    //     blive_loge("push msg to qlist failed!");
    // }

    return ;
}
