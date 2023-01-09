#include "FileManager.h"

// TODO
// make sure not overwrite existing file
//
// multipart upload??
//
// delete folder
//
// delete everything
//
// move index.html code into program so can wipe LittleFS and not delete index.html
//
// add AP code and code to enter SSID and password and mdns name
//
// minimize index.html and use gzip to save space?
//#define index_html_gz_len 726
//const uint8_t index_html_gz[] PROGMEM = {
//0x1F, 0x8B, 0x08, 0x08, 0x0B, 0x87, 0x90, 0x57, 0x00, 0x03, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F,
//https://www.mischianti.org/2020/10/26/web-server-with-esp8266-and-esp32-byte-array-gzipped-pages-and-spiffs-2/
//can this be done automatically at compile time and saved in a .h file?
//
// should file upload be asynchronous
//
// can FatVolume functions can be used? has ls(), rmdir(), etc.
//
// is a / slash used for uploads on windows? will code break if uploaded from windows?
//
// clean up webpages. add polish to network.html


AsyncWebServer web_server(80);
//AsyncHttpClient ahClient;

DNSServer dnsServer;

Preferences preferences;


bool restart_needed = false;
bool station_mode = false;

IPAddress IP;
String mdns_host;

uint16_t width = 0;
uint16_t height = 0;
String ratio;


class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    String url = "http://";
    url += get_ip();
    url += "/network.html";
    request->redirect(url);
  }
};


void handle_folder_upload(AsyncWebServerRequest *request, const String &param_path, size_t index, uint8_t *data, size_t len, bool final) {

  //ESP.wdtDisable();

  static File fs_file; //new file to be written to the filesystem
  static String fs_path;

  if (!index) {
    /*
    size_t filesize = 0;

    // contentLength() includes the count of bytes in bin file plus the bytes in the header and footer.
    // these extra bytes will cause the writing of a filesystem to fail
    // so use a separate form input on the upload page to send the actual filesize alongside the bin file
    if (request->hasParam("filesize", true)) {
      filesize = request->getParam("filesize", true)->value().toInt();
    }
    else {
      // this doesn't work for spiffs or littlefs 
      filesize = request->contentLength();
    }
    */

    DEBUG_PRINT("if index: ");
    DEBUG_PRINTLN(param_path);

    String param_path_top_dir = "/";
    param_path_top_dir += param_path.substring(0, param_path.indexOf('/'));

    fs_path = "";
    if (param_path_top_dir != IMG_ROOT) {
      fs_path = IMG_ROOT;
    }

    fs_path += "/";
    fs_path += param_path;

    create_dirs(fs_path.substring(0, fs_path.lastIndexOf("/")+1));

    // previous file should not still be open
    if (fs_file) {
      delay(1); 
      fs_file.close();
    }

    DEBUG_PRINT("fs_path: ");
    DEBUG_PRINTLN(fs_path);
    fs_file = LittleFS.open(fs_path, "w");
  }

  for(size_t i = 0; i < len; i++){
    fs_file.write(data[i]);
  }

  if (final) {
    if (fs_file) {
      delay(1); 
      fs_file.close();
    }

    DEBUG_PRINTF("upload complete: %s, %u B\n", fs_path.c_str(), index+len);
    DEBUG_CONSOLE.flush();

    // if want to load a page after the upload is finished, do so here.
    //request->send(LittleFS, "/remove.html");
  }

}


void create_dirs(String path) {
  int f = path.indexOf('/');
  if (f == -1) {
    return;
  }
  if (f == 0) {
    path = path.substring(1);
  }
  if (!path.endsWith("/")) {
    path += '/';
  }
  //DEBUG_PRINTLN(path);

  f = path.indexOf('/');

  while (f != -1) {
    String dir = "/";
    dir += path.substring(0, f);
    if (!LittleFS.exists(dir)) {
      LittleFS.mkdir(dir);
    }
    //DEBUG_PRINT("create_dirs: ");
    //DEBUG_PRINTLN(dir);
    int nf = path.substring(f+1).indexOf('/');
    if (nf != -1) {
      f += nf + 1;
    }
    else {
      f = -1;
      }
  }
}

String file_list;
String file_list_tmp;
// NOTE: an empty folder will not be added when building a littlefs image.
// Empty folders will not be created when uploaded either.
void list_files(File dir, String parent) {
  String path = parent;
  if (!parent.endsWith("/")) {
    path += "/";
  }
  path += dir.name();

  //DEBUG_PRINT("list_files(): ");
  //DEBUG_PRINTLN(path);

  file_list_tmp += path;
  file_list_tmp += "/\n";
  while (File entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      list_files(entry, path);
      entry.close();
    }
    else {
      file_list_tmp += path;
      file_list_tmp += "/";
      file_list_tmp += entry.name();
      file_list_tmp += "\t";
      file_list_tmp += entry.size();
      file_list_tmp += "\n";
      entry.close();
    }
  }
}

