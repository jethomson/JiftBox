#include <Arduino.h>
#include <AnimatedGIF.h>
#include <JPEGDEC.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <cstring>
#include <vector>

#include "GIFDraw.h"
#include "JPEGDraw.h"

#include "FileManager.h"




//#define DEBUG_CONSOLE Serial
#undef DEBUG_CONSOLE
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

//using namespace std;

//#define DEBUG
#define MAX_IMAGE_WIDTH 240 // Adjust for your images
#define NUM_GIF_PLAYS 2
#define SLEEP_AFTER_NUM_PLAYS_ENABLED false
#define NUM_PLAYS_TIL_SLEEP 20
#define BOOT_MENU_DELAY 5 // seconds
// slideshow images aren't expected to have a delay specified, but gif frames always will if created using ezgif.com
#define SLIDESHOW_DELAY 3000 // milliseconds
#define USE_FM true // not using the file manager will cut the program size in half
//#define NUM_OVERLAYS 50
#define NUM_OVERLAYS 12
#define OVERLAY_Y_SPEED 2

AnimatedGIF gif;
JPEGDEC jpeg;

TFT_eSPI tft = TFT_eSPI();
const uint32_t tpixels = tft.width() * tft.height();

TFT_eSprite spr_bg = TFT_eSprite(&tft); 
std::vector<TFT_eSprite> overlays(4, TFT_eSprite(&tft));
uint16_t overlay_w, overlay_h;
struct overlay_position {
  int x = 0;
  int y = 0;
};
overlay_position opos[NUM_OVERLAYS];


struct node {
  std::string imgname;
  node *prev;
  node *next;
  node(std::string);
};
node::node(std::string _imgname){
    this->imgname = _imgname;
    this->prev = nullptr;
    this->next = nullptr;
}
struct node *head_node = NULL;
struct node *curr_node = NULL;


//left button is GPIO0 and right button is GPIO35
const uint8_t button_back = 0;
const uint8_t button_fwd = 35;
const uint8_t debounce_delay = 50;

volatile bool check_button_back = false;
volatile bool check_button_fwd = false;
void IRAM_ATTR button_back_interrupt() {
  check_button_back = true;
}
void IRAM_ATTR button_fwd_interrupt() {
  check_button_fwd = true;
}

volatile bool times_up = false;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  times_up = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// these are reset when forward or back button is pressed
// making these global and reseting them in check_buttons() makes for much cleaner code
int8_t g_skip = 0;
uint8_t g_num_gif_plays = 0;
uint32_t g_pm_jpg = 0; // previous millis() for jpg images
bool g_delay_active = false;

int get_button(uint8_t button_pin);
void check_skip(void);
//void create_file_list(String dirname);
bool create_file_list(File dir, String parent);
void delete_file_list(void);
void espDelay(uint32_t ms);
void deep_sleep(void);
uint16_t find_GCD(uint16_t a, uint16_t b);
void fm_mode(void);
void startup_choice(void);
void decide_image(void);
void handle_image(std::string imgname);
void handle_gif(std::string imgname);
void handle_jpg(std::string imgname);
void handle_overlay(void);



int get_button(uint8_t button_pin) {
  uint32_t prev_ms = millis();
  int prev_reading = digitalRead(button_pin);

  while (true) {
    if ((millis() - prev_ms) > debounce_delay) {
      int reading = digitalRead(button_pin);
      if (reading == prev_reading) {
        return reading;
      }
      else {
        prev_ms = millis();
        prev_reading = reading;
      }
    }
  }
}

void check_skip() {
  if (check_button_back) {
    check_button_back = false;
    if (get_button(button_back) == LOW) {
      g_skip = -1;
    }
  }
  if (check_button_fwd) {
    check_button_fwd = false;
    if (get_button(button_fwd) == LOW) {
      g_skip = 1;
    }
  }
}


