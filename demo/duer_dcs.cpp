// Copyright (2016) Baidu Inc. All rights reserved.
/**
 * File: duer_dcs.cpp
 * Desc:
 */

#include "duer_dcs.h"
#include "lightduer_dcs.h"
#include "duer_alert.h"
#include "baidu_media_manager.h"
#include "baidu_media_play.h"
#include "lightduer_log.h"
#include "duer_app.h"
#include "lightduer_voice.h"

static unsigned char s_volume = duer::DEFAULT_VOLUME;
static bool s_is_mute = false;
static duer::DuerApp *s_duerapp;
static const int UNIT_VOLUME = (duer::MAX_VOLUME - duer::MIN_VOLUME) / 14;
static bool s_is_audio_playing = false;
static int s_media_flag;

void duer_dcs_volume_set_handler(int volume)
{
    DUER_LOGV("Entry");

    if ((volume <= duer::MAX_VOLUME) && (volume >= duer::MIN_VOLUME)) {
        s_volume = volume;
        duer::MediaManager::instance().set_volume(volume);
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_volume_adjust_handler(int volume)
{
    DUER_LOGV("Entry");

    int target_volume = s_volume + volume;

    if (target_volume < duer::MIN_VOLUME) {
        target_volume = duer::MIN_VOLUME;
    }

    if (target_volume > duer::MAX_VOLUME) {
        target_volume = duer::MAX_VOLUME;
    }

    duer_dcs_volume_set_handler(target_volume);
}

void duer_dcs_mute_handler(bool is_mute)
{
    DUER_LOGV("Entry");

    if (s_is_mute == is_mute) {
        return;
    }

    s_is_mute = is_mute;

    if (is_mute) {
        duer::MediaManager::instance().set_volume(duer::MIN_VOLUME);
    } else {
        duer::MediaManager::instance().set_volume(s_volume);
    }

    duer_dcs_on_mute();
}

void duer_dcs_get_speaker_state(int *volume, bool *is_mute)
{
    DUER_LOGV("Entry");

    if (!volume || !is_mute) {
        return;
    }

    *volume = s_volume;
    *is_mute = s_is_mute;
}

void duer_dcs_listen_handler()
{
    DUER_LOGV("Entry");
    // Currently, don't open microphtone automatically
    //s_duerapp->talk_start();
}

void duer_dcs_stop_listen_handler()
{
    DUER_LOGV("Entry");

    s_duerapp->talk_stop();
}

void duer_dcs_speak_handler(const char *url)
{
    DUER_LOGV("Entry");

    int flags = duer::MEDIA_FLAG_SPEECH | duer::MEDIA_FLAG_SAVE_PREVIOUS;
    duer::MediaManager::instance().play_url(url, flags);
}

void duer_dcs_audio_play_handler(const char *url, const int offset)
{
    DUER_LOGV("Entry");

    if (offset > 0) {
        duer::MediaManager::instance().play_previous_media_continue();
    } else {
        int flags = duer::MEDIA_FLAG_DCS_URL;
        duer::MediaManager::instance().play_url(url, flags);
    }
    s_is_audio_playing = true;
}

void duer_dcs_audio_stop_handler()
{
    DUER_LOGV("Entry");

    if (s_is_audio_playing) {
        duer::MediaManager::instance().stop();
        s_is_audio_playing = false;
    }
}

int duer_dcs_audio_get_play_progress(void)
{
    return duer::MediaManager::instance().get_play_progress();
}

void duer_dcs_audio_pause_handler()
{
    DUER_LOGV("Entry");

    if (s_is_audio_playing) {
        duer::MediaManager::instance().stop();
        s_is_audio_playing = false;
    }
}

void duer_dcs_audio_resume_handler(const char* url, const int offset)
{
    DUER_LOGV("Entry");

    duer::MediaManager::instance().play_previous_media_continue();
    s_is_audio_playing= true;
}

duer_status_t duer_dcs_input_text_handler(const char *text, const char *type)
{
    DUER_LOGI("ASR result: %s", text);

    return DUER_OK;
}

duer_status_t duer_dcs_render_card_handler(baidu_json *payload)
{
    baidu_json *type = NULL;
    baidu_json *content = NULL;
    duer_status_t ret = DUER_OK;

    do {
        if (!payload) {
            ret = DUER_ERR_FAILED;
            break;
        }

        type = baidu_json_GetObjectItem(payload, "type");
        if (!type) {
            ret = DUER_ERR_FAILED;
            break;
        }

        if (strcmp("TextCard", type->valuestring) == 0) {
            content = baidu_json_GetObjectItem(payload, "content");
            if (!content)  {
                ret = DUER_ERR_FAILED;
                break;
            }
            DUER_LOGI("Render card content: %s", content->valuestring);
        }
    } while (0);

    return ret;
}

namespace duer
{

void voice_up()
{
    if (s_volume < duer::MAX_VOLUME) {
        s_volume = s_volume + UNIT_VOLUME > duer::MAX_VOLUME ?
                   duer::MAX_VOLUME : s_volume + UNIT_VOLUME;
        duer_dcs_volume_set_handler(s_volume);
    }
}

void voice_down()
{
    if (s_volume > duer::MIN_VOLUME) {
        s_volume = s_volume - UNIT_VOLUME < duer::MIN_VOLUME ?
                   duer::MIN_VOLUME : s_volume - UNIT_VOLUME;
        duer_dcs_volume_set_handler(s_volume);
    }
}

void dcs_test(void)
{
    static int i = 0;

    switch (i % 5) {
    case 0:
        duer_dcs_send_play_control_cmd(DCS_NEXT_CMD);
        break;
    case 1:
        duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD);
        break;
    case 2:
        duer_dcs_close_multi_dialog();
        duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD);
        duer_voice_set_mode(DUER_VOICE_MODE_CHINESE_TO_ENGLISH);
        break;
    case 3:
        duer_dcs_close_multi_dialog();
        duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD);
        duer_voice_set_mode(DUER_VOICE_MODE_ENGLISH_TO_CHINESE);
        break;
    case 4:
        duer_voice_set_mode(DUER_VOICE_MODE_DEFAULT);
        break;
    default:
        break;
    }

    i++;
}

