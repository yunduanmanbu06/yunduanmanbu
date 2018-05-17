// Copyright (2016) Baidu Inc. All rights reserved.
/**
 * File: duer_app.cpp
 * Desc: Demo for how to start Duer OS.
 */

#include "duer_app.h"
#include "baidu_ca_scheduler.h"
#include "baidu_ca_network_socket.h"
#include "baidu_media_manager.h"
#include "lightduer_log.h"
#include "device_controller.h"
#include "events.h"
#if defined(TARGET_UNO_91H)
#include "gpadckey.h"
#include "WiFiStackInterface.h"
#endif

#include "baidu_json.h"
#include "heap_monitor.h"
// #include "baidu_json.h"
#include "lightduer_dcs.h"
#include "duer_dcs.h"
#ifdef RDA_SMART_CONFIG
#include "smart_config.h"
#endif // RDA_SMART_CONFIG

#include "duerapp_ota.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_voice.h"

namespace duer
{

#if defined(TARGET_UNO_91H)
#define LED_RED LED1
#define LED_GREEN LED2
#define LED_BLUE LED3

static GpadcKey s_button(KEY_A0);
static GpadcKey s_pause_button(KEY_A1);
static GpadcKey s_test(KEY_A5);
static GpadcKey s_key_up(KEY_A3);
static GpadcKey s_key_down(KEY_A4);

#else
#define DUER_APP_RECORDER_BUTTON  SW2
static mbed::InterruptIn s_button = mbed::InterruptIn(DUER_APP_RECORDER_BUTTON);
#endif

static const char BAIDU_DEV_CONNECT_SUCCESS_PROMPT_FILE[] = "http://dx.sc.chinaz.com/Files/DownLoad/sound1/201804/10027.mp3";
//static const char BAIDU_DEV_CONNECT_SUCCESS_PROMPT_FILE[] = "file://wdmycloud/music/cloud_connected.mp3";

//static const char BAIDU_DEV_CONNECT_SUCCESS_PROMPT_FILE[] = "/sd/cloud_connected.mp3";
static const char BAIDU_DEV_DISCONNECTED_PROMPT_FILE[] = "/sd/cloud_disconnected.mp3";
static const char BAIDU_DEV_RECONNECTWIFI_PROMPT_FILE[] = "/sd/cloud_reconnectwifi.mp3";
static const char BAIDU_DEV_RESTART_DEVICE_PROMPT_FILE[] = "/sd/cloud_restartdevice.mp3";

static const unsigned int RECONN_DELAY_MIN = 1000;
// after reconnect wifi fail, the min delay to retry connect, should less than RECONN_DELAY_MAX
static const unsigned int NEW_RECONN_DELAY = 32000;
static const unsigned int RECONN_DELAY_MAX = 32001;

static volatile bool s_is_recorder_busy = false;

#ifdef RDA_SMART_CONFIG
int smart_config(char* smartconfig_ssid, char* smartconfig_password) {
    int ret = 0;
    char ssid[RDA_SMARTCONFIG_SSID_LENGTH];
    char password[RDA_SMARTCONFIG_PASSWORD_LENGTH];

    ret = rda5981_flash_read_sta_data(ssid, password);
    if (ret == 0 && strlen(ssid)) {
        DUER_LOGI("get ssid from flash: ssid:%s, pass:%s", ssid, password);
    } else {
        ret = rda5981_getssid_from_smartconfig(ssid, password);
        if (ret != 0){
            DUER_LOGI("get ssid from smartconfig fail!");
            return -1;
        } else {
            DUER_LOGI("ssid:%s, password:%s", ssid, password);
            rda5981_flash_write_sta_data(ssid, password);
        }
    }

    memcpy(smartconfig_ssid, ssid, RDA_SMARTCONFIG_SSID_LENGTH);
    memcpy(smartconfig_password, password, RDA_SMARTCONFIG_PASSWORD_LENGTH);
    return 0;
}
#endif // RDA_SMART_CONFIG

class SchedulerEventListener : public Scheduler::IOnEvent {
public:
    SchedulerEventListener(DuerApp* app) :
        _app(app)
    {
        Scheduler::instance().set_on_event_listener(this);
        event_set_handler(EVT_RESTART, _app, &DuerApp::start);
    }

