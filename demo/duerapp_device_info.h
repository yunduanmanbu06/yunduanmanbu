// Copyright (2017) Baidu Inc. All rights reserveed.
/*
 * File: duerapp_device_info.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: Device Information
 */

#ifndef BAIDU_DUER_DUERAPP_DEVICE_INFO_H
#define BAIDU_DUER_DUERAPP_DEVICE_INFO_H

#include <stdint.h>

#ifndef FIREWARE_VERSION
#define FIRMWARE_VERSION "1.1.1.1"
#endif

#define VERSION_SZ       24

extern const unsigned int OTA_FW_INFO_ADDR;
extern const unsigned int OTA_UPGRADE_ADDR;
extern const unsigned int OTA_UPGRADE_AREA_SIZE;

struct FirmwareInfo {
    uint32_t magic;
    uint8_t  version[VERSION_SZ];
    uint32_t addr;
    uint32_t size;
    uint32_t crc32;
};

struct ImageHeader {
    uint16_t magic;
    uint8_t  encrypt_algo;
    uint8_t  resv[1];
    uint8_t  version[VERSION_SZ];
    uint32_t crc32;
    uint32_t size;
};

/*
 * Initialise Device Information
 *
 * @param void:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */

extern int duer_init_device_info(void);

#endif // BAIDU_DUER_DUERAPP_DEVICE_INFO_H
