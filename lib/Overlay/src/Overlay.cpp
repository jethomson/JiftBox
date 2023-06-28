#include <cstring>

#include <string.h>
#include <vector>

#include "Overlay.h"

using namespace std;


TFT_eSPI *_tft;
vector<std::unique_ptr<TFT_eSprite>> sprites;

string sprite_settings_JSON;

struct SpriteSettings {
  vector<string> filenames;
  vector<int8_t> num_instances;
  vector<int8_t> lin_directions;
  vector<int8_t> lin_speeds;
  vector<int8_t> spin_directions;
  vector<int8_t> spin_speeds;
  vector<int8_t> jiggles;
  vector<int8_t> wind_cats;
  vector<int8_t> wind_speeds;
  vector<int8_t> edge_effects;
};


bool show_overlays = false;

// an overlay tracks the position of an instance of sprite image data
// there can be multiple overlays in different positions using the same sprite image data
struct Overlay {
  uint8_t si = 0; // this is an index into the sprites vector
  int16_t x = 0;
  int16_t y = 0;
  int8_t x_dir = 0;
  int8_t y_dir = 0;
  int8_t lin_speed = 0;   // random, individual requires saving this at the individual overlay level
  int8_t spin_dir = 0;    // random, individual requires saving this at the individual overlay level
  int8_t spin_speed = 0;  // random, individual requires saving this at the individual overlay level
  int16_t spin_angle = 0;
  uint8_t outbounds_cnt = 0;
};


uint16_t read16(fs::File &f);
uint32_t read32(fs::File &f);
void load_sprites(SpriteSettings &sprite_settings);
void overlay_setup(TFT_eSPI *tft);
TFT_eSprite& get_sprite(uint8_t si);
//size_t simple_JSON_parse(string s, string key, size_t start_pos, vector<string> &output);
//template<class T> size_t simple_JSON_parse(string s, string key, size_t start_pos, vector<T> &output, uint8_t output_type);
void handle_overlay(std::string bgimgname, TFT_eSprite* background);


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

//TODO: TFT_eSPI has a drawBitmap function that should be investigated
void load_sprites(SpriteSettings &sprite_settings) {
  sprites.clear();

  uint8_t sprite_cnt = 0;
  int8_t erase[sprite_settings.filenames.size()];

  for (uint8_t i = 0; i < sprite_settings.filenames.size(); i++) {
    erase[i] = -1;
    if(!LittleFS.exists(sprite_settings.filenames[i].c_str()) || sprite_settings.num_instances[i] < 1) {
      // exists will output an error message like: [ 26496][E][vfs_api.cpp:29] open(): No sprite does not start with /
      erase[i] = i; // invalid filename, not found, or 0 instances so mark for removal from vector
      continue;
    }

    //Serial.println(sprite_settings.filenames[i].c_str());
    File entry = LittleFS.open(sprite_settings.filenames[i].c_str());
    if (!entry.isDirectory() && sprite_cnt < MAX_SPRITE_FILES_OPEN) {
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


        //Serial.print("planes: ");
        //Serial.println(planes);

        //Serial.print("bpp: ");
        //Serial.println(bpp);

        //Serial.print("comp: ");
        //Serial.println(comp);

        // why does this function crash if a sprite isn't loaded

        //if ((planes == 1) && (bpp == 16 || bpp == 24) && (comp == 0)) { // use this to cause a crash for testing
        if ((planes == 1) && (bpp == 16 && comp == 3) || (bpp == 24 && comp == 0)) {
          entry.seek(seekOffset);
          data = new uint16_t[bmp_h*bmp_w];
          
          if (upside_down) {
            tptr = &data[(bmp_h*bmp_w)-1]; // writing to data from finish to end will flip the image
          }
          else {
            tptr = data;
          }


          //Serial.print("upside_down: ");
          //Serial.println(upside_down);


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
          erase[i] = i; // unknown format so mark for removal from vector
          Serial.println("BMP format not recognized.");
        }
      }
      else {
        erase[i] = i; // unsupported file so mark for removal from vector
      }
    }
    entry.close();
  }
  // erase bad entries from vectors. go from back to front so indexes stay valid.
  for (int8_t i = sprite_settings.filenames.size()-1; i >= 0; i--) {
    if (erase[i] != -1) {
      sprite_settings.filenames.erase(sprite_settings.filenames.begin() + i);
      sprite_settings.num_instances.erase(sprite_settings.num_instances.begin() + i);
      sprite_settings.lin_directions.erase(sprite_settings.lin_directions.begin() + i);
      sprite_settings.lin_speeds.erase(sprite_settings.lin_speeds.begin() + i);
      sprite_settings.spin_directions.erase(sprite_settings.spin_directions.begin() + i);
      sprite_settings.spin_speeds.erase(sprite_settings.spin_speeds.begin() + i);
      sprite_settings.jiggles.erase(sprite_settings.jiggles.begin() + i);
      sprite_settings.wind_cats.erase(sprite_settings.wind_cats.begin() + i);
      //sprite_settings.wind_speeds.erase(sprite_settings.wind_speeds.begin() + i); // nothing in this vector yet, so no need to deleter No Overlay entries
      sprite_settings.edge_effects.erase(sprite_settings.edge_effects.begin() + i);
    }
  }
}