void duer_set_duerapp_instance(void *duerapp)
{
    s_duerapp = (DuerApp *)duerapp;
}

class DcsMeaidaPlayerListener : public MediaPlayerListener {
public:
    virtual int on_start(int flags);

    virtual int on_stop(int flags);

    virtual int on_finish(int flags);
};

static DcsMeaidaPlayerListener s_dcs_media_listener;

int DcsMeaidaPlayerListener::on_start(int flags)
{
    s_media_flag = flags;

    return 0;
}

int DcsMeaidaPlayerListener::on_stop(int flags)
{
    return 0;
}

int DcsMeaidaPlayerListener::on_finish(int flags)
{
    if (flags & MEDIA_FLAG_DCS_URL) {
        s_is_audio_playing = false;
        duer_dcs_audio_on_finished();
    } else if (flags & MEDIA_FLAG_SPEECH) {
        duer_dcs_speech_on_finished();
    } else {
        // do nothing
    }

    return 0;
}

static void duer_media_stuttuered_cb(bool is_stuttuerd, int flags)
{
    if (flags & MEDIA_FLAG_DCS_URL) {
        duer_dcs_audio_on_stuttered(is_stuttuerd);
    }
}

void dcs_pause_play(void)
{
    static bool is_paused = false;

    // Report DCS_PLAY_CMD/DCS_PAUSE_CMD event if dcs audio is playing,
    // then DCS server will dispatch directive to play/pause audio player.
    if (s_media_flag & MEDIA_FLAG_DCS_URL) {
        if (is_paused) {
            duer_dcs_send_play_control_cmd(DCS_PLAY_CMD);
            is_paused = false;
        } else {
            duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD);
            is_paused = true;
        }
    } else {
        MediaManager::instance().pause_or_resume();
    }
}

void duer_dcs_initialize()
{
    static bool is_first_time = true;

    duer_dcs_framework_init();
    duer_dcs_voice_input_init();
    duer_dcs_voice_output_init();
    duer_dcs_audio_player_init();
    duer_dcs_speaker_control_init();
    duer_dcs_screen_init();
#ifdef ENABLE_ALERT
    duer_alert_init();
#endif

    duer::MediaManager::instance().register_stuttered_cb(duer_media_stuttuered_cb);

    if (is_first_time) {
        is_first_time = false;
        MediaManager::instance().register_listener(&s_dcs_media_listener);
        // report SynchronizeState event when device boot
        duer_dcs_sync_state();
    }
}

} // namespace duer