    virtual ~SchedulerEventListener()
    {
        event_set_handler(EVT_RESTART, NULL);
    }

    virtual int on_start()
    {
        DUER_LOGI("SchedulerEventListener::on_start");

        MEMORY_STATISTICS("Scheduler::on_start");

        device_controller_init();
        duer_dcs_initialize();

#ifdef BD_FEATURE_ENABLE_OTA
        int ret = duer_initialize_ota();
        if (ret != DUER_OK) {
            DUER_LOGE("Initialize OTA failed");
        }

        ret = duer_ota_set_reboot(ENABLE_REBOOT);
        if (ret != DUER_OK) {
            DUER_LOGE("Enable OTA reboot failed");
        }
#endif

        event_set_handler(EVT_KEY_REC_PRESS, _app, &DuerApp::talk_start);
        event_set_handler(EVT_KEY_REC_RELEASE, _app, &DuerApp::talk_stop);
        event_set_handler(EVT_KEY_PAUSE, &dcs_pause_play);
        event_set_handler(EVT_KEY_VOICE_UP, &voice_up);
        event_set_handler(EVT_KEY_VOICE_DOWN, &voice_down);
        event_set_handler(EVT_KEY_TEST, &dcs_test);

        _app->set_color(DuerApp::CYAN);


//		connect success by yhy play_data

//      MediaManager::instance().play_url(BAIDU_DEV_CONNECT_SUCCESS_PROMPT_FILE,0);	
			
//	  	MediaManager::instance().play_local(BAIDU_DEV_CONNECT_SUCCESS_PROMPT_FILE);//Á´½ÓÌáÊ¾Òô
		static char connet_buf[0x1000];
	
		//rda5981_read_flash(0x18288000 ,  connet_buf ,0x1000);
		
		//MediaManager::instance().play_data(connet_buf , 0x1000);
		
		rda5981_read_flash(0x18289000 ,  connet_buf ,0x1000);//connect success----0x18289000
		
		MediaManager::instance().play_data(connet_buf , 0x1000);
		//rda5981_read_flash(0x1828a000 ,  connet_buf ,0x1000);
		
		//MediaManager::instance().play_data(connet_buf , 0x1000);		
		

		
        _app->reset_delay();
        return 0;
    }

    virtual int on_stop()
    {
        _app->set_color(DuerApp::PURPLE);

        MediaManager::instance().stop();
        _app->talk_stop();

        event_set_handler(EVT_KEY_REC_PRESS, NULL);
        event_set_handler(EVT_KEY_REC_RELEASE, NULL);
        event_set_handler(EVT_KEY_PAUSE, NULL);
        event_set_handler(EVT_KEY_TEST, NULL);
        event_set_handler(EVT_KEY_VOICE_UP, NULL);
        event_set_handler(EVT_KEY_VOICE_DOWN, NULL);

        MEMORY_STATISTICS("Scheduler::on_stop");

        DUER_LOGI("SchedulerEventListener::on_stop");

        _app->restart();

        return 0;
    }

    virtual int on_action(const char* url, bool is_speech, bool is_continue_previous) {
        DUER_LOGI("SchedulerEventListener::on_action: %s", url);
        int flags = 0;
        if (is_speech) {
            flags |= MEDIA_FLAG_SPEECH;
        }

        if (is_continue_previous) {
            flags |= MEDIA_FLAG_SAVE_PREVIOUS;
        }

        _app->set_color(DuerApp::BLUE);
        MediaManager::instance().play_url(url, flags);
        return 0;
    }