void handle_file_list(void) {
  static uint64_t pm = millis();
  if ((millis() - pm) > 1000) {
    pm = millis();
    file_list_tmp = "";
    // cannot prevent "open(): /littlefs/images does not exist, no permits for creation" message
    // the abscense of IMG_ROOT is not an error.
    // tried using exists() before open to prevent message but exists() calls open()
    File img_root = LittleFS.open(IMG_ROOT);
    //File img_root = LittleFS.open("/");  // use this to see all files for debugging
    if (img_root) {
      list_files(img_root, "/");
      img_root.close();
    }
    file_list = file_list_tmp;
    file_list_tmp = "";
  }
}

String delete_list;
void delete_files(String name, String parent) {
  String path = parent;
  if (!parent.endsWith("/")) {
    path += "/";
  }
  path += name;

  //DEBUG_PRINT("path: ");
  //DEBUG_PRINTLN(path);

  File entry = LittleFS.open(path);
  if (entry) {
    if (entry.isDirectory()) {
      while (File e = entry.openNextFile()) {
        String ename = e.name();
        e.close();
        delay(1);
        delete_files(ename, path);
      }
      entry.close();
      delay(1);
      LittleFS.rmdir(path);
    }
    else {
      entry.close();
      delay(1);
      LittleFS.remove(path);
    }
  }
}

void handle_delete_list(void) {
  if (delete_list != "") {
    //DEBUG_PRINT("delete_list: ");
    //DEBUG_PRINTLN(delete_list);

    int f = delete_list.indexOf('\n');
    while (delete_list != "") {
      String name = delete_list.substring(0, f);
      delete_files(name, "/");
      if (f+1 < delete_list.length()) {
        delete_list = delete_list.substring(f+1);
        f = delete_list.indexOf('\n');
      }
      else {
        delete_list = "";
      }
    }
  }
}

bool attempt_connect() {
  bool attempt;
  preferences.begin("netinfo", false);
  attempt = !preferences.getBool("create_ap", true);
  preferences.end();
  return attempt;
}

String get_mode(void) {
  String mode = "unknown";
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
    mode = "station";
  }
  else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    mode = "AP";
  }
  return mode;
}

String get_ip(void) {
  return IP.toString();
}

String get_mdns_addr(void) {
  String mdns_addr = mdns_host;
  mdns_addr += ".local";
  return mdns_addr;
}

void wifi_AP(void) {
  DEBUG_PRINTLN(F("Entering AP Mode."));
  //WiFi.softAP(SOFT_AP_SSID, "123456789");
  WiFi.softAP(SOFT_AP_SSID, "");
  
  IP = WiFi.softAPIP();

  DEBUG_PRINT(F("AP IP address: "));
  DEBUG_PRINTLN(IP);
}

