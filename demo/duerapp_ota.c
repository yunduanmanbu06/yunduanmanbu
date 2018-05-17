// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_ota.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Demo
 */

#include <stdint.h>
#include <stdio.h>
#include "lightduer_log.h"
#include "rda5981_ota.h"
#include "rda_wdt_api.h"
#include "lightduer_types.h"
#include "duerapp_device_info.h"
#include "lightduer_ota_unpack.h"
#include "lightduer_ota_updater.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_ota_installer.h"

#define PAGE_SIZE 1024

struct rda_ota_update_context {
    duer_ota_updater_t *updater;
    int address;
    duer_ota_module_info_t info;
};

static unsigned char cache[PAGE_SIZE];
static unsigned int left_size = 0;
static unsigned int last_addr = 0;
static unsigned int available_partition = 0;

static duer_package_info_t s_package_info = {
    .product = "Demo",
    .batch   = "12",
    .os_info = {
        .os_name = "Mbed OS",
        .developer = "Allen",
    }
};

static int ota_notify_data_begin(void *ctx)
{
    memset(cache, 0xff, PAGE_SIZE);
    left_size = 0;
    last_addr = 0;
    available_partition = 0;

    return DUER_OK;
}

static int ota_notify_meta_data(void *cxt, duer_ota_package_meta_data_t *meta)
{
    int status = DUER_OK;
    unsigned int upgrade_max_size;
    const unsigned int upgrade_start_address = OTA_UPGRADE_ADDR;
    struct rda_ota_update_context *context = NULL;
    if (meta == NULL || cxt == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    context = (struct rda_ota_update_context*)cxt;

    if (context && meta && meta->install_info.module_count > 0) {
        context->info = meta->install_info.module_list[0];
        upgrade_max_size = ((context->info.module_size) / 4096 + 1) * 4096; // PARTION_SIZE;
        if (upgrade_max_size > OTA_UPGRADE_AREA_SIZE) {
            DUER_LOGE("upgrade size: 0x%x is greater than the max size:0x%x \n",
                    upgrade_max_size,
                    OTA_UPGRADE_AREA_SIZE);
            status = -1;

            return status;
        }
        context->address =  upgrade_start_address;   // write start position
        available_partition = upgrade_start_address;
        status = rda5981_write_partition_start(upgrade_start_address, upgrade_max_size);
        last_addr = 0;
        DUER_LOGI("available parition address:0x%08x, start result:%d\n",
                available_partition,
                status);
    }

    return status;
}

static int ota_notify_module_data(
        void *cxt,
        unsigned int offset,
        unsigned char *data,
        unsigned int size)
{
    int status = -1;
    int ret = DUER_OK;
    unsigned int copy;
    struct rda_ota_update_context *context = NULL;
    if (cxt == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    context = (struct rda_ota_update_context *)cxt;

    if (context && (last_addr + PAGE_SIZE) <= OTA_UPGRADE_AREA_SIZE) {
        if ((left_size + size) < PAGE_SIZE) {
            DUER_LOGI("cache %d bytes from 0x%8x\n", size, (context->address + offset));
            memcpy(cache + left_size, data, size);
            left_size += size;
            data += size;
            size = 0;
            status = 0;
        } else {
            copy = PAGE_SIZE - left_size;
            memcpy(cache + left_size, data, copy);
            status = rda5981_write_partition(last_addr, cache, PAGE_SIZE);
            DUER_LOGI("%d bytes written from 0x%8x, result:%d\n", PAGE_SIZE, last_addr,(int)status);
            last_addr += PAGE_SIZE;

            memcpy(cache, data + copy, size - copy);
            left_size = size - copy;
            data += size;
            size = 0;
        }
    }
out:
    return ret;
}

static int ota_notify_data_end(void *ctx)
{
    int status = -1;
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: notify data end");

    if (ctx == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    if (left_size > 0) {
        if ((last_addr + PAGE_SIZE) <= OTA_UPGRADE_AREA_SIZE) {
            status = rda5981_write_partition(last_addr, cache, PAGE_SIZE);
        }
    } else {
        status = 0;
    }
    DUER_LOGI("%d bytes written from 0x%8x, result:%d\n", left_size, last_addr,(int)status);

    rda5981_write_partition_end();
out:
    return ret;
}

static int ota_update_img_begin(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: update image begin");

    return ret;

}

static int ota_update_img(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: updating image");

    return ret ;
}

static int ota_update_img_end(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: update image end");

    return ret;
}

static duer_ota_installer_t updater = {
    .notify_data_begin  = ota_notify_data_begin,
    .notify_meta_data   = ota_notify_meta_data,
    .notify_module_data = ota_notify_module_data,
    .notify_data_end    = ota_notify_data_end,
    .update_img_begin   = ota_update_img_begin,
    .update_img         = ota_update_img,
    .update_img_end     = ota_update_img_end,
};

static int duer_ota_init_updater(duer_ota_updater_t *ota_updater)
{
    int ret = DUER_OK;
    struct rda_ota_update_context *ota_install_handle = NULL;

    ota_install_handle = (struct rda_ota_update_context *)malloc(sizeof(*ota_install_handle));
    if (ota_install_handle == NULL) {

        DUER_LOGE("OTA Unpack OPS: Malloc failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    ota_install_handle->updater = ota_updater;

    ret = duer_ota_unpack_register_installer(ota_updater->unpacker, &updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Unpack OPS: Register updater failed ret:%d", ret);

        goto out;
    }

    ret = duer_ota_unpack_set_private_data(ota_updater->unpacker, ota_install_handle);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Unpack OPS: Set private data failed ret:%d", ret);
    }
out:
    return ret;
}

static int duer_ota_uninit_updater(duer_ota_updater_t *ota_updater)
{
    struct rda_ota_update_context *ota_install_handle = NULL;

    ota_install_handle = duer_ota_unpack_get_private_data(ota_updater->unpacker);
    if (ota_install_handle == NULL) {
        DUER_LOGE("OTA Unpack OPS: Get private data failed");

        return DUER_ERR_INVALID_PARAMETER;
    }

    free(ota_install_handle);

    return DUER_OK;
}

static int reboot(void *arg)
{
    int ret = DUER_OK;

    DUER_LOGE("OTA Unpack OPS: Prepare to restart system");

    rda_wdt_softreset();

    return ret;
}

static int get_package_info(duer_package_info_t *info)
{
    int ret = DUER_OK;
    struct ImageHeader *staged_header = NULL;
    char firmware_version[FIRMWARE_VERSION_LEN + 1];

    if (info == NULL) {
        DUER_LOGE("Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    memset(firmware_version, 0, sizeof(firmware_version));

    ret = duer_get_firmware_version(firmware_version, FIRMWARE_VERSION_LEN);
    if (ret != DUER_OK) {
        DUER_LOGE("Get firmware version failed");

        goto out;
    }

    strncpy((char *)&s_package_info.os_info.os_version,
            firmware_version,
            FIRMWARE_VERSION_LEN + 1);

    staged_header = (struct ImageHeader *)(OTA_UPGRADE_ADDR);
    strncpy((char *)&s_package_info.os_info.staged_version,
            (const char*)staged_header->version,
            NAME_LEN - 1);
    s_package_info.os_info.staged_version[NAME_LEN] = '\0';

    memcpy(info, &s_package_info, sizeof(*info));

out:
    return ret;
}

static duer_package_info_ops_t s_package_info_ops = {
    .get_package_info = get_package_info,
};

static duer_ota_init_ops_t s_ota_init_ops = {
    .init_updater = duer_ota_init_updater,
    .uninit_updater = duer_ota_uninit_updater,
    .reboot = reboot,
};

int duer_initialize_ota(void)
{
    int ret = DUER_OK;

    ret = duer_init_ota(&s_ota_init_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Init OTA failed");

        goto out;
    }

    ret = duer_ota_register_package_info_ops(&s_package_info_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Register OTA package info ops failed");
    }
out:
    return ret;
}