void overlay_setup(TFT_eSPI *tft) {
  _tft = tft;
  
  File f = LittleFS.open("/sprite_settings.json", "r");
  if (f) {
    sprite_settings_JSON = f.readString().c_str();
    if (sprite_settings_JSON.length() > 0) {
      show_overlays = true;
    }
    f.close();
  }
  else {
    Serial.println("Failed to open config file");
  }
  //show_overlays = false;
}


TFT_eSprite& get_sprite(uint8_t si) {
  return *sprites[si];
}


/*
// cannot get this to work. maybe figure out later.
template<class T> size_t simple_JSON_parse(string s, string key, size_t start_pos, vector<T> &output, uint8_t output_type) {
  size_t end_pos = string::npos;

  size_t match_start = 0;
  size_t match_end = 0;
  string delimiter = "\"" + key + "\":\""; // fragile, config file can't have any space around : and everything must be a string

  if ((match_start = s.find(delimiter, start_pos)) != string::npos) {
    if ((match_end = s.find("\"", match_start+delimiter.length())) != string::npos) {
      if (output_type == 1) {
        T value = stoi(s.substr( match_start+delimiter.length(), match_end-(match_start+delimiter.length()) ));
        output.push_back(value);
      }
      else {
        T value = s.substr( match_start+delimiter.length(), match_end-(match_start+delimiter.length()) );
        output.push_back(value);
      }
      end_pos = match_end;
    }
  }

  return end_pos;
}
*/

