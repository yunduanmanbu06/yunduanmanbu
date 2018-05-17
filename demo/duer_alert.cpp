// Copyright (2017) Baidu Inc. All rights reserved.
/**
 * File: duer_alert.cpp
 * Auth: Gang Chen(chengang12@baidu.com)
 * Desc: Duer alert functions.
 */

#include "duer_alert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mbed.h"
#include "heap_monitor.h"
#include "lightduer_dcs_alert.h"
#include "lightduer_net_ntp.h"
#include "lightduer_types.h"
#include "lightduer_dcs_alert.h"
#include "lightduer_log.h"
#include "lightduer_timers.h"
#include "baidu_media_manager.h"
#include "baidu_iot_mutex.h"
#include "baidu_measure_stack.h"

typedef struct {
    duer_dcs_alert_info_type alert_info;
    duer_timer_handler handle;
    bool is_active;
} duerapp_alert_data;

typedef struct _duer_alert_list_node {
    struct _duer_alert_list_node *next;
    duerapp_alert_data *data;
} duer_alert_list_node_t;

static duer_alert_list_node_t s_alert_list;
static const int ALERT_QUEUE_SIZE = 5;
static const int ALERT_TREAD_STACK_SIZE = 1024 * 4;
static rtos::Queue<duerapp_alert_data, ALERT_QUEUE_SIZE> s_message_q;
static rtos::Thread s_alert_thread(osPriorityNormal, ALERT_TREAD_STACK_SIZE);
static iot_mutex_t s_alert_mutex;
static const char *BAIDU_ALERT_MP3_FILE = "/sd/alert.mp3";
using duer::MediaPlayerListener;

class AlertMeaidaPlayerListener : public MediaPlayerListener {
public:
    virtual int on_start(int flags);

    virtual int on_stop(int flags);

    virtual int on_finish(int flags);
};

static AlertMeaidaPlayerListener s_alert_media_listener;

static duerapp_alert_data *duer_find_active_alert()
{
    duer_alert_list_node_t *node = s_alert_list.next;

    while (node) {
        if (node->data) {
            if (node->data->is_active) {
                return node->data;
            }
        }

        node = node->next;
    }

    return NULL;
}

static void duer_check_active_alert(duer_dcs_alert_event_type event)
{
    duerapp_alert_data *alert = NULL;

    iot_mutex_lock(s_alert_mutex, 0);
    alert = duer_find_active_alert();
    if (alert) {
        duer_dcs_report_alert_event(alert->alert_info.token, event);
        if (event == ALERT_STOP) {
            alert->is_active = false;
        }
    }
    iot_mutex_unlock(s_alert_mutex);
}

int AlertMeaidaPlayerListener::on_start(int flags)
{
    duer_check_active_alert(ALERT_START);

    return 0;
}

int AlertMeaidaPlayerListener::on_stop(int flags)
{
    duer_check_active_alert(ALERT_STOP);

    return 0;
}

int AlertMeaidaPlayerListener::on_finish(int flags)
{
    duer_check_active_alert(ALERT_STOP);

    return 0;
}

static duer_errcode_t duer_alert_list_push(duerapp_alert_data *data)
{
    duer_errcode_t rt = DUER_OK;
    duer_alert_list_node_t *new_node = NULL;
    duer_alert_list_node_t *tail = &s_alert_list;

    new_node = (duer_alert_list_node_t *)MALLOC(sizeof(duer_alert_list_node_t), ALARM);
    if (!new_node) {
        DUER_LOGE("Memory too low");
        rt = DUER_ERR_MEMORY_OVERLOW;
        goto error_out;
    }
    new_node->next = NULL;
    new_node->data = data;

    while (tail->next) {
        tail = tail->next;
    }

    tail->next = new_node;

error_out:
    return rt;
}

static duer_errcode_t duer_alert_list_remove(duerapp_alert_data *data)
{
    duer_alert_list_node_t *pre = &s_alert_list;
    duer_alert_list_node_t *cur = NULL;
    duer_errcode_t rt = DUER_ERR_FAILED;

    while (pre->next) {
        cur = pre->next;
        if (cur->data == data) {
            pre->next = cur->next;
            FREE(cur);
            rt = DUER_OK;
            break;
        }
        pre = pre->next;
    }

    return rt;
}

