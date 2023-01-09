#ifndef __JTFILEMANAGER__
#define __JTFILEMANAGER__

#include <Arduino.h>

#include <DNSServer.h>

#include <WiFi.h>
#include <WiFiClient.h>
//#include <AsyncTCP.h>

//#define TEMPLATE_PLACEHOLDER '$'

#include <ESPAsyncWebServer.h>
//#include <AsyncHttpClient.h>

//#ifdef TEMPLATE_PLACEHOLDER
//#undef TEMPLATE_PLACEHOLDER
//#define TEMPLATE_PLACEHOLDER '$'
//#endif


#ifdef ESP8266
//#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
//#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

//#include <iostream>
#include <FS.h>
#include <LittleFS.h>

#include <Preferences.h>

//#include <string.h>
//using namespace std;

/*
//adjust these to your requirements
#define LOCAL_IP IPAddress(192, 168, 1, 15)
#define GATEWAY IPAddress(192, 168, 1, 1)
#define SUBNET_MASK IPAddress(255, 255, 255, 0)
#define DNS1 IPAddress(1, 1, 1, 1)
#define DNS2 IPAddress(9, 9, 9, 9)
*/

#define DEBUG_CONSOLE Serial
#ifdef DEBUG_CONSOLE
 #define DEBUG_BEGIN(x)     DEBUG_CONSOLE.begin (x)
 #define DEBUG_PRINT(x)     DEBUG_CONSOLE.print (x)
 #define DEBUG_PRINTDEC(x)     DEBUG_PRINT (x, DEC)
 #define DEBUG_PRINTLN(x)  DEBUG_CONSOLE.println (x)
 #define DEBUG_PRINTF(...) DEBUG_CONSOLE.printf(__VA_ARGS__)
#else
 #define DEBUG_BEGIN(x)
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTDEC(x)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINTF(...)
#endif 

#define WIFI_CONNECT_TIMEOUT 10000 // milliseconds

#define SOFT_AP_SSID "JiftBox"
#define MDNS_HOSTNAME "jiftbox"

//#define SSID "PhotoOrnament"
//#define MDNS_HOSTNAME "photoornament"


// all folders uploaded will be stored in IMG_ROOT
// if a folder with the same name as IMG_ROOT is uploaded all of its contents will be put in IMG_ROOT
// for example, if IMG_ROOT is /images and you upload a folder named images than the contents
// of images will be put in /images not /images/images
// if you upload a folder named photos it will be stored as /images/photos
// do not put a / at the end of IMG_ROOT
#define IMG_ROOT "/images"

void handle_folder_upload(AsyncWebServerRequest *, const String &, size_t, uint8_t *, size_t, bool);
void create_dirs(String);
void list_files(File, String);
void handle_file_list(void);
void delete_files(String, String);
void handle_delete_list(void);
bool attempt_connect();
String get_mode(void);
String get_ip(void);
String get_mdns_addr(void);
void wifi_AP(void);
bool wifi_connect(void);
void mdns_setup(void);
String processor(const String& var);
bool filterOnNotLocal(AsyncWebServerRequest *);
void web_server_initiate(void);
void fm_setup(uint16_t w, uint16_t h, String r);
void fm_loop(void);


#endif // __JTFILEMANAGER__