/*
// allows only children folders, no grandchildren
void create_file_list(String dirname) {
  struct node *prev_node = NULL;
  struct node *new_node = NULL;

  String imgname;

  File dir = LittleFS.open(dirname);
  File img = dir.openNextFile();
  if (!img.isDirectory()) {
    imgname = img.name();
    imgname = dirname + "/" + imgname;
    new_node = new node(imgname.c_str());
  }
  if (img) {
    img.close();
  }
  head_node = new_node;

  while (img = dir.openNextFile()) {
    if (!img.isDirectory()) {
      prev_node = new_node;
      imgname = img.name();
      imgname = dirname + "/" + imgname;
      new_node = new node(imgname.c_str());
      new_node->prev = prev_node;
      new_node->next = NULL;
      prev_node->next = new_node;
    }
    if (img) {
      img.close();
    }
  }
  new_node->next = head_node;
  head_node->prev = new_node;
  curr_node = head_node;
  dir.rewindDirectory();
  if (dir) {
    dir.close();
  }
}
*/

bool create_file_list(File dir, String parent) {
  static bool has_playable_file = false;
  String path = parent;
  if (!parent.endsWith("/")) {
    path += "/";
  }
  path += dir.name();

  while (File entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      create_file_list(entry, path);
      entry.close();
    }
    else {
      struct node *prev_node = NULL;
      String imgname = path;
      imgname += "/";
      imgname += entry.name();

      String imgname_lc = imgname;
      imgname_lc.toLowerCase();
      if (imgname_lc.endsWith(".gif") || imgname_lc.endsWith(".jpg") || imgname_lc.endsWith(".jpeg")) {
        has_playable_file = true;

        prev_node = curr_node;
        curr_node = new node(imgname.c_str());
        if (prev_node == NULL) {
          head_node = curr_node;
        }
        curr_node->prev = prev_node;
        curr_node->next = NULL;
        if (prev_node != NULL) {
          prev_node->next = curr_node;
        }
      }

      entry.close();
    }
  }
  return has_playable_file;
}

void delete_file_list() {
  struct node *next = NULL;
  curr_node = head_node;
  if (curr_node != NULL && curr_node->prev != NULL) {
    (curr_node->prev)->next = NULL;
  }
  while (curr_node != NULL) {
    next = curr_node->next;

    delete curr_node;
    curr_node = next;
  }
  head_node = NULL;
}

void espDelay(uint32_t ms) {
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void deep_sleep() {

  tft.fillScreen(TFT_BLACK);
  digitalWrite(TFT_BL, LOW);
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);

  //After using light sleep, you need to disable timer wake, because here use external IO port to wake up
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
  delay(200); // don't think espDelay() can be used here
  esp_deep_sleep_start();
}

uint16_t find_GCD(uint16_t a, uint16_t b) {
  if (b == 0) {
    return a;
  }
  return find_GCD(b, a % b);
}

void fm_mode() {
  if (attempt_connect()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Attempting to", tft.width() / 2, tft.height() / 2 - 32);
    tft.drawString("connect to WiFi.", tft.width() / 2, tft.height() / 2 - 16);
  	if (!wifi_connect()) {
	  	// failure to connect will result in creating AP
      tft.drawString("Connection failed.", tft.width() / 2, tft.height() / 2 + 16);
      tft.drawString("Creating AP.", tft.width() / 2, tft.height() / 2 + 32);
      espDelay(2000);
  		wifi_AP();
	  }
  }
  else {
    wifi_AP();
  }

  mdns_setup();

  tft.begin();

  tft.setRotation(0);
  uint16_t pw = tft.width(); // portrait width
  uint16_t ph = tft.height(); // portrait height
  tft.setRotation(1);
  uint16_t lw = tft.width(); // landscape width
  uint16_t lh = tft.height(); // landscape height
  tft.setRotation(0);

  uint16_t d = find_GCD(lw, lh);
  uint16_t ntr = lw/d;
  uint16_t dtr = lh/d;
  String ratio = String(ntr) + ":" + String(dtr);
  String disp_info1 = String("landscape: ") + String(lw) + "x" + String(lh);
  String disp_info2 = String("aspect ratio: ") + ratio;

  fm_setup(lw, lh, ratio);
  String mode = "Mode: " + get_mode();
  String IP = get_ip();
  DEBUG_PRINTLN(IP);
  String mdns_addr = get_mdns_addr();

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);



  tft.drawString(disp_info1.c_str(), pw/ 2, ph / 2 - 96);
  tft.drawString(disp_info2.c_str(), pw/ 2, ph / 2 - 80);

  tft.drawString("File Manager Mode", pw/ 2, ph / 2 - 48);
  tft.drawString("Restart to exit.", pw / 2, ph / 2 - 32);

  tft.drawString(mode, pw / 2, ph / 2);
  tft.drawString("IP Address:", pw / 2, ph / 2 + 16);
  tft.drawString(IP, pw / 2, ph / 2 + 32);
  tft.drawString("mDNS Address:", pw / 2, ph / 2 + 48);
  tft.drawString(mdns_addr, pw / 2, ph / 2 + 64);
  

  while (true) {
    fm_loop();
  }
}