static char *duerapp_alert_strdup(const char *str)
{
    int len = 0;
    char *dest = NULL;

    if (!str) {
        return NULL;
    }

    len = strlen(str);
    dest = (char *)MALLOC(len + 1, ALARM);
    if (!dest) {
        return NULL;
    }

    snprintf(dest, len + 1, "%s", str);
    return dest;
}

static void duer_free_alert_data(duerapp_alert_data *alert)
{
    if (alert) {
        if (alert->alert_info.type) {
            FREE(alert->alert_info.type);
            alert->alert_info.type = NULL;
        }

        if (alert->alert_info.time) {
            FREE(alert->alert_info.time);
            alert->alert_info.time = NULL;
        }

        if (alert->alert_info.token) {
            FREE(alert->alert_info.token);
            alert->alert_info.token = NULL;
        }

        if (alert->handle) {
            duer_timer_release(alert->handle);
            alert->handle = NULL;
        }

        FREE(alert);
    }
}

static duerapp_alert_data *duer_create_alert_data(const duer_dcs_alert_info_type *alert_info,
                                                  duer_timer_handler handle)
{
    duerapp_alert_data *alert = NULL;

    alert = (duerapp_alert_data *)MALLOC(sizeof(duerapp_alert_data), ALARM);
    if (!alert) {
        goto error_out;
    }

    memset(alert, 0, sizeof(duerapp_alert_data));

    alert->alert_info.type = duerapp_alert_strdup(alert_info->type);
    if (!alert->alert_info.type) {
        goto error_out;
    }

    alert->alert_info.time = duerapp_alert_strdup(alert_info->time);
    if (!alert->alert_info.time) {
        goto error_out;
    }

    alert->alert_info.token = duerapp_alert_strdup(alert_info->token);
    if (!alert->alert_info.token) {
        goto error_out;
    }

    alert->handle = handle;

    return alert;

error_out:
    DUER_LOGE("Memory too low");
    duer_free_alert_data(alert);

    return NULL;
}

static void duer_alert_callback(void *param)
{
    duerapp_alert_data *alert = (duerapp_alert_data *)param;

    DUER_LOGI("alert started: token: %s\n", alert->alert_info.token);

    alert->is_active = true;
    // play url
    duer::MediaManager::instance().play_local(BAIDU_ALERT_MP3_FILE);
}

void duer_dcs_alert_set_handler(const duer_dcs_alert_info_type *alert_info)
{
    duerapp_alert_data* alert = NULL;

    DUER_LOGI("set alert: scheduled_time: %s, token: %s\n", alert_info->time, alert_info->token);

    alert = duer_create_alert_data(alert_info, NULL);
    if (!alert) {
        duer_dcs_report_alert_event(alert_info->token, SET_ALERT_FAIL);
    } else {
        // create alert in duer_alert_thread and return immediately
        s_message_q.put(alert, 1);
    }
}

static duerapp_alert_data *duer_find_target_alert(const char *token)
{
    duer_alert_list_node_t *node = s_alert_list.next;

    while (node) {
        if (node->data) {
            if (strcmp(token, node->data->alert_info.token) == 0) {
                return node->data;
            }
        }

        node = node->next;
    }

    return NULL;
}

void duer_dcs_get_all_alert(baidu_json *alert_list)
{
    duer_alert_list_node_t *node = s_alert_list.next;

    while (node) {
        duer_insert_alert_list(alert_list, &node->data->alert_info, node->data->is_active);
        node = node->next;
    }
}