size_t simple_JSON_parse(string s, string key, size_t start_pos, vector<int8_t> &output) {
  size_t end_pos = string::npos;

  size_t match_start = 0;
  size_t match_end = 0;
  string delimiter = "\"" + key + "\":\""; // fragile, config file can't have any space around : and everything must be a string

  if ((match_start = s.find(delimiter, start_pos)) != string::npos) {
    if ((match_end = s.find("\"", match_start+delimiter.length())) != string::npos) {
      output.push_back(stoi(s.substr(match_start+delimiter.length(), match_end-(match_start+delimiter.length()))));
      end_pos = match_end;
    }
  }

  return end_pos;
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

bool draw_sprite(TFT_eSprite &sprite, TFT_eSprite* background, int16_t x0, int16_t y0, int16_t angle) {
  bool outbounds = true;
  if (!(x0 + sprite.width() <= 0 || x0 >= _tft->width() || y0 + sprite.height() <= 0 || y0 >= _tft->height())) {
    outbounds = false;
    //TFT_TRANSPARENT 0x0120 // This is actually a dark green
    //0x0120 == RGB: 0, 36, 0 == #002400
    //sprite.pushToSprite(background, x0, y0, TFT_TRANSPARENT); // set location of upper left corner
    background->setPivot(x0+sprite.width()/2, y0+sprite.height()/2); // set location of center point of sprite
    sprite.pushRotated(background, angle, TFT_TRANSPARENT);
  }

  /*
  int16_t min_x;
  int16_t max_x;
  int16_t min_y;
  int16_t max_y;
  //getRotatedBounds enlarges the bounding box to account for rounding errors:  does min_x = x0-2, min_y = y0-2, max_x = x1+2, max_y = y1+2
  // where (x0, y0) is the top left corner, and (x1, y1) is the bottom right corner
  sprite.getRotatedBounds(background, angle, &min_x, &min_y, &max_x, &max_y);
  Serial.print("min_x: ");
  Serial.println(min_x);
  Serial.print("max_x: ");
  Serial.println(max_x);
  Serial.print("min_y: ");
  Serial.println(min_y);
  Serial.print("max_y: ");
  Serial.println(max_y);
  */
  return outbounds;
}


void handle_overlay(std::string bgimgname, TFT_eSprite* background) {
  if (show_overlays) {
    static int wind_cnt = 0;

    static SpriteSettings sprite_settings;
  
    static string prev_bgimgname;
    static vector<Overlay> overlays;

    if (sprite_settings_JSON.find("\"ALL\":{\"enabled\":\"1\"") != string::npos) {
      bgimgname = "ALL";
    }
    if(prev_bgimgname != bgimgname) {
    // this section only runs when the next image is loaded
      prev_bgimgname = bgimgname;
      sprite_settings.filenames.clear();
      sprite_settings.num_instances.clear();
      sprite_settings.lin_directions.clear();
      sprite_settings.lin_speeds.clear();
      sprite_settings.spin_directions.clear();
      sprite_settings.spin_speeds.clear();
      sprite_settings.jiggles.clear();
      sprite_settings.wind_cats.clear();
      sprite_settings.wind_speeds.clear();
      sprite_settings.edge_effects.clear();
      overlays.clear();
      // the primary key for sprite_settings_JSON is the filename of the background image
      // settings_substring contains the filename of the sprite and the sprite's settings for a particular background image filename
      // easier to look for primary key and enabled property at the same time
      string phrase = "\"" + bgimgname +"\":{\"enabled\":\"1\"";
      size_t match_start = 0;
      size_t match_end = 0;
      size_t pos = 0;
      string settings_substring;
      if ((match_start = sprite_settings_JSON.find(phrase)) != string::npos) { // set match_start here so we can skip any data before bgimgname
        if ((match_start = sprite_settings_JSON.find("\"sprite_settings\":", match_start)) != string::npos) {
          if ((match_end = sprite_settings_JSON.find("}]}", match_start)) != string::npos) {
            settings_substring = sprite_settings_JSON.substr(match_start, match_end-match_start+3); // use +3 to include }]}
            while((pos = simple_JSON_parse(settings_substring, "file", pos, sprite_settings.filenames)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "count", pos, sprite_settings.num_instances)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "linear_direction", pos, sprite_settings.lin_directions)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "linear_speed", pos, sprite_settings.lin_speeds)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "spin_direction", pos, sprite_settings.spin_directions)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "spin_speed", pos, sprite_settings.spin_speeds)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "jiggle", pos, sprite_settings.jiggles)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "wind", pos, sprite_settings.wind_cats)) != string::npos);
            pos = 0;
            while((pos = simple_JSON_parse(settings_substring, "edge_effect", pos, sprite_settings.edge_effects)) != string::npos);
            pos = 0;
          }
          else {
            //bad format
            return;
          }
        }
      }
      else {
        return;
      }

      load_sprites(sprite_settings);

      uint16_t total_num_overlays = 0;
      for (uint8_t si = 0; si < sprites.size(); si++) {
        total_num_overlays += sprite_settings.num_instances[si];
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

      wind_cnt = 0;
      sprite_settings.wind_speeds.reserve(sprites.size());

      // set initial positions of overlays
      // i: spans the entire overlay vector
      //si: spans the number of sprite files loaded. this will always be less than or equal to MAX_SPRITE_FILES_OPEN.
      //    multiple overlays can use the same sprite so max(i) >= max(si)
      // j: spans the number of instances of a particular sprite with position of each instance tracked by the overlays vector
      uint16_t i = 0;
      for (uint8_t si = 0; si < sprites.size(); si++) {
        if (sprite_settings.wind_cats[si] == 1) {
          sprite_settings.wind_speeds[si] = random(-5, 6);
        }
        else if (sprite_settings.wind_cats[si] == 2) {
          sprite_settings.wind_speeds[si] = random(-10, 11);
        }
        else {
          sprite_settings.wind_speeds[si] = 0;
        }
        int8_t lin_direction = -99;
        int8_t lin_speed = -99;
        int8_t spin_direction = -99;
        int8_t spin_speed = -99;

        TFT_eSprite &sprite = get_sprite(si); 
        uint8_t cnt = sprite_settings.num_instances[si];

        int divx = (_tft->width()-sprite.width())/cnt;
        int divy = (_tft->height()-sprite.height())/cnt;
        for (uint8_t j = 0; j < cnt; j++) {
          int xlb = j*divx;
          int xub = (j+1)*divx;
          int ylb = j*divy;
          int yub = (j+1)*divy;
 
          // giving sprites random indices within the overlays vector will result in a random z height
          // so that all instances of one type of sprite will not always be covered by all instances of another type of sprite.
          overlays[ri[i]].si = si;

          if (sprite_settings.lin_directions[si] == -2) {
            lin_direction = random(1, 9);
          }
          else if (sprite_settings.lin_directions[si] == -1) {
            // if lin_direction has not been set yet pick a random one for the group
            // otherwise the lin_direction picked for the first overlay in this group will be reused
            if (lin_direction == -99) {
              lin_direction = random(1, 9);
            }
          }
          else {
            lin_direction = sprite_settings.lin_directions[si];
          }
          switch(lin_direction) {
            case 0: // no lin_direction
              overlays[ri[i]].x_dir = 0;
              overlays[ri[i]].y_dir = 0;
              break;
            case 1: // up
              overlays[ri[i]].x_dir = 0;
              overlays[ri[i]].y_dir = -1;
              break;
            case 2: // up-right
              overlays[ri[i]].x_dir = 1;
              overlays[ri[i]].y_dir = -1;
              ylb = ylb - divy;
              yub = yub - divy;
              break;
            case 3: // right
              overlays[ri[i]].x_dir = 1;
              overlays[ri[i]].y_dir = 0;
              break;
            case 4: // down-right
              overlays[ri[i]].x_dir = 1;
              overlays[ri[i]].y_dir = 1;
              ylb = ylb - divy;
              yub = yub - divy;
              break;
            case 5: // down
              overlays[ri[i]].x_dir = 0;
              overlays[ri[i]].y_dir = 1;
              break;
            case 6: // down-left
              overlays[ri[i]].x_dir = -1;
              overlays[ri[i]].y_dir = 1;
              ylb = ylb + divy;
              yub = yub + divy;
              break;
            case 7: // left
              overlays[ri[i]].x_dir = -1;
              overlays[ri[i]].y_dir = 0;
              break;
            case 8: // up-left
              overlays[ri[i]].x_dir = -1;
              overlays[ri[i]].y_dir = -1;
              ylb = ylb + divy;
              yub = yub + divy;
              break;
            default:
              overlays[ri[i]].x_dir = 0;
              overlays[ri[i]].y_dir = 0;
              break;
          }

          if (sprite_settings.lin_speeds[si] == -2) {
            lin_speed = random(1, 12);
          }
          else if (sprite_settings.lin_speeds[si] == -1) {
            if (lin_speed == -99) {
              lin_speed = random(1, 12);
            }
          }
          else {
            lin_speed = sprite_settings.lin_speeds[si];
          }
          overlays[ri[i]].lin_speed = lin_speed;

          if (sprite_settings.spin_directions[si] == -2) {
            spin_direction = random(0, 3);
          }
          else if (sprite_settings.spin_directions[si] == -1) {
            if (spin_direction == -99) {
              spin_direction = random(0, 3);
            }
          }
          else {
            spin_direction = sprite_settings.spin_directions[si];
          }
          switch(spin_direction) {
            case 0: // no spin_direction
              overlays[ri[i]].spin_dir = 0;
              break;
            case 1: // counter-clockwise
              overlays[ri[i]].spin_dir = -1;
              break;
            case 2: // clockwise
              overlays[ri[i]].spin_dir = 1;
              break;
            default:
              overlays[ri[i]].spin_dir = 0;
              break;
          }

          if (sprite_settings.spin_speeds[si] == -2) {
            spin_speed = random(1, 12);
          }
          else if (sprite_settings.spin_speeds[si] == -1) {
            // if spin_speed has not been set yet pick a random one for the group
            // otherwise the spin_speed picked for the first overlay in this group will be reused
            if (spin_speed == -99) {
              spin_speed = random(1, 12);
            }
          }
          else {
            spin_speed = sprite_settings.spin_speeds[si];
          }
          switch(spin_speed) {
            case 0:
              overlays[ri[i]].spin_speed = 0;
              break;
            case 1:
              overlays[ri[i]].spin_speed = 1;
              break;
            case 2:
              overlays[ri[i]].spin_speed = 5;
              break;
            case 3:
              overlays[ri[i]].spin_speed = 10;
              break;
            case 4:
              overlays[ri[i]].spin_speed = 15;
              break;
            case 5:
              overlays[ri[i]].spin_speed = 20;
              break;
            case 6:
              overlays[ri[i]].spin_speed = 25;
              break;
            case 7:
              overlays[ri[i]].spin_speed = 30;
              break;
            case 8:
              overlays[ri[i]].spin_speed = 35;
              break;
            case 9:
              overlays[ri[i]].spin_speed = 40;
              break;
            case 10:
              overlays[ri[i]].spin_speed = 45;
              break;
            case 11:
              overlays[ri[i]].spin_speed = 50;
              break;
            default:
              overlays[ri[i]].spin_speed = 0;
              break;
          }
          overlays[ri[i]].spin_speed = spin_speed;

          if (overlays[ri[i]].y_dir != 0) {
            overlays[ri[i]].x = random(xlb, xub);
            if (overlays[ri[i]].lin_speed > 0) {
              overlays[ri[i]].y = -(overlays[ri[i]].y_dir - 1)*_tft->height() - random(0, _tft->height());
            }
            else {
              overlays[ri[i]].y = random(0, _tft->height());
            }
            //overlays[ri[i]].x = 170; // wrap corner case debug
            //overlays[ri[i]].y = 0;
          }
          else if (overlays[ri[i]].x_dir != 0) {
            if (overlays[ri[i]].lin_speed > 0) {
              overlays[ri[i]].x = -(overlays[ri[i]].x_dir - 1)*_tft->width() - random(0, _tft->width());
            }
            else {
              overlays[ri[i]].x = random(0, _tft->width());
            }
            overlays[ri[i]].y = random(ylb, yub);
          }
          else {
            overlays[ri[i]].x = random(0, _tft->width()-sprite.width());
            overlays[ri[i]].y = random(0, _tft->height()-sprite.height());
          }
          i++;
        }
      }
    }
    
    // below here runs every time
    for (uint16_t i = 0; i < overlays.size(); i++) {
      uint8_t si = overlays[i].si;
      TFT_eSprite &sprite = get_sprite(si); 

      //TODO: add option to preserve positions across multiple background images.
      int jigglex = sprite_settings.jiggles[si]*random(-2, 3);
      int jiggley = sprite_settings.jiggles[si]*random(-2, 3);

      int dx = 0;
      int dy = 0;
      dx = overlays[i].x_dir*overlays[i].lin_speed;
      dx = dx + jigglex + sprite_settings.wind_speeds[si];
      dy = overlays[i].y_dir*overlays[i].lin_speed;
      dy = dy + jiggley;
      overlays[i].x = overlays[i].x + dx;
      overlays[i].y = overlays[i].y + dy;

      overlays[i].spin_angle += overlays[i].spin_dir*overlays[i].spin_speed;
      if (overlays[i].spin_angle <= -360) {
        overlays[i].spin_angle += 360;
      }
      else if (overlays[i].spin_angle >= 360) {
        overlays[i].spin_angle -= 360;
      }

      bool outbounds = draw_sprite(sprite, background, overlays[i].x, overlays[i].y, overlays[i].spin_angle);
      if (outbounds) {
        overlays[i].outbounds_cnt++;
      }
      else {
        overlays[i].outbounds_cnt = 0;
      }

      // unbound -- x and y are given new values if the overlay stays out of bounds too long, so it is essentially no longer the same overlay when it reappears and therefore it is described as new.
      // randomizing when the effect is triggered allows time for an overlay starting from an initial condition out of bounds to make its way inbounds
      // randomizing is also equivalent to giving the new overlay a random initial position
      if (sprite_settings.edge_effects[si] == 0 && overlays[i].outbounds_cnt >= random(0,11)) {
        overlays[i].outbounds_cnt = 0;
        if (overlays[i].x + sprite.width() <= 0) {
          // adding a random offset to the initial position of the new overlay does not work well, because often it will flit back and forth on the outsides of the borders
          overlays[i].x = _tft->width();
          overlays[i].y = random(0, _tft->height()-sprite.height());
        }
        else if (overlays[i].x >= _tft->width()) {
          overlays[i].x = -sprite.width();
          overlays[i].y = random(0, _tft->height()-sprite.height());
        }

        if (overlays[i].y + sprite.width() <= 0) {
          overlays[i].x = random(0, _tft->width()-sprite.width());
          overlays[i].y = _tft->height();
        }
        else if (overlays[i].y >= _tft->height()) {
          overlays[i].x = random(0, _tft->width()-sprite.width());
          overlays[i].y = -sprite.height();
        }
      }
      // wrap -- all edges are connected together
      else if (sprite_settings.edge_effects[si] == 1) {
        int clone_x = overlays[i].x;
        int clone_y = overlays[i].y;
        bool cloned = false;
        if (overlays[i].x < 0) {
          (void *)draw_sprite(sprite, background, overlays[i].x + _tft->width(), overlays[i].y, overlays[i].spin_angle);
          clone_x = overlays[i].x + _tft->width();
          cloned = true;
        }
        else if (overlays[i].x + sprite.width() > _tft->width()) {
          (void *)draw_sprite(sprite, background, overlays[i].x - _tft->width(), overlays[i].y, overlays[i].spin_angle);
          clone_x = overlays[i].x - _tft->width();
          cloned = true;
        }

        if (overlays[i].y < 0) {
          (void *)draw_sprite(sprite, background, overlays[i].x, overlays[i].y + _tft->height(), overlays[i].spin_angle);
          clone_y = overlays[i].y + _tft->height();
          cloned = true;
        }
        else if (overlays[i].y + sprite.height() > _tft->height()) {
          (void *)draw_sprite(sprite, background, overlays[i].x, overlays[i].y - _tft->height(), overlays[i].spin_angle);
          clone_y = overlays[i].y - _tft->height();
          cloned = true;
        }

        if (cloned) {
          (void *)draw_sprite(sprite, background, clone_x, clone_y, overlays[i].spin_angle);
          if (outbounds) {
            // if original is out of bounds that means the clone is completely visible and can therefore can replace the original with the clone
            overlays[i].x = clone_x;
            overlays[i].y = clone_y;
          }
        }
      }
      // bounce -- no energy lost
      else if (sprite_settings.edge_effects[si] == 2) {
        if (overlays[i].x <= 0) {
          overlays[i].x_dir = 1;
        }
        else if (overlays[i].x + sprite.width() >= _tft->width()) {
          overlays[i].x_dir = -1;
        }

        if (overlays[i].y <= 0) {
          overlays[i].y_dir = 1;
        }
        else if (overlays[i].y + sprite.height() >= _tft->height()) {
          overlays[i].y_dir = -1;
        }
      }
    }

    wind_cnt++;
    if (wind_cnt > 25) {
      wind_cnt = 0;
      for (uint8_t si = 0; si < sprites.size(); si++) {
        if (sprite_settings.wind_cats[si] == 1) {
          sprite_settings.wind_speeds[si] = random(-5, 6);
        }
        else if (sprite_settings.wind_cats[si] == 2) {
          sprite_settings.wind_speeds[si] = random(-10, 11);
        }
      }
    }
  }
}


