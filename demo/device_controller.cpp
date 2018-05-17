// Copyright (2016) Baidu Inc. All rights reserved.
/**
 * File: device_controller.cpp
 * Desc: Demo for control device.
 */
#include "device_controller.h"
#include "baidu_ca_scheduler.h"
#include "baidu_media_manager.h"
#include "lightduer_log.h"
#include "lightduer_dcs.h"

namespace duer
{

#if defined(TEST_BOARD)

static duer_status_t set_volume(duer_context ctx, duer_msg_t* msg, duer_addr_t* addr)
{
    duer_handler handler = (duer_handler)ctx;
    int volume = 0;
    bool is_mute = 0;
    int msg_code = DUER_MSG_RSP_CHANGED;
    const int LEN = 4;
    char str_volume[LEN] = {0};
    DUER_LOGV("set_volume");

    if (handler && msg) {
        if (msg->msg_code == DUER_MSG_REQ_GET) {
            duer_dcs_get_speaker_state(&volume, &is_mute);
            DUER_LOGI("volume get: %d", volume);
            snprintf(str_volume, LEN, "%d", volume);
        } else if (msg->msg_code == DUER_MSG_REQ_PUT) {
            if (msg->payload && msg->payload_len > 0 && msg->payload_len < LEN) {
                snprintf(str_volume, LEN, "%s", (char*)msg->payload);
                str_volume[msg->payload_len] = 0;
                DUER_LOGI("volume set: %s, len: %d", str_volume, msg->payload_len);
                volume = atoi(str_volume);
                duer_dcs_volume_set_handler(volume);
            } else {
                msg_code = DUER_MSG_RSP_FORBIDDEN;
                str_volume[0] = 0;
                DUER_LOGE("volume set invalid");
            }
        }

        Scheduler::instance().response(msg, msg_code, str_volume);
    }

    return DUER_OK;
}

static duer_status_t play_control(duer_context ctx, duer_msg_t* msg, duer_addr_t* addr)
{
    duer_handler handler = (duer_handler)ctx;
    int msg_code = DUER_MSG_RSP_CHANGED;
    const int LEN = 10;
    char command_str[LEN] = {0};
    DUER_LOGV("play_control");

    if (handler && msg && msg->msg_code == DUER_MSG_REQ_PUT) {
        if (msg->payload && msg->payload_len > 0 && msg->payload_len < LEN) {
            snprintf(command_str, LEN, "%s", (char*)msg->payload);
            command_str[msg->payload_len] = 0;
            DUER_LOGI("Play control: %s", command_str);

            if (strncmp(command_str, "Previous", LEN) == 0) {
                duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD);
            } else if (strncmp(command_str, "Next", LEN) == 0) {
                duer_dcs_send_play_control_cmd(DCS_NEXT_CMD);
            } else {
                msg_code = DUER_MSG_RSP_FORBIDDEN;
                DUER_LOGE("Unknown command");
            }
        } else {
            msg_code = DUER_MSG_RSP_FORBIDDEN;
            DUER_LOGE("Command invalid");
        }

        Scheduler::instance().response(msg, msg_code, command_str);
    }

    return DUER_OK;
}

#endif

void device_controller_init(void)
{
#if defined(TEST_BOARD)
    duer_res_t res[] = {
        {DUER_RES_MODE_DYNAMIC, DUER_RES_OP_PUT | DUER_RES_OP_GET, "set_volume", set_volume},
        {DUER_RES_MODE_DYNAMIC, DUER_RES_OP_PUT, "play_control", play_control},
    };
    Scheduler::instance().add_controll_points(res, sizeof(res) / sizeof(res[0]));
#endif
}

} // namespace duer