    virtual int on_data(const char* data)
    {
        DUER_LOGV("SchedulerEventListener::on_data: %s", data);

        return 0;
    }

private:
    DuerApp* _app;
};

class RecorderListener : public Recorder::IListener {
public:
    RecorderListener(DuerApp* app) :
        _app(app),
        _start_send_data(false)
    {
    }

    virtual ~RecorderListener()
    {
    }

    virtual int on_start()
    {
        _app->set_color(DuerApp::RED);
        DUER_LOGI("RecorderObserver::on_start");
        MEMORY_STATISTICS("Recorder::on_start");
        return 0;
    }

    virtual int on_resume()
    {
        return 0;
    }

    virtual int on_data(const void* data, size_t size)
    {
        if (!_start_send_data) {
            _start_send_data = true;
        }

        DUER_LOGV("RecorderObserver::on_data: data = %p, size = %d", data, size);
        Scheduler::instance().send_content(data, size, false);
        return 0;
    }

    virtual int on_pause()
    {
        return 0;
    }

    virtual int on_stop()
    {
        if (_start_send_data) {
            Scheduler::instance().send_content(NULL, 0, false);
            _start_send_data = false;
        } else {
            MediaManager::instance().stop_completely();
        }

        s_is_recorder_busy = false;

        MEMORY_STATISTICS("Recorder::on_stop");
        DUER_LOGI("RecorderObserver::on_stop");
        _app->set_color(DuerApp::GREEN);
        return 0;
    }

private:
    DuerApp* _app;
    bool _start_send_data;
};

DuerApp::DuerApp()
    : _recorder_listener(new RecorderListener(this))
    , _on_event(new SchedulerEventListener(this))
#if !defined(TARGET_UNO_91H)
    , _indicate(LED_BLUE, LED_GREEN, LED_RED)
#endif
    , _timer(this, &DuerApp::restart_timeout, osTimerOnce)
    , _delay(RECONN_DELAY_MIN)
#if defined(TEST_BOARD)
    , _send_ticker(this, &DuerApp::send_timestamp, osTimerPeriodic)
#endif
{
    _recorder.set_listener(_recorder_listener);

#if !defined(TARGET_UNO_91H)
    _indicate = OFF;
#endif
}

DuerApp::~DuerApp()
{
    delete _recorder_listener;
    delete _on_event;
}

void DuerApp::restart_timeout()
{
    //DUER_LOGI("restart_timeout, tid:%p", Thread::gettid());
    event_trigger(EVT_RESTART);
}

void DuerApp::start()
{
    DUER_LOGI("Start the DuerApp");

    Scheduler::instance().start();

    s_button.fall(this, &DuerApp::button_fall_handle);
    s_button.rise(this, &DuerApp::button_rise_handle);

#if defined(TARGET_UNO_91H)
    s_key_up.fall(this, &DuerApp::voice_up_button_fall_handle);
    s_key_down.fall(this, &DuerApp::voice_down_button_fall_handle);
    s_pause_button.fall(this, &DuerApp::pause_button_fall_handle);
    s_test.fall(this, &DuerApp::test_handle);
#endif
}

void DuerApp::stop()
{
    s_button.fall(NULL);
    s_button.rise(NULL);
#if defined(TARGET_UNO_91H)
    s_pause_button.fall(NULL);
#endif
    Scheduler::instance().stop();
}

void DuerApp::reset_delay() {
    _delay = RECONN_DELAY_MIN;
}

void DuerApp::restart() {
    if (_delay > RECONN_DELAY_MAX) {
        // try to reconnect the wifi interface
        WiFiStackInterface* net_stack
                = (WiFiStackInterface*)SocketAdapter::get_network_interface();
        if (net_stack != NULL) {
            //MediaManager::instance().play_local(BAIDU_DEV_RECONNECTWIFI_PROMPT_FILE);
			static char connet_buf[0x1000];
	
			rda5981_read_flash(0x18288000 ,  connet_buf ,0x1000);//dis connect
		
			MediaManager::instance().play_data(connet_buf , 0x1000);
			
			
            net_stack->disconnect();
#ifdef RDA_SMART_CONFIG
            int ret = 0;
            ret = net_stack->scan(NULL, 0);//necessary

            char smartconfig_ssid[RDA_SMARTCONFIG_SSID_LENGTH];
            char smartconfig_password[RDA_SMARTCONFIG_PASSWORD_LENGTH];
            ret = smart_config(smartconfig_ssid, smartconfig_password);
            if (ret == 0 && net_stack->connect(smartconfig_ssid, smartconfig_password) == 0) {
#elif defined(TARGET_UNO_91H)
            if (net_stack->connect(CUSTOM_SSID, CUSTOM_PASSWD) == 0) {
#else
            if (net_stack->connect() == 0) {
#endif // RDA_SMART_CONFIG
                const char* ip = net_stack->get_ip_address();
                const char* mac = net_stack->get_mac_address();
                DUER_LOGI("IP address is: %s", ip ? ip : "No IP");
                DUER_LOGI("MAC address is: %s", mac ? mac : "No MAC");
                _delay = RECONN_DELAY_MIN;
            } else {
                _delay = NEW_RECONN_DELAY;
                DUER_LOGW("Network initial failed, retry...");
            }
        } else {
            MediaManager::instance().play_local(BAIDU_DEV_RESTART_DEVICE_PROMPT_FILE);
            DUER_LOGE("no net_stack got, try restart the device!!");
        }
    }
    if (_delay < RECONN_DELAY_MAX) {
        _timer.start(_delay);
        _delay <<= 1;
    } else {
        // due the wifi reconnect strategy, here will not enter
        MediaManager::instance().play_local(BAIDU_DEV_DISCONNECTED_PROMPT_FILE);
    }
}

void DuerApp::talk_start()
{
    // waiting for recorder thread finish
    if (s_is_recorder_busy) {
        _recorder.stop();
        Thread::wait(50);
        if (s_is_recorder_busy) {
            DUER_LOGE("Recorder is busy, failed to start listen");
            return;
        }
    }

    MediaManager::instance().stop();

    Scheduler::instance().clear_content();

    s_is_recorder_busy = true;
    duer_dcs_on_listen_started();
    _recorder.start();
}

void DuerApp::talk_stop()
{
    _recorder.stop();
    duer_dcs_on_listen_stopped();
}

void DuerApp::set_color(Color c)
{
#if !defined(TARGET_UNO_91H)
    _indicate = c;
#endif

    if (c == CYAN) {
        _delay = RECONN_DELAY_MIN;
#if defined(TEST_BOARD)
        _send_ticker.start(60 * 1000);//update interval to 1min
#endif
    } else if (c == PURPLE) {
#if defined(TEST_BOARD)
        _send_ticker.stop();
#endif
    }
}

void DuerApp::button_fall_handle()
{
    event_trigger(EVT_KEY_REC_PRESS);
}

void DuerApp::button_rise_handle()
{
    // It is unnecessary to close micphone if cloud VAD is used.
    if (duer_voice_get_mode() != DUER_VOICE_MODE_DEFAULT) {
        event_trigger(EVT_KEY_REC_RELEASE);
    }
}

void DuerApp::pause_button_fall_handle()
{
    event_trigger(EVT_KEY_PAUSE);
}

void DuerApp::test_handle()
{
    event_trigger(EVT_KEY_TEST);
}

void DuerApp::voice_up_button_fall_handle()
{
    event_trigger(EVT_KEY_VOICE_UP);
}

void DuerApp::voice_down_button_fall_handle()
{
    event_trigger(EVT_KEY_VOICE_DOWN);
}

#if defined(TEST_BOARD)
void DuerApp::send_timestamp()
{
    Object data;
    data.putInt("time", us_ticker_read());
    Scheduler::instance().report(data);
}
#endif

} // namespace duer
