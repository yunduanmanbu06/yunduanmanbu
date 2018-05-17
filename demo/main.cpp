// Copyright (2016) Baidu Inc. All rights reserved.
/**
 * File: main.cpp
 * Desc: Sample code for startup DuerOS
 */

#include "mbed.h"
#include "baidu_media_manager.h"
#include "baidu_media_play.h"
#include "duer_app.h"
#include "events.h"
#include "lightduer_log.h"
#if defined(TARGET_UNO_91H)
#include "SDMMCFileSystem.h"
#include "WiFiStackInterface.h"
#include "factory_test.h"
#include "gpadckey.h"
#ifdef RDA_SMART_CONFIG
#include "smart_config.h"
#endif // RDA_SMART_CONFIG
#elif defined(TARGET_K64F)
#include "SDFileSystem.h"
#include "EthernetInterface.h"
#else
#error "Not supported"
#endif // TARGET_UNO_91H
#include "lightduer_connagent.h"
#include "baidu_ca_network_socket.h"
#if defined(ENABLE_UPNP_RENDER)
#include "minirender.h"
#include "baidu_measure_stack.h"
#endif

#include "duerapp_ota.h"
#include "duerapp_device_info.h"
#include "lightduer_ota_notifier.h"
#include "duer_dcs.h"
#include "Rda58xx.h"
//#include "player.h"
extern rda58xx _rda58xx;

#if defined(TARGET_UNO_91H)

// Initialize SD card
SDMMCFileSystem g_sd(GPIO_PIN9, GPIO_PIN0, GPIO_PIN3, GPIO_PIN7, GPIO_PIN12, GPIO_PIN13, "sd");

static WiFiStackInterface s_net_stack;
#else
SDFileSystem g_sd = SDFileSystem(D11, D12, D13, D10, "sd");
static EthernetInterface s_net_stack;
#endif // TARGET_UNO_91H

const char* ip;
const char* mac;


//GpadcKey switch_button(KEY_A0);


//void switch_UART_MODE()
//{
//    _rda58xx.setMode(UART_MODE);
//	Thread::wait(100);
	
//	printf("switch_UART_MODE!\n");
	
//}


void* baidu_get_netstack_instance(void)
{
    return (void*)&s_net_stack;
}

#if defined(ENABLE_UPNP_RENDER)
void test_upnp_render(void const *argument) {

    Thread::wait(5000);

    printf("### test upnp start\n");

    upnp_test(ip, mac);

    printf("### test upnp end\n");
}
#endif

// main() runs in its own thread in the OS
int main()
{
#if defined(TARGET_UNO_91H)
    // Initialize RDA FLASH
    const unsigned int RDA_FLASH_SIZE     = 0x400000;   // Flash Size
    const unsigned int RDA_SYS_DATA_ADDR  = 0x18204000; // System Data Area, fixed size 4KB
    const unsigned int RDA_USER_DATA_ADDR = 0x18205000; // User Data Area start address
    const unsigned int RDA_USER_DATA_LENG = 0x3000;     // User Data Area Length
	printf("\nrda5981_set_flash >>>>\n");
    rda5981_set_flash_size(RDA_FLASH_SIZE);
    rda5981_set_user_data_addr(RDA_SYS_DATA_ADDR, RDA_USER_DATA_ADDR, RDA_USER_DATA_LENG);
	rda5981_erase_flash(0x18208000, 0x080000);
	printf("\nrda5981_set_flash finished! >>>>\n");
    // Test added by RDA
    factory_test();
#endif

    //DUER_LOGI("\nEntry Tinydu Main>>>>\n");
	printf("\nEntry Tinydu Main>>>>\n");


	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready\n");
        Thread::wait(100);
    	}
	_rda58xx.setMode(BT_MODE);
	if (BT_MODE == _rda58xx.getMode())
	{
			printf("BT_MODE!\n");

	}
	if (UART_MODE == _rda58xx.getMode())
	{
			printf("UART_MODE!\n");

	}
	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready 130\n");
        Thread::wait(100);
    	}
    printf("Codec Ready 132\n");
	Thread::wait(1000);

	_rda58xx.setMode(UART_MODE);
	if (BT_MODE == _rda58xx.getMode())
		{
			printf("BT_MODE!\n");

		}
	if (UART_MODE == _rda58xx.getMode())
		{
			printf("UART_MODE!\n");

		}

	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready 150\n");
        Thread::wait(100);
    	}
    printf("Codec Ready 153\n");
	Thread::wait(1000);