bool wifi_connect(void) {
  bool success = false;

  //if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET_MASK, DNS1, DNS2)) {
  //  DEBUG_PRINTLN(F("WiFi config failed."));
  //}

  preferences.begin("netinfo", false);
 
  String ssid;
  String password;

  ssid = preferences.getString("ssid", ""); 
  password = preferences.getString("password", "");

  DEBUG_PRINTLN(F("Entering Station Mode."));
  if (WiFi.SSID() != ssid.c_str()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    WiFi.persistent(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
  }

  if (WiFi.waitForConnectResult(WIFI_CONNECT_TIMEOUT) == WL_CONNECTED) {
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINT(F("Connected: "));
    IP = WiFi.localIP();
    DEBUG_PRINTLN(IP);
    success = true;
  }
  else {
    DEBUG_PRINT(F("Failed to connect to WiFi."));
    preferences.putBool("create_ap", true);
    success = false;
  }
  preferences.end();
  return success;
}
  
void mdns_setup() {
  preferences.begin("netinfo", false);
  mdns_host = preferences.getString("mdns_host", "");

  if (mdns_host == "") {
    mdns_host = MDNS_HOSTNAME;
  }

  if(!MDNS.begin(mdns_host.c_str())) {
    DEBUG_PRINTLN(F("Error starting mDNS"));
  }
  preferences.end();
}

String processor(const String& var) {
  if(var == "MY_WIDTH")
    return String(width);
  if(var == "MY_HEIGHT")
    return String(height);
  if(var == "MY_RATIO")
    return String(ratio);
  return String();
}

bool filterOnNotLocal(AsyncWebServerRequest *request) {
  // have to refer to service when requesting hostname from MDNS
  // but this code is not working for me.
  //Serial.println(MDNS.hostname(1));
  //Serial.println(MDNS.hostname(MDNS.queryService("http", "tcp")));
  //return request->host() != get_ip() && request->host() != MDNS.hostname(1); 

  return request->host() != get_ip() && request->host() != mdns_host;
}

void web_server_initiate(void) {

  if (WiFi.getMode() == WIFI_STA) {
    web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      //request->send(LittleFS, "/html/index.html");
      request->send(LittleFS, "/html/index.html", String(), false, processor);

      //request->send_P(200, "text/html", index_html);

      //AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", min_index_html_gz, min_index_html_gz_len);
      //response->addHeader("Content-Encoding", "gzip");
      //request->send(response);
    });

    web_server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });

    web_server.on("/upload.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/html/upload.html");
    });

    web_server.on("/remove.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/html/remove.html");
    });

    web_server.on("/network.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/html/network.html");
    });

    web_server.on("/network.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/html/network.html");
    });

    web_server.on("/overlay.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      //String overlay_url = "/html/overlay.html?display_width=";
      //overlay_url + String(width) + "&display_height=" + String(height);
      request->send(LittleFS, "/html/overlay.html", String(), false, processor);
    });


    //OTA update via web page
    //AsyncCallbackWebHandler& on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest, ArUploadHandlerFunction onUpload);
    web_server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->redirect("/upload.html");
    }, handle_folder_upload);

    web_server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", file_list);
    });

    web_server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0; i < params; i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          //DEBUG_PRINTF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());

          String param_name = p->name();
          // remove leading / and file size from param_name to add only filename to the delete_list
          delete_list += param_name.substring(1, param_name.indexOf('\t'));
          delete_list += "\n";
        }
      }

      // what uses args()? not sure if will need this in the future.
      /*
      //List all parameters (Compatibility)
      int args = request->args();
      for(int i=0;i<args;i++){
        DEBUG_PRINTF("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
      }
      */

      request->redirect("/remove.html");
    });

    web_server.onNotFound([](AsyncWebServerRequest *request) {
      request->redirect("/");
    });


    //using format results in an unusable filesystem
    //web_server.on("/format", HTTP_GET, [](AsyncWebServerRequest *request) {
    //  LittleFS.format();
    //  request->redirect("/");
    //});
  }
  else {
    // want limited access when in AP mode. AP mode is just for WiFi setup.
    web_server.onNotFound([](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/html/network.html");
    });
  }

  web_server.on("/savenetinfo", HTTP_POST, [](AsyncWebServerRequest *request) {
    preferences.begin("netinfo", false);

    //int params = request->params();
    //for(int i=0; i < params; i++){
    //  AsyncWebParameter* p = request->getParam(i);
    //  if(p->isPost()){
    //    DEBUG_PRINTF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    //  }
    //}

    if(request->hasParam("ssid", true)) {
      AsyncWebParameter* p = request->getParam("ssid", true);
      preferences.putString("ssid", p->value().c_str());
    }

    if(request->hasParam("password", true)) {
      AsyncWebParameter* p = request->getParam("password", true);
      preferences.putString("password", p->value().c_str());
    }

    if(request->hasParam("mdns_host", true)) {
      AsyncWebParameter* p = request->getParam("mdns_host", true);
      String mdns = p->value();
      mdns.replace(" ", ""); // autocomplete will add space to end of a word if phone is used to enter mdns hostname. remove it.
      mdns.toLowerCase();
      preferences.putString("mdns_host", mdns.c_str());
    }

    preferences.putBool("create_ap", false);

    preferences.end();

    request->redirect("/restart.html");
  });

  web_server.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    restart_needed = true;
    request->send(LittleFS, "/html/restart.html");
  });


  // create a captive portal that catches every attempt to access data besides what the ESP serves to network.html
  // requests to the ESP are handled normally
  // a captive portal makes it easier for a user to save their WiFi credentials to the ESP because they do not
  // need to know the ESP's IP address.
  dnsServer.start(53, "*", IP);
  web_server.addHandler(new CaptiveRequestHandler()).setFilter(filterOnNotLocal);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  web_server.begin();
}


void fm_setup(uint16_t w, uint16_t h, String r) {
  width = w;
  height = h;
  ratio = r;

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN(F("Failed to mount file system"));
  }

  if (!LittleFS.exists(IMG_ROOT)) {
    LittleFS.mkdir(IMG_ROOT);
  }

  web_server_initiate();

  //ESP.wdtEnable(4000); //[ms], parameter is required but ignored
}


void fm_loop(void) {
  //ESP.wdtFeed();

  dnsServer.processNextRequest();

  if (restart_needed || (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED)) {
    delay(2000);
    ESP.restart();
  }

  handle_file_list();
  handle_delete_list();
}

//void setup(void) {
//  DEBUG_BEGIN(115200);
//  fm_setup();
//}

//void loop(void) {
//  fm_loop();
//}