static time_t duer_dcs_get_time_stamp(const char *time)
{
    time_t time_stamp = 0;
    struct tm time_tm;
    int rs = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;

    rs = sscanf(time, "%d-%d-%dT%d:%d:%d+", &year, &month, &day, &hour, &min, &sec);
    if (rs != 6) {
        return (time_t)-1;
    }

    time_tm.tm_year  = year - 1900;
    time_tm.tm_mon   = month - 1;
    time_tm.tm_mday  = day;
    time_tm.tm_hour  = hour;
    time_tm.tm_min   = min;
    time_tm.tm_sec   = sec;
    time_tm.tm_isdst = 0;

    time_stamp = mktime(&time_tm);
    return time_stamp;
}

/**
 * We use ntp to get current time, it might spend too long time and block the thread,
 * hence we use a new thread to create alert.
 */
static void duer_alert_thread()
{
    DuerTime cur_time;
    size_t delay = 0;
    int rs = DUER_OK;
    duerapp_alert_data *alert = NULL;
    uint32_t time = 0;

    while (1) {
        if (rs != DUER_OK) {
            duer_dcs_report_alert_event(alert->alert_info.token, SET_ALERT_FAIL);
            duer_free_alert_data(alert);
        }

        osEvent evt = s_message_q.get();

        if (evt.status != osEventMessage) {
            continue;
        }

        alert = (duerapp_alert_data*)evt.value.p;

        time = duer_dcs_get_time_stamp(alert->alert_info.time);
        if (time < 0) {
            rs = DUER_ERR_FAILED;
            continue;
        }

        rs = duer_ntp_client(NULL, 5000, &cur_time);
        if (rs < 0) {
            DUER_LOGE("Failed to get NTP time\n");
            rs = DUER_ERR_FAILED;
            continue;
        }

        if (time <= cur_time.sec) {
            DUER_LOGE("The alert is expired\n");
            rs = DUER_ERR_FAILED;
            continue;
        }

        delay = (time - cur_time.sec) * 1000 - cur_time.usec / 1000;
        alert->handle = duer_timer_acquire(duer_alert_callback,
                                           alert,
                                           DUER_TIMER_ONCE);
        if (!alert->handle) {
            DUER_LOGE("Failed to create timer\n");
            rs = DUER_ERR_FAILED;
            continue;
        }

        rs = duer_timer_start(alert->handle, delay);
        if (rs != DUER_OK) {
            DUER_LOGE("Failed to start timer\n");
            rs = DUER_ERR_FAILED;
            continue;
        }

        /*
         * The alerts is storaged in the ram, hence the alerts will be lost after close the device.
         * You could stoage them into flash or sd card, and restore them after restart the device
         * according to your request.
         */
        iot_mutex_lock(s_alert_mutex, 0);
        duer_alert_list_push(alert);
        duer_dcs_report_alert_event(alert->alert_info.token, SET_ALERT_SUCCESS);
        iot_mutex_unlock(s_alert_mutex);
        rs = DUER_OK;
    }
}

void duer_dcs_alert_delete_handler(const char *token)
{
    duerapp_alert_data *target_alert = NULL;

    DUER_LOGI("delete alert: token %s", token);

    iot_mutex_lock(s_alert_mutex, 0);

    target_alert = duer_find_target_alert(token);
    if (!target_alert) {
        duer_dcs_report_alert_event(token, DELETE_ALERT_FAIL);
        iot_mutex_unlock(s_alert_mutex);
        DUER_LOGE("Cannot find the target alert\n");
        return;
    }

    duer_alert_list_remove(target_alert);
    duer_dcs_report_alert_event(token, DELETE_ALERT_SUCCESS);

    iot_mutex_unlock(s_alert_mutex);

    duer_free_alert_data(target_alert);
}

void duer_alert_init()
{
    static bool is_first_time = true;

    /**
     * The init function might be called sevaral times when ca re-connect,
     * but some operations only need to be done once.
     */
    if (is_first_time) {
        is_first_time = false;
        s_alert_mutex = iot_mutex_create();
        s_alert_thread.start(duer_alert_thread);
#ifdef BAIDU_STACK_MONITOR
        register_thread(&s_alert_thread, "duer_alert_thread");
#endif
    }

    duer::MediaManager::instance().register_listener(&s_alert_media_listener);

    duer_dcs_alert_init();
}