/*
	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready\n");
        Thread::wait(100);
    	}
     


	_rda58xx.setMode(BT_MODE);
	if (BT_MODE == _rda58xx.getMode())
		{
			printf("BT_MODE!\n");

		}
	if (UART_MODE == _rda58xx.getMode())
		{
			printf("UART_MODE!\n");

		}

	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready 130\n");
        Thread::wait(100);
    	}
    printf("Codec Ready 132\n");
	Thread::wait(1000);

	_rda58xx.setMode(UART_MODE);
	if (BT_MODE == _rda58xx.getMode())
		{
			printf("BT_MODE!\n");

		}
	if (UART_MODE == _rda58xx.getMode())
		{
			printf("UART_MODE!\n");

		}

	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready 150\n");
        Thread::wait(100);
    	}
    printf("Codec Ready 153\n");
	Thread::wait(1000);

	_rda58xx.setMode(BT_MODE);
	if (BT_MODE == _rda58xx.getMode())
		{
			printf("BT_MODE!\n");

		}
	if (UART_MODE == _rda58xx.getMode())
		{
			printf("UART_MODE!\n");

		}

	while(!_rda58xx.isReady()) 
	{
        printf("Codec Not Ready 130\n");
        Thread::wait(100);
    	}
    printf("Codec Ready 132\n");
	Thread::wait(1000);
		
		//mbed::GpadcKey key_erase = mbed::GpadcKey(KEY_A0);
    		switch_button.fall(&(switch_UART_MODE));
		_rda58xx.setMode(UART_MODE);
		
	if (BT_MODE == _rda58xx.getMode())
	{
			printf("BT_MODE!\n");

	}
	if (UART_MODE == _rda58xx.getMode())
	{
			printf("UART_MODE!\n");

	}

	while(!_rda58xx.isReady()) 
	{

		printf("Codec Not Ready 130\n");
        	Thread::wait(100);
    	}

		printf("Codec Ready 132\n");
		Thread::wait(1000);
	
		
		//break;


	

	
	//Player player;
	//player.begin();
	//while(!player.isReady()) 
	{       
	//printf("Codec Not Ready\n");        
	//Thread::wait(1000);    
	}    
	//printf("Codec Ready\n");
	//player.setBtMode();
*/
	while(!_rda58xx.isReady()) 
	{

		printf("Codec Not Ready 130\n");
        	Thread::wait(100);
    	}

		printf("Codec Ready 132\n");
		Thread::wait(1000);
    // Brings up the network interface
#ifdef RDA_SMART_CONFIG
    typedef void (*dummy_func)();
	printf("dummy_func\n");
    mbed::GpadcKey key_erase = mbed::GpadcKey(KEY_A2);
    key_erase.fall((dummy_func)rda5981_flash_erase_sta_data);
	printf("key_erase.fall\n");
    int ret = 0;
    ret = s_net_stack.scan(NULL, 0);//necessary
	printf("smartconfig_ssid\n");
    char smartconfig_ssid[duer::RDA_SMARTCONFIG_SSID_LENGTH];
    char smartconfig_password[duer::RDA_SMARTCONFIG_PASSWORD_LENGTH];
    ret = duer::smart_config(smartconfig_ssid, smartconfig_password);
	printf("smartconfig_password\n");
	
    if (ret == 0 && s_net_stack.connect(smartconfig_ssid, smartconfig_password) == 0) {
#elif defined(TARGET_UNO_91H)

	printf("defined(TARGET_UNO_91H) 12312\n");
	int ret = 0;
	ret = s_net_stack.connect(CUSTOM_SSID, CUSTOM_PASSWD);
	printf("s_net_stack.connect(CUSTOM_SSID, CUSTOM_PASSWD) :%D \n" ,ret );
	
	printf("if (s_net_stack.connect(CUSTOM_SSID, CUSTOM_PASSWD) == 0)\n");
    //if (s_net_stack.connect(CUSTOM_SSID, CUSTOM_PASSWD) == 0) 
    	//ret = 0;
	
    if (ret == 0) 
	{
	printf("s_net_stack.connect\n");
#else
	printf("if (s_net_stack.connect() == 0)\n");
    if (s_net_stack.connect() == 0) {
	printf("s_net_stack.connect() == 0\n");
#endif // RDA_SMART_CONFIG
	printf(" ip = s_net_stack.get_ip_address();\n");
        ip = s_net_stack.get_ip_address();
        mac = s_net_stack.get_mac_address();
        DUER_LOGI("IP address is: %s", ip ? ip : "No IP");
        DUER_LOGI("MAC address is: %s", mac ? mac : "No MAC");
    	} 

	else 
	{		
	 printf("Network initial failed....");
        DUER_LOGE("Network initial failed....");
	while(!_rda58xx.isReady()) 
	{

		printf("Codec Not Ready 130\n");
        	Thread::wait(100);
    	}

		printf("Codec Ready 132\n");
        //Thread::wait(osWaitForever);
        _rda58xx.setMode(BT_MODE);
		//while(1);
		if (BT_MODE == _rda58xx.getMode())
	{
			printf("BT_MODE!\n");

	}
	if (UART_MODE == _rda58xx.getMode())
	{
			printf("UART_MODE!\n");

	}
	while(1);
        //Thread::wait(1000); 
    	}
	printf("Initialize the Duer OS....");
    // Initialize the Duer OS
    duer_initialize();
    duer_init_device_info();

    duer::MediaManager::instance().initialize();

    duer::SocketAdapter::set_network_interface(&s_net_stack);

    duer::MediaManager::instance().set_volume(duer::DEFAULT_VOLUME);

    duer::DuerApp app;
    app.start();

    duer_set_duerapp_instance(&app);

#if defined(ENABLE_UPNP_RENDER)
    Thread thread(test_upnp_render, NULL, osPriorityNormal, 10*1024);
#if defined(BAIDU_STACK_MONITOR)
    register_thread(&thread, "DLNA");
#endif
#endif

    duer::event_loop();
}