void startup_choice() {
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);

  tft.drawString("LeftButton:", tft.width() / 2, tft.height() / 2 - 16);
#ifdef USE_FM
  tft.drawString("[File Manager]", tft.width() / 2, tft.height() / 2 );
#else
  tft.drawString("[Start Now]", tft.width() / 2, tft.height() / 2 );
#endif
  tft.drawString("RightButton:", tft.width() / 2, tft.height() / 2 + 16);
  tft.drawString("[Deep Sleep]", tft.width() / 2, tft.height() / 2 + 32 );
  tft.drawString("Starting in:", tft.width() / 2, tft.height() / 2 + 64 );
  String delay = String(BOOT_MENU_DELAY) + " seconds";
  tft.drawString(delay, tft.width() / 2, tft.height() / 2 + 80 );

  uint64_t alarm_value = BOOT_MENU_DELAY * 1000000; // microseconds
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, alarm_value, true);
  timerAlarmEnable(timer);


  while (true) {
    if (check_button_back) {
      check_button_back = false;
      if (get_button(button_back) == LOW) {
#ifdef USE_FM
        fm_mode();
#else
        break;
#endif
      }
    }
    if (check_button_fwd) {
      check_button_fwd = false;
      if (get_button(button_fwd) == LOW) {
        DEBUG_PRINTLN("Sleeping");
        deep_sleep();
      }
    }

    if (times_up){
      timerAlarmDisable(timer);
      break;
    }
  }
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void setoverlays(void) {

  //create_file_list(File dir, String parent)
  //create_file_list(img_root, "/")

  uint8_t overlay_cnt = 0;

  File dir = LittleFS.open("/images/overlays/");

  while (File entry = dir.openNextFile()) {
    if (!entry.isDirectory()) {

      uint16_t *overlay;

      fs::File bmpFS;

      bmpFS = entry;

      uint32_t seekOffset;
      uint16_t row, col;
      uint8_t  r, g, b;

      if (read16(bmpFS) == 0x4D42)
      {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        overlay_w = read32(bmpFS);
        overlay_h = read32(bmpFS);

        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
          bmpFS.seek(seekOffset);

          overlay = new uint16_t[overlay_h*overlay_w];
          // start from the end because BMP are stored upside down
          uint16_t* tptr = &overlay[(overlay_h*overlay_w)-1];

          uint16_t padding = (4 - ((overlay_w * 3) & 3)) & 3;
          uint8_t lineBuffer[overlay_w * 3 + padding];

          for (row = 0; row < overlay_h; row++) {
            bmpFS.read(lineBuffer, sizeof(lineBuffer));
            uint8_t* bptr = lineBuffer;
            // Convert 24 to 16 bit colours
            for (uint16_t col = 0; col < overlay_w; col++)
            {
              b = *bptr++;
              g = *bptr++;
              r = *bptr++;
              *tptr-- = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }
          }
        }
        else Serial.println("BMP format not recognized.");
      }

      overlays[overlay_cnt].setColorDepth(16);
      //spr_overlay.createSprite(overlay_w, overlay_h);
      if (overlays[overlay_cnt].createSprite(overlay_w, overlay_h) == nullptr) {
        Serial.println("overlay creation failed");
      }
      overlays[overlay_cnt].setSwapBytes(true);
      overlays[overlay_cnt].pushImage(0, 0, overlay_w, overlay_h, overlay);

      bmpFS.close();
      overlay_cnt++;
    }
    entry.close();
  }

  dir.close();
}



