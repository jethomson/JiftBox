#include <cstring>

#include <string.h>
#include <vector>

#include "Overlay.h"

using namespace std;


TFT_eSPI *_tft;
TFT_eSprite *sprites[MAX_SPRITES];

bool load_overlays = false;
string overlay_assignments_string;

struct overlay {
  uint8_t si = 0;
  int x = 0;
  int y = 0;
};


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

//TODO: handle "[ 61539][E][vfs_api.cpp:29] open(): No Overlay does not start with /"
void load_sprites(vector<string> overlay_names) {
  if (load_overlays) {
    uint8_t sprite_cnt = 0;

    //File dir = LittleFS.open("/overlays");
    //while (File entry = dir.openNextFile()) {

    for (uint8_t i = 0; i < overlay_names.size(); i++) {
      File entry = LittleFS.open(overlay_names[i].c_str());
      if (!entry.isDirectory() && sprite_cnt < MAX_SPRITES) {

        uint16_t *data;

        uint32_t seekOffset;
        uint16_t row, col;
        uint8_t  r, g, b;
        uint16_t bmp_w, bmp_h;

        if (read16(entry) == 0x4D42) {
          read32(entry);
          read32(entry);
          seekOffset = read32(entry);
          read32(entry);
          bmp_w = read32(entry);
          bmp_h = read32(entry);

          if ((read16(entry) == 1) && (read16(entry) == 24) && (read32(entry) == 0)) {
            entry.seek(seekOffset);

            data = new uint16_t[bmp_h*bmp_w];
            // start from the end because BMP are stored upside down
            uint16_t* tptr = &data[(bmp_h*bmp_w)-1];

            uint16_t padding = (4 - ((bmp_w * 3) & 3)) & 3;
            uint8_t lineBuffer[bmp_w * 3 + padding];

            for (row = 0; row < bmp_h; row++) {
              entry.read(lineBuffer, sizeof(lineBuffer));
              uint8_t* bptr = lineBuffer;
              // Convert 24 to 16 bit colours
              for (uint16_t col = 0; col < bmp_w; col++) {
                b = *bptr++;
                g = *bptr++;
                r = *bptr++;
                *tptr-- = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
              }
            }
          }
          else Serial.println("BMP format not recognized.");
        }

        TFT_eSprite *spr = new TFT_eSprite(_tft); 
        spr->setColorDepth(16);
        spr->createSprite(bmp_w, bmp_h);
        if (!spr->created()) {
          Serial.println("Overlay creation failed!");
        }
        spr->setSwapBytes(true);
        spr->pushImage(0, 0, bmp_w, bmp_h, data);
        
        sprites[sprite_cnt] = spr;
        
        sprite_cnt++;
      }
      entry.close();
    }
    //dir.close();
  }
}

void overlay_setup(TFT_eSPI *tft) {
  _tft = tft;
  
  File f = LittleFS.open("/overlay_assignment.json", "r");
  if (f) {
    overlay_assignments_string = f.readString().c_str();
    if (overlay_assignments_string.length() > 0) {
      load_overlays = true;
    }
    f.close();
  }
  else {
    Serial.println("Failed to open config file");
  }
}


TFT_eSprite* get_sprite(uint8_t si) {
  return sprites[si];
}


size_t simple_JSON_parse(string s, string key, size_t start_pos, vector<string> &output) {
  size_t end_pos = string::npos;

  size_t match_start = 0;
  size_t match_end = 0;
  string delimiter = "\"" + key + "\":\""; // fragile, config file can't have any space around : and everything must be a string

  if ((match_start = s.find(delimiter, start_pos)) != string::npos) {
    if ((match_end = s.find("\"", match_start+delimiter.length())) != string::npos) {
      output.push_back(s.substr(match_start+delimiter.length(), match_end-(match_start+delimiter.length())));
      end_pos = match_end;
    }
  }

  return end_pos;
}


