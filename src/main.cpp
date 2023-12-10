#include <Arduino.h>
#include <AnimatedGIF.h>
#include <JPEGDEC.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <cstring>



#include "GIFDraw.h"
#include "JPEGDraw.h"
#include "Overlay.h"

#include "FileManager.h"

#define NUM_GIF_PLAYS 4
#define SLEEP_AFTER_NUM_PLAYS_ENABLED false
#define NUM_PLAYS_TIL_SLEEP 20
#define BOOT_MENU_DELAY 1 // seconds
// slideshow images aren't expected to have a delay specified, but gif frames always will if created using ezgif.com
#define SLIDESHOW_DELAY 9000 // milliseconds
#define USE_FM true // not using the file manager will cut the program size in half

// alternate approach
//#ifdef ARDUINO_BOARD
//# if ARDUINO_BOARD == "TTGO-Display"
//# elif ARDUINO_BOARD == "LilyGo T-Display-S3"
//# endif
//#endif


#ifdef ARDUINO_TTGO_T_DISPLAY
  #define MAX_IMAGE_WIDTH 240 // Adjust for your images
  //when USB connector is pointing down, then left button is GPIO0 and right button is GPIO35
  const uint8_t button_back = 0;
  const uint8_t button_fwd = 35;
#endif
#ifdef ARDUINO_LILYGO_T_DISPLAY_S3
  #include "TouchLib.h"
  #include "Wire.h"

  #define PIN_LCD_BL                   38 // BackLight enable pin (see Dimming.txt)

  #define PIN_LCD_D0                   39
  #define PIN_LCD_D1                   40
  #define PIN_LCD_D2                   41
  #define PIN_LCD_D3                   42
  #define PIN_LCD_D4                   45
  #define PIN_LCD_D5                   46
  #define PIN_LCD_D6                   47
  #define PIN_LCD_D7                   48

  #define PIN_POWER_ON                 15 // LCD and battery Power Enable

  #define PIN_LCD_RES                  5
  #define PIN_LCD_CS                   6
  #define PIN_LCD_DC                   7
  #define PIN_LCD_WR                   8
  #define PIN_LCD_RD                   9

  #define PIN_BUTTON_1                 0
  #define PIN_BUTTON_2                 14
  #define PIN_BAT_VOLT                 4

  #define PIN_IIC_SCL                  17
  #define PIN_IIC_SDA                  18

  #if defined(TOUCH_MODULES_CST_MUTUAL)
    #define PIN_TOUCH_INT                16
    #define PIN_TOUCH_RES                21
    TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS328_SLAVE_ADDRESS, PIN_TOUCH_RES);
  #elif defined(TOUCH_MODULES_CST_SELF)
    #define PIN_TOUCH_INT                16
    #define PIN_TOUCH_RES                21
    TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
  #endif

  #define MAX_IMAGE_WIDTH 320 // Adjust for your images
  //when USB connector is pointing down, then left button is GPIO0 (boot) and right button is GPIO14
  const uint8_t button_back = 0;
  const uint8_t button_fwd = 14;
#endif

#undef DEBUG_CONSOLE
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

using namespace std;


AnimatedGIF gif;
JPEGDEC jpeg;

TFT_eSPI tft = TFT_eSPI();
const uint32_t tpixels = tft.width() * tft.height();

TFT_eSprite screen_buf = TFT_eSprite(&tft);

struct node {
  std::string imgname;
  node *prev;
  node *next;
  node(std::string);
};
node::node(std::string _imgname) {
    this->imgname = _imgname;
    this->prev = nullptr;
    this->next = nullptr;
}
struct node *head_node = NULL;
struct node *curr_node = NULL;


const uint8_t debounce_delay = 50;

volatile bool check_button_back = false;
volatile bool check_button_fwd = false;
#if defined(TOUCH_MODULES_CST_MUTUAL) || defined(TOUCH_MODULES_CST_SELF)
volatile bool check_touch = false;
#endif
void IRAM_ATTR button_back_interrupt() {
  check_button_back = true;
}
void IRAM_ATTR button_fwd_interrupt() {
  check_button_fwd = true;
}

#if defined(TOUCH_MODULES_CST_MUTUAL) || defined(TOUCH_MODULES_CST_SELF)
void IRAM_ATTR touch_interrupt() {
  check_touch = true;
}
#endif


volatile bool times_up = false;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
 
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  times_up = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// this is set when forward or back button is pressed
// making it global and reseting it in handle_image() makes for much cleaner code
int8_t g_skip = 0;

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
uint16_t handle_image(void);
void handle_gif(std::string imgname);
void handle_jpg(std::string imgname);



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

#if defined(TOUCH_MODULES_CST_MUTUAL) || defined(TOUCH_MODULES_CST_SELF)
  if (check_touch) {
    check_touch = false;
    if (touch.read()) {
      TP_Point t = touch.getPoint(0);
      g_skip = (t.y < 170) ? -1 : 1;
    }
  }
#endif
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


