#ifndef __OVERLAY__
#define __OVERLAY__

#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define MAX_OVERLAY_FILES 4

#define OVERLAY_Y_SPEED 2

void overlay_setup(TFT_eSPI *tft);
void handle_overlay(std::string imgname, TFT_eSprite* background);


#endif // __OVERLAY__