void handle_overlay(std::string imgname, TFT_eSprite* background) {
  static int wind_cnt = 0;
  static int wind = 0;
  static int y_dir = 1;

  static vector<string> overlay_names;
  static vector<string> overlay_cnts;
  static string prev_imgname;
  static vector<overlay> overlays;  
  //static uint16_t num_overlays;

  //string imgname = "bokeh_no_transparency.gif";  
  if(prev_imgname != imgname) {
    prev_imgname = imgname;
    overlay_cnts.clear();
    overlays.clear();

    size_t match_start = 0;
    size_t match_end = 0;
    string overlay_dict;
    if ((match_start = overlay_assignments_string.find(imgname)) != string::npos) {
      if ((match_end = overlay_assignments_string.find("}]", match_start)) != string::npos) {
        overlay_dict = overlay_assignments_string.substr(match_start, match_end-match_start+2); // use +2 to include }]
        //Serial.println(overlay_dict.c_str());

        size_t pos = 0;
        uint8_t i = 0;
        while((pos = simple_JSON_parse(overlay_dict, "file", pos, overlay_names)) != string::npos);
        pos = 0;
        while((pos = simple_JSON_parse(overlay_dict, "count", pos, overlay_cnts)) != string::npos);
        //for (unsigned int i = 0; i < overlay_cnts.size(); i++) {
        //  Serial.println(overlay_names[i].c_str());
        //  Serial.println(overlay_cnts[i].c_str());
        //}
      }
      else {
        //bad format
        return;
      }
    }
    else {
      //Serial.print(imgname.c_str());
      //Serial.println(" not found in json file.");
      return;
    }
    
    load_sprites(overlay_names);
    uint16_t num_overlays = 0;
    for (uint8_t i = 0; i < overlay_cnts.size(); i++) {
      num_overlays += stoi(overlay_cnts[i]);
    }
    if (num_overlays == 0) {
      return;
    }
    overlays.resize(num_overlays);
    
    uint16_t ri[overlays.size()];
    for (uint16_t i = 0; i < overlays.size(); i++) {
      ri[i] = i;
    }
    random_shuffle(&ri[0], &ri[overlays.size()-1]);
    
    // set initial positions of overlays
    uint16_t i = 0;
    for (uint8_t si = 0; si < overlay_cnts.size(); si++) {
      uint8_t cnt = stoi(overlay_cnts[si]);
      //Serial.println(cnt);
      for (uint8_t j = 0; j < cnt; j++) {
        int xlb = j*_tft->width()/cnt;
        int xub = (j+1)*_tft->width()/cnt;
        //int ylb = si*_tft->height()/overlay_cnts[si];
        //int yub = (si+1)*_tft->height()/overlay_cnts[si];

        // giving sprites random indices within the overlays vector will result in a random z height
        // that is to all instances of one type of sprite will not always be below all instances of another type of sprite.
        overlays[ri[i]].si = si;
        overlays[ri[i]].x = random(xlb, xub);
        overlays[ri[i]].y = random(-_tft->height(), -9);

        //overlays[i].x = 100;
        //overlays[i].y = 40;
        
        i++;
      }
    }
  }
  
  uint16_t i = 0;
  for (uint8_t si = 0; si < overlay_cnts.size(); si++) {
    uint8_t cnt = stoi(overlay_cnts[si]);
    //Serial.println(cnt);
    for (uint8_t j = 0; j < cnt; j++) {

      //TFT_TRANSPARENT 0x0120 // This is actually a dark green
      //0x0120 == RGB: 0, 36, 0 == #002400
      TFT_eSprite *sprite = get_sprite(overlays[i].si); 
      sprite->pushToSprite(background, overlays[i].x, overlays[i].y, TFT_TRANSPARENT);


      int jitter = random(-2, 3);
      overlays[i].x = overlays[i].x + jitter + wind;
      overlays[i].y = overlays[i].y + y_dir*OVERLAY_Y_SPEED;
      //overlays[i].x = overlays[i].x - 1;
      //overlays[i].y = overlays[i].y + 1;
      // use 20 as a fudge factor so character is off the screen before we reset its location
      if (overlays[i].x + sprite->width() < -20) {
        overlays[i].x = _tft->width();
      }
      else if (overlays[i].x > _tft->width() + 20) {
        overlays[i].x = -sprite->width();
      }

      if (overlays[i].y > _tft->height() + 20) {
        if (y_dir > 0) {
          overlays[i].y = random(-20-sprite->height(), -sprite->height());
        }
      }
      else if (overlays[i].y + sprite->height() < -20) {
        if (y_dir < 0) {
          overlays[i].y = _tft->height() + random(0, 20);
        }
      }
      i++;
    }
  }
  
  wind_cnt++;
  if (wind_cnt > 50) {
    wind_cnt = 0;
    wind = random(-10, 11);
    //if (random(0, 2)) {
    //  y_dir = -1*y_dir;
    //}
  }
}