void decide_image(void) {
  if (g_skip < 0) {
    g_skip = 0;
    g_num_gif_plays = 0;
    curr_node = curr_node->prev;
  }
  else if (g_skip > 0) {
    g_skip = 0;
    g_num_gif_plays = 0;
    curr_node = curr_node->next;
  }
  else if (g_num_gif_plays == NUM_GIF_PLAYS || (millis() - g_pm_jpg) > SLIDESHOW_DELAY) {
    g_num_gif_plays = 0;
    g_pm_jpg = millis();
    curr_node = curr_node->next;
  }
}


void handle_image(std::string imgname) {
  String imgname_lc = imgname.c_str();
  imgname_lc.toLowerCase();
  if (imgname_lc.endsWith(".gif")) {
    handle_gif(imgname);
    g_num_gif_plays++;
  }
  else if (imgname_lc.endsWith(".jpg") || imgname_lc.endsWith(".jpeg")) {
    //tft.fillScreen(TFT_BLACK);
    handle_jpg(imgname);
    //spr_bg.pushSprite(0, 0);
    handle_overlay();
    //g_pm_jpg = millis();
    //g_delay_active = true;
  }
}

void handle_gif(std::string imgname) {
  if (gif.open((char *)imgname.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    tft.startWrite();
    while (gif.playFrame(true, NULL)) {    
      check_skip();  
      if (g_skip != 0) {
        break;
      }
    }
    gif.close();
    tft.endWrite(); // Release TFT chip select for other SPI devices
  }
}

void handle_jpg(std::string imgname) {
  // NOTE: progressive JPEGs are not supported. they require too much RAM.
  if (jpeg.open((const char *)imgname.c_str(), JPEGOpenFile, JPEGCloseFile, JPEGReadFile, JPEGSeekFile, JPEGDraw) == 1) {
    //int iOptions = JPEG_AUTO_ROTATE; // is JPEG_AUTO_ROTATE used? does not appear to do anything
    int iOptions = 0;
    int jw = jpeg.getWidth();
    int jh = jpeg.getHeight();
    int jpixels = jw * jh;

    DEBUG_PRINTLN("");
    DEBUG_PRINT("         opened: ");
    DEBUG_PRINTLN(imgname.c_str());

    jpeg.setPixelType(RGB565_BIG_ENDIAN); // The SPI LCD wants the 16-bit pixels in big-endian order

    DEBUG_PRINTF("Image size: %d x %d, orientation: %d, bpp: %d\n", jpeg.getWidth(), jpeg.getHeight(), jpeg.getOrientation(), jpeg.getBpp());

    if (jpixels > tpixels && jpeg.hasThumb()) {
      if (jpeg.getThumbWidth() != 0 && jpeg.getThumbHeight() != 0) {
        DEBUG_PRINTF("Thumbnail present: %d x %d\n", jpeg.getThumbWidth(), jpeg.getThumbHeight());
        jw = jpeg.getThumbWidth();
        jh = jpeg.getThumbHeight();
        jpixels = jw * jh;
        iOptions = JPEG_EXIF_THUMBNAIL | iOptions;
      }
    }

    //if (jpixels <= tpixels) { // no discarded margins
    if (jpixels <= 2*tpixels) {
      // 2*tpixels allow margins of about 20% on each side to be discarded
      // sqrt(2)*tw*sqrt(2)*th
      // sqrt(2) is about 1.4
      iOptions = 0 | iOptions; // 0 for no scaling down
    }
    //else if (jpixels <= 4*tpixels) { // no discarded margins
    else if (jpixels <= 6*tpixels) {
      // if want all of the image to fit within display after scaling use 4 (i.e. 2*tw*2*th = (2*2)*tpixels = 4*tpixels
      // however using x_offset and y_offset allows an image bigger than the display area to be shown by discarding the margins
      // if top, bottom, left, and right margins of about 11% each are desired then:
      // 1.22 is an increase of 22%
      // (2*2)*(1.22*tw*1.22*th) = (2*2)*(1.22*1.22)*tpixels ~ 6*tpixels
      iOptions = JPEG_SCALE_HALF | iOptions;
      jw = jw/2;
      jh = jh/2;
    }
    //else if (jpixels <= 16*tpixels) { // no discarded margins
    else if (jpixels <= 24*tpixels) {
      // (4*4)*(1.22*tw*1.22*th) = (4*4)*(1.22*1.22)*tpixels ~ 24*tpixels
      iOptions = JPEG_SCALE_QUARTER | iOptions;
      jw = jw/4;
      jh = jh/4;
    }
    else {
      iOptions = JPEG_SCALE_EIGHTH | iOptions;
      jw = jw/8;
      jh = jh/8;
    }

    //tft.setRotation(jw >= jh ? 1 : 0);
    //tft.setRotation(jpeg.getOrientation());
    int ort = jpeg.getOrientation();
    DEBUG_PRINT("rotation used: ");
    if (ort == 0 || ort == 1) {
      tft.setRotation(jw >= jh ? 1 : 0);
      DEBUG_PRINTLN(tft.getRotation());
    }
    else if (ort == 2 || ort == 3) {
      tft.setRotation(jw >= jh ? 3 : 2);
      DEBUG_PRINTLN(tft.getRotation());
    }

    // image width would need to be over 2 billion pixels wide for this to underflow
    int x_offset = (tft.width() - jw)/2;
    //if (x_offset < 0) {
    //  x_offset = 0;
    //}
    int y_offset = (tft.height() - jh)/2;
    //if (y_offset < 0) {
    //  y_offset = 0;
    //}

    jpixels = jw*jh;
    if (jpixels < tpixels) {
      //this prevents the previous image from being shown if this image is smaller than the display.
      //do not want to do this for every image because it causes a flicker when switching images.
      tft.fillScreen(TFT_BLACK);
    }

    //if (jpixels <= 2*tpixels) {
      // if the number of pixels of the scaled image is more than twice the display pixels, then do not bother showing the image.
      jpeg.decode(x_offset, y_offset, iOptions);
      DEBUG_PRINT("         displayed: ");
      DEBUG_PRINTLN(imgname.c_str());
    //}
    jpeg.close();
  }
}

void handle_overlay(void) {
  static int wind_cnt = 0;
  static int wind = 0;
  static int y_dir = 1;
  
  //yield();
  //espDelay(100);

  //TFT_eSprite spr_copy = TFT_eSprite(&tft);
  //spr_copy.setColorDepth(16);
  //spr_copy.createSprite(tft.width(), tft.height());
  //spr_bg.pushToSprite(&spr_copy, 0, 0);
  //spr_bg.pushImage(0, 0, tft.width(), tft.height(), bg);

  //if (draw_overlays) {
  if (true) {
    for (int i = 0; i < NUM_OVERLAYS; i++) {
      //spr_bg.print("*");
      //spr_bg.drawChar('*', local_x, local_y, 2);
      //spr_bg.drawString(":)", local_x+30, local_y+20, 2);
      // drawString will use a font if loaded
      //spr_bg.drawString("*", local_x, local_y);

      //TFT_TRANSPARENT 0x0120 // This is actually a dark green
      //0x0120 == RGB: 0, 36, 0 == #002400
      //spr_overlay.pushToSprite(&spr_bg, opos[i].x, opos[i].y, TFT_TRANSPARENT);
      overlays[i % overlays.size()].pushToSprite(&spr_bg, opos[i].x, opos[i].y, TFT_TRANSPARENT);
    }
  }

  spr_bg.pushSprite(0, 0);
  //spr_overlay.pushSprite(0, 0);

  //spr_copy.deleteSprite();

  wind_cnt++;
  if (wind_cnt > 50) {
    wind_cnt = 0;
    wind = random(-10, 11);
    if (random(0, 2)) {
      y_dir = -1*y_dir;
    }
  }

  for (int i = 0; i < NUM_OVERLAYS; i++) {
    int jitter = random(-2, 3);
    opos[i].x = opos[i].x + jitter + wind;
    opos[i].y = opos[i].y + y_dir*OVERLAY_Y_SPEED;
    //opos[i].x = opos[i].x - 1;
    //opos[i].y = opos[i].y + 1;
    // use 20 as a fudge factor so character is off the screen before we reset its location
    if (opos[i].x + overlay_w < -20) {
      opos[i].x = tft.width();
    }
    else if (opos[i].x > tft.width() + 20) {
      opos[i].x = -overlay_w;
    }

    if (opos[i].y > tft.height() + 20) {
      if (y_dir > 0) {
        opos[i].y = random(-20-overlay_h, -overlay_h);
      }
    }
    else if (opos[i].y + overlay_h < -20) {
      if (y_dir < 0) {
        opos[i].y = tft.height() + random(0, 20);
      }
    }
  }
}


void setup() {
#ifdef DEBUG_CONSOLE
  DEBUG_BEGIN(115200);
  while (!DEBUG_CONSOLE);
#else
  Serial.begin(115200);
  while (!Serial);
#endif

  //Not reading the battery
  // ??? possible power saving trick I don't understand
  //digitalWrite(POWER_EN_PIN, LOW);

  // schematic doesn't show pullup resistor for GPIO0, S1.
  // other programs for same board don't require internal pullup
  // but my particular board does. odd.
  pinMode(button_back, INPUT_PULLUP); 
  pinMode(button_fwd, INPUT);
  attachInterrupt(button_back, button_back_interrupt, FALLING);
  attachInterrupt(button_fwd, button_fwd_interrupt, FALLING);

  // Initialise FS
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }

  tft.begin();
#ifdef USE_DMA
  tft.initDMA();
#endif

  startup_choice();

  LittleFS.mkdir(IMG_ROOT);

  File img_root = LittleFS.open(IMG_ROOT);
  if (img_root) {
    if(create_file_list(img_root, "/")) {
      img_root.close();
      curr_node->next = head_node;
      head_node->prev = curr_node;
      curr_node = head_node;
    }
    else {
      img_root.close();
      tft.setRotation(0);
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_GREEN);
      tft.setCursor(0, 0);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(1);

      tft.drawString("No files found.", tft.width() / 2, tft.height() / 2 );
      while(true) {
        yield();
      }

    }
  }
  
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);


  spr_bg.setColorDepth(16);
  spr_bg.createSprite(tft.width(), tft.height());
  if (spr_bg.createSprite(tft.width(), tft.height()) == nullptr) {
    DEBUG_PRINTLN("Setting background failed!");
    while(1) yield();
  }
  
  
  setoverlays();

  for (int i = 0; i < NUM_OVERLAYS; i++) {
    int xlb = i*tft.width()/NUM_OVERLAYS;
    int xub = (i+1)*tft.width()/NUM_OVERLAYS;
    //int ylb = i*tft.height()/NUM_OVERLAYS;
    //int yub = (i+1)*tft.height()/NUM_OVERLAYS;
    
    opos[i].x = random(xlb, xub);
    opos[i].y = random(-tft.height(), -9);

    //opos[i].x = 100;
    //opos[i].y = 40;

  }
  //tft.fillScreen(TFT_TRANSPARENT);

  GIFDraw_setup(&tft);
  JPEGDraw_setup(&spr_bg);

  gif.begin(BIG_ENDIAN_PIXELS);
  
}

void loop() {
  DEBUG_PRINTLN(esp_get_free_heap_size());

  static uint16_t num_played = 0;
  std::string imgname;

  /*
  check_skip();
  bool play = decide_image();
  if (play) {
    imgname = curr_node->imgname;
    handle_image(imgname.c_str());
    num_played++;
  }
  */
  check_skip();
  decide_image();
  imgname = curr_node->imgname;
  Serial.println(imgname.c_str());
  handle_image(imgname.c_str());
  num_played++;

  // TODO: num_plays is broken after adding overlays. counts up too fast. need to fix.
  if (SLEEP_AFTER_NUM_PLAYS_ENABLED && num_played == NUM_PLAYS_TIL_SLEEP) {
    deep_sleep();
  }
}
