// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_device_info.cpp
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: Device Information
 */

#include "duerapp_device_info.h"
#include <stdint.h>
#include <stdio.h>
#include "lightduer_log.h"
#include "lightduer_types.h"
#include "lightduer_dev_info.h"
#include "WiFiStackInterface.h"
#include "rda_ccfg_api.h"

extern void *baidu_get_netstack_instance(void);

static int get_firmware_version(char *firmware_version)
{
#ifdef BD_FEATURE_ENABLE_OTA
    struct FirmwareInfo *firmware_info = NULL;

    firmware_info = (struct FirmwareInfo *)(OTA_FW_INFO_ADDR);
    strncpy(firmware_version, (const char*)firmware_info->version, FIRMWARE_VERSION_LEN);
#else
    strncpy(firmware_version, FIRMWARE_VERSION, FIRMWARE_VERSION_LEN);
#endif

    return DUER_OK;
}

static struct DevInfoOps dev_info_ops = {
    .get_firmware_version = get_firmware_version,
};

int duer_init_device_info(void)
{
    int ret = DUER_OK;

    ret = duer_register_device_info_ops(&dev_info_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Dev Info: Register dev ops failed");
    }

    return ret;
}
