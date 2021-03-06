// Copyright (2017) Baidu Inc. All rights reserveed.
/*
 * File: duerapp_ota.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Demo
 */

#ifndef BAIDU_DUER_DUERAPP_OTA_H
#define BAIDU_DUER_DUERAPP_OTA_H

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Initialise OTA Module
 *
 * @param void:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */

extern int duer_initialize_ota(void);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_DUERAPP_OTA_H