uint16_t handle_image(void) {
  static uint16_t num_played = 0;
  static uint8_t num_gif_plays = 0;
  static uint32_t pm_jpg = millis(); // previous millis() for jpg images


  if (g_skip < 0) {
    g_skip = 0;
    num_gif_plays = 0;
    curr_node = curr_node->prev;
  }
  else if (g_skip > 0) {
    g_skip = 0;
    num_gif_plays = 0;
    curr_node = curr_node->next;
  }
  else if (num_gif_plays >= NUM_GIF_PLAYS || (millis() - pm_jpg) > SLIDESHOW_DELAY) {
    num_gif_plays = 0;
    pm_jpg = millis();
    curr_node = curr_node->next;
    num_played++;
  }
  // else do nothing and continue using the current image

  std::string imgname = curr_node->imgname;
  String imgname_lc = imgname.c_str();
  imgname_lc.toLowerCase();
  if (imgname_lc.endsWith(".gif")) {
    handle_gif(imgname);
    num_gif_plays++;
    pm_jpg = millis();
  }
  else if (imgname_lc.endsWith(".jpg") || imgname_lc.endsWith(".jpeg")) {
    handle_jpg(imgname);
    handle_overlay(imgname, &screen_buf);
    screen_buf.pushSprite(0, 0);
  }

  return num_played;
}


void handle_gif(std::string imgname) {
  //long lTime;

  int dt = 0;
  int delayMilliseconds = 0;

  if (gif.open((char *)imgname.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    tft.startWrite();
    //lTime = micros();
    dt = millis();
    while (gif.playFrame(false, &delayMilliseconds)) {
      handle_overlay(imgname, &screen_buf);
      screen_buf.pushSprite(0, 0);
      check_skip();  
      if (g_skip != 0) {
        break;
      }

      // since overlays take time it is better we handle the delay here instead of letting playFrame() do it.
      dt = millis() - dt;
      if (dt < delayMilliseconds)
        delay(delayMilliseconds - dt);
        dt = millis();
    }
    gif.close();
    tft.endWrite(); // Release TFT chip select for other SPI devices

    //lTime = micros() - lTime;
    //DEBUG_PRINT((int)lTime, DEC);
    //DEBUG_PRINTLN(" microseconds");
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

    // image width would need to be over 2 billion pixels wide for this to underflow if unsigned is used
    int x_offset = (tft.width() - jw)/2;
    //if (x_offset < 0) {
    //  x_offset = 0;
    //}
    int y_offset = (tft.height() - jh)/2;
    //if (y_offset < 0) {
    //  y_offset = 0;
    //}


    if (jw < tft.width() || jh < tft.height()) {
      //this prevents the previous image from being shown in the background if the current image is smaller than the display.
      //do not want to do this for every image because it causes a flicker when switching images.
      tft.fillScreen(TFT_BLACK);
    }

    jpixels = jw*jh; // this is now the number of pixels in the scaled image
    //if (jpixels <= 2*tpixels) {
      // show jpeg if the number of pixels of the scaled image is less than twice the display pixels; otherwise do not bother showing the image.
      jpeg.decode(x_offset, y_offset, iOptions);
      DEBUG_PRINT("         displayed: ");
      DEBUG_PRINTLN(imgname.c_str());
    //}
    jpeg.close();
  }
}

void setup() {

// can free a lot of memory by disabling bluetooth?

//Not reading the battery
// ??? possible power saving trick I don't understand
//digitalWrite(POWER_EN_PIN, LOW);

#ifdef ARDUINO_LILYGO_T_DISPLAY_S3
  pinMode(PIN_POWER_ON, OUTPUT); 
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  digitalWrite(PIN_LCD_BL, HIGH);

  digitalWrite(PIN_TOUCH_RES, LOW);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);

  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
#endif

#ifdef DEBUG_CONSOLE
  DEBUG_BEGIN(115200);
  //while (!DEBUG_CONSOLE);
  uint16_t pm = millis();
  while (!DEBUG_CONSOLE && (millis() - pm) <= 1000) {
    delay(250);
  }
#else
  Serial.begin(115200);
  // need to add timeout otherwise ESP32-S3 will get stuck here if not connect to computer.
  uint16_t pm = millis();
  while (!Serial && (millis() - pm) <= 1000) {
    delay(250);
  }
#endif


  // TTGO T-Display schematic doesn't show pullup resistor for GPIO0, S1.
  // other programs for same board don't require internal pullup
  // but my particular board does. odd.
  // Does LilyGo T-Display S3 also require pullup?
  pinMode(button_back, INPUT_PULLUP); 
  pinMode(button_fwd, INPUT);
  attachInterrupt(button_back, button_back_interrupt, FALLING);
  attachInterrupt(button_fwd, button_fwd_interrupt, FALLING);
#if defined(TOUCH_MODULES_CST_MUTUAL) || defined(TOUCH_MODULES_CST_SELF)
  attachInterrupt(PIN_TOUCH_INT, touch_interrupt, CHANGE);
#endif


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

  screen_buf.setColorDepth(16);
  screen_buf.createSprite(tft.width(), tft.height());
  //if (screen_buf.createSprite(tft.width(), tft.height()) == nullptr) {
  if (!screen_buf.created()) {
    DEBUG_PRINTLN("Setting screen_buf failed!");
    while(1) yield();
  }
  
  overlay_setup(&tft);

  GIFDraw_setup(&screen_buf);
  JPEGDraw_setup(&screen_buf);

  //gif.begin(BIG_ENDIAN_PIXELS);
  gif.begin(GIF_PALETTE_RGB565_BE);
}


void loop() {
  DEBUG_PRINTLN(esp_get_free_heap_size());

  static uint16_t num_played = 0;

  check_skip();
  num_played = handle_image();

  if (SLEEP_AFTER_NUM_PLAYS_ENABLED && num_played >= NUM_PLAYS_TIL_SLEEP) {
    deep_sleep();
  }
}