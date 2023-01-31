/**
 * @file config.h
 * @author zhongqiaoning (691365572@qq.com)
 * @brief 外部json格式配置文件相关头部信息
 * @version 0.1
 * @date 2023-01-31
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __BLIVE_EXT_CONFIG_H__
#define __BLIVE_EXT_CONFIG_H__

#include "utils.h"

typedef struct {
    struct {
        Bool        capt_first;                 /*排队舰长优先*/
        Bool        allow_gift_jump;            /*允许礼物插队*/
        Bool        allow_gift_promotion;       /*允许礼物提升排队次序*/
        Bool        allow_git_accum;            /*允许礼物价值累计*/
        uint32_t    minvalue_gift_jump;         /*最小的插队礼物价值*/
        uint32_t    minvalue_gift_promotion;    /*最小的提升排队次序礼物价值*/
    } queue_jump_config;    /*插队规则*/

    struct {
        char        title_color[8];         /*标题颜色*/
        char        capt_color[8];          /*舰长颜色*/
        char        fans_color[8];          /*粉丝牌颜色*/
        char        others_color[8];        /*白嫖颜色*/
    } color_config;         /*颜色配置*/
    
    struct {
        Bool        restore_last_queue;     /*重启后恢复上一次关闭前的队伍*/
        Bool        allow_rearrange;        /*允许过号的B插入队伍*/
        uint16_t    max_kept_rearranger;    /*过号重排最多保留几人*/
        uint16_t    reaward_per_pass;       /*每过号1位，插队时向后调整几位*/
        Bool        add_blklst_freq_rearr;  /*频繁过号加入黑名单*/
        uint16_t    add_blklst_rearr_cnt;   /*单次启动期间，连续过号多少次加入黑名单*/
    } rearrange_config;     /*过号重排规则*/

    struct {
        cJSON*      blacklist;      /*黑名单*/
        cJSON*      whitelist;      /*白名单*/
    } filter_config;        /*过滤规则*/
} blive_ext_cfg;

#endif