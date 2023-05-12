#include <cstring>

#include <string.h>
#include <vector>

#include "Overlay.h"

using namespace std;


TFT_eSPI *_tft;
vector<std::unique_ptr<TFT_eSprite>> sprites;

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
//TODO: delete sprites before creating new ones
void load_sprites(vector<string> overlay_fnames) {
  sprites.clear();

  if (load_overlays) {
    uint8_t sprite_cnt = 0;

    for (uint8_t i = 0; i < overlay_fnames.size(); i++) {
      Serial.println(overlay_fnames[i].c_str());
      if (overlay_fnames[i] == "No Overlay") {
        continue;
      }
      File entry = LittleFS.open(overlay_fnames[i].c_str());
      if (!entry.isDirectory() && sprite_cnt < MAX_OVERLAY_FILES_OPEN) {
        uint16_t *data;

        uint32_t seekOffset;
        uint16_t row, col;
        uint8_t  b1, b2, b3;
        uint32_t bmp_w;
        int32_t bmp_h;
        uint16_t planes;
        uint16_t bpp;
        uint32_t comp;
        bool upside_down = true;
        uint16_t* tptr;
        /*
			// Header
			set16(0x4d42);                   // BM
			set32(fileLength);               // total length
			seek(4);                         // skip unused fields
			set32(0x7a);                     // offset to pixels
			
			// DIB header
			set32(0x6c);                     // header size (108)
			set32(w);
			set32(-h >>> 0);                 // negative = top-to-bottom
			set16(1);                        // 1 plane
			set16(bpp);                      // 16, 24, or 32-bits
        */

        if (read16(entry) == 0x4D42) {
          read32(entry);
          read32(entry);
          seekOffset = read32(entry);
          read32(entry);
          bmp_w = read32(entry);
          bmp_h = read32(entry);
          planes = read16(entry);
          bpp = read16(entry);
          comp = read32(entry);
          // normally BMP are stored upside down but if h is negative it indicates the BMP is stored upside up
          if (bmp_h < 0) {
            upside_down = false;
            bmp_h = abs(bmp_h);
          }


          Serial.print("planes: ");
          Serial.println(planes);

          Serial.print("bpp: ");
          Serial.println(bpp);

          Serial.print("comp: ");
          Serial.println(comp);

          // why does this function crash if a sprite isn't loaded

          //if ((planes == 1) && (bpp == 16 || bpp == 24) && (comp == 0)) {
          if ((planes == 1) && (bpp == 16 || bpp == 24)) {  // gimp creates 16 bit bmp with comp 3 

            entry.seek(seekOffset);
            data = new uint16_t[bmp_h*bmp_w];
            
            if (upside_down) {
              tptr = &data[(bmp_h*bmp_w)-1]; // writing to data from finish to end will flip the image
            }
            else {
              tptr = data;
            }


            Serial.print("upside_down: ");
            Serial.println(upside_down);


            uint8_t bytespp = bpp/8;
            uint16_t padding = (4 - ((bmp_w * bytespp) & bytespp)) & bytespp;
            uint8_t lineBuffer[bmp_w * bytespp + padding];

            for (row = 0; row < bmp_h; row++) {
              entry.read(lineBuffer, sizeof(lineBuffer));
              uint8_t* bptr;
              if (upside_down) {
                //(bmp_w * bytespp) + padding - 1 is the last byte of the lineBuffer
                //but we do not want to read the padding so use: (bmp_w * bytespp) + padding - 1 - padding = (bmp_w * bytespp)-1
                bptr = &lineBuffer[(bmp_w * bytespp)-1];
              }
              else {
                bptr = lineBuffer;
              }
              if (bpp == 24) {
                // convert 24 bit to 16 bit
                for (uint16_t col = 0; col < bmp_w; col++) {
                  if (upside_down) {
                    b3 = *bptr--;  // red
                    b2 = *bptr--;  // green
                    b1 = *bptr--;  // blue
                    *tptr-- = ((b3 & 0xF8) << 8) | ((b2 & 0xFC) << 3) | (b1 >> 3);
                  }
                  else {
                    b1 = *bptr++;  // blue
                    b2 = *bptr++;  // green
                    b3 = *bptr++;  // red
                    *tptr++ = ((b3 & 0xF8) << 8) | ((b2 & 0xFC) << 3) | (b1 >> 3);
                  }
                }
              }
              else { // 16 bit
                for (uint16_t col = 0; col < bmp_w; col++) {
                  if (upside_down) {
                    b2 = *bptr--;  // byte 2
                    b1 = *bptr--;  // byte 1
                    *tptr-- = (b2 << 8) | b1;
                  }
                  else {
                    b1 = *bptr++;  // byte 1
                    b2 = *bptr++;  // byte 2
                    *tptr++ = (b2 << 8) | b1;
                  }
                }
              }
            }

            sprites.push_back(std::unique_ptr<TFT_eSprite>(new TFT_eSprite(_tft)));
            //sprites.push_back(std::make_unique<TFT_eSprite>(_tft));

            sprites[sprite_cnt]->setColorDepth(16);
            sprites[sprite_cnt]->createSprite(bmp_w, bmp_h);
            if (!sprites[sprite_cnt]->created()) {
              Serial.println("Overlay creation failed!");
            }
            sprites[sprite_cnt]->setSwapBytes(true);
            // pushImage() makes a copy of data, so data can be deleted.
            sprites[sprite_cnt]->pushImage(0, 0, bmp_w, bmp_h, data);
            delete data;
            sprite_cnt++;
          }
          else {
            Serial.println("BMP format not recognized.");
          }
        }
      }
      entry.close();
    }
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


TFT_eSprite& get_sprite(uint8_t si) {
  return *sprites[si];
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

  struct overlay_attributes {
    vector<string> filenames;
    vector<string> num_instances;
  };

  static overlay_attributes attributes;

  static string prev_imgname;
  static vector<overlay> overlays;  

  if(prev_imgname != imgname) {
  // this section only runs when the next image is loaded
    prev_imgname = imgname;
    attributes.filenames.clear();
    attributes.num_instances.clear();
    overlays.clear();

    size_t match_start = 0;
    size_t match_end = 0;
    string attributes_string;
    if ((match_start = overlay_assignments_string.find(imgname)) != string::npos) {
      if ((match_end = overlay_assignments_string.find("}]", match_start)) != string::npos) {
        attributes_string = overlay_assignments_string.substr(match_start, match_end-match_start+2); // use +2 to include }]

        size_t pos = 0;
        uint8_t i = 0;
        while((pos = simple_JSON_parse(attributes_string, "file", pos, attributes.filenames)) != string::npos);
        pos = 0;
        while((pos = simple_JSON_parse(attributes_string, "count", pos, attributes.num_instances)) != string::npos);
      }
      else {
        //bad format
        return;
      }
    }
    else {
      return;
    }

    load_sprites(attributes.filenames);

    // TODO: need to fix crash if overlay is referenced but does not exist

    Serial.println(esp_get_free_heap_size());

    uint16_t total_num_overlays = 0;
    for (uint8_t ni = 0; ni < attributes.filenames.size(); ni++) {
      if (attributes.filenames[ni] == "No Overlay") {
        attributes.num_instances[ni] = "0";
      }
      total_num_overlays += stoi(attributes.num_instances[ni]);
    }
    if (total_num_overlays == 0) {
      return;
    }
    overlays.resize(total_num_overlays);
    
    uint16_t ri[overlays.size()];
    for (uint16_t i = 0; i < overlays.size(); i++) {
      ri[i] = i;
    }
    random_shuffle(&ri[0], &ri[overlays.size()-1]);
    
    // set initial positions of overlays
    // i: spans the entire overlay vector
    //ni: spans the number of overlay image filenames. this will always be less than or equal to MAX_OVERLAY_FILES_OPEN.
    //    since sprites is created using attributes.filenames their lengths will be the same and si will index the sprite containing the image whose filename is indexed by ni when si equals ni
    // j: spans the number of overlay instances for a particular overlay image
    uint16_t i = 0;
    for (uint8_t ni = 0; ni < attributes.filenames.size(); ni++) {
      uint8_t cnt = stoi(attributes.num_instances[ni]);
      for (uint8_t j = 0; j < cnt; j++) {
        int xlb = j*_tft->width()/cnt;
        int xub = (j+1)*_tft->width()/cnt;
        //int ylb = si*_tft->height()/overlay_cnts[si];
        //int yub = (si+1)*_tft->height()/overlay_cnts[si];

        // giving sprites random indices within the overlays vector will result in a random z height
        // so that all instances of one type of sprite will not always be below all instances of another type of sprite.
        overlays[ri[i]].si = ni;
        overlays[ri[i]].x = random(xlb, xub);
        overlays[ri[i]].y = random(-_tft->height(), -9);

        //overlays[i].x = 100;
        //overlays[i].y = 40;
        
        i++;
      }
    }
  }
  
  // below here runs every time

  // i: spans the entire overlay vector
  //ni: spans the number of overlay image filenames. this will always be less than or equal to MAX_OVERLAY_FILES_OPEN.
  //    since sprites is created using attributes.filenames their lengths will be the same and si will index the sprite containing the image whose filename is indexed by ni when si equals ni
  // j: spans the number of overlay instances for a particular overlay image
  uint16_t i = 0;
  for (uint8_t ni = 0; ni < attributes.filenames.size(); ni++) {
    for (uint8_t j = 0; j < stoi(attributes.num_instances[ni]); j++) {
      //TFT_TRANSPARENT 0x0120 // This is actually a dark green
      //0x0120 == RGB: 0, 36, 0 == #002400
      TFT_eSprite &sprite = get_sprite(overlays[i].si); 
      sprite.pushToSprite(background, overlays[i].x, overlays[i].y, TFT_TRANSPARENT);


      int jitter = random(-2, 3);
      overlays[i].x = overlays[i].x + jitter + wind;
      overlays[i].y = overlays[i].y + y_dir*OVERLAY_Y_SPEED;
      //overlays[i].x = overlays[i].x - 1;
      //overlays[i].y = overlays[i].y + 1;
      // use 20 as a fudge factor so character is off the screen before we reset its location
      if (overlays[i].x + sprite.width() < -20) {
        overlays[i].x = _tft->width();
      }
      else if (overlays[i].x > _tft->width() + 20) {
        overlays[i].x = -sprite.width();
      }

      if (overlays[i].y > _tft->height() + 20) {
        if (y_dir > 0) {
          overlays[i].y = random(-20-sprite.height(), -sprite.height());
        }
      }
      else if (overlays[i].y + sprite.height() < -20) {
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


