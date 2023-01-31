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

void danmu_callbacks(blive* entity, const cJSON* msg, blive_ext_cfg* config)
{
    cJSON*      json_obj = NULL;
    char*       danmu_body = NULL;              /*弹幕内容*/
    uint32_t    danmu_sender_uid = 0;           /*弹幕发送者uid*/
    char*       danmu_sender_name = 0;          /*弹幕发送者昵称*/
    uint32_t    fans_price_level = 0;           /*弹幕发送者粉丝牌等级*/
    char*       fans_price_name = NULL;         /*弹幕发送者粉丝牌名称*/
    char*       fans_price_liver_name = NULL;   /*弹幕发送者粉丝牌对应的主播名称*/

    /**
     * @brief 弹幕消息示例：
     * 
     *    ---       {
     *     ↑            "cmd": "DANMU_MSG",
     *     |            "info": [
     *     |                [
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
     *                          0,0, 0, 10000, 1, "" 
     *                      ],
     * 粉丝牌信息 →          [
     *     粉丝牌等级            5,
     *     粉丝牌名称            "小纸鱼",
     *     粉丝牌对应的主播      "薄海纸鱼", 
     *                          706667, 6126494, "", 0, 12632256, 12632256, 12632256, 0, 0, 1837617
     *                      ],
     *    ---                   [0, 0, 9868950, ">50000", 2],
     *     ↑                    ["", "" ],
     *     |                    0,
     *     |                    0,
     *     |                    null,
     *     |                    {"ts": 1673789362, "ct": "A4721FE3"},
     *   无用消息                0,
     *     |                    0,
     *     |                    null,
     *     |                    null,
     *     |                    0,
     *     |                    21
     *     ↓                ]
     *    ---           }
     * 
     */

    /*获取info的数组中，第2个元素，即为弹幕内容*/
    json_obj = cJSON_GetObjectItem(msg, "info");
    danmu_body = cJSON_GetArrayItem(json_obj, 1)->valuestring;

    /*获取info的数组中，第3个元素，即为弹幕发送者信息*/
    json_obj = cJSON_GetObjectItem(msg, "info");
    json_obj = cJSON_GetArrayItem(json_obj, 2);
    danmu_sender_uid = cJSON_GetArrayItem(json_obj, 0)->valueint;
    danmu_sender_name = cJSON_GetArrayItem(json_obj, 1)->valuestring;

    /*获取info的数组中，第4个元素，即为粉丝牌信息*/
    json_obj = cJSON_GetObjectItem(msg, "info");
    json_obj = cJSON_GetArrayItem(json_obj, 3);
    if (cJSON_GetArrayItem(json_obj, 0) != NULL) {
        fans_price_level = cJSON_GetArrayItem(json_obj, 0)->valueint;
        fans_price_name = cJSON_GetArrayItem(json_obj, 1)->valuestring;
        fans_price_liver_name = cJSON_GetArrayItem(json_obj, 2)->valuestring;
        printf("%s [%s Lv.%d] %s(%d): %s\n", fans_price_liver_name, fans_price_name, fans_price_level, 
                danmu_sender_name, danmu_sender_uid, danmu_body);
    } else {
        printf("%s(%d): %s\n", danmu_sender_name, danmu_sender_uid, danmu_body);
    }

    return ;
}