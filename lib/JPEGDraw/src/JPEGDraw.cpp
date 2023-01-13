#include <JPEGDEC.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "JPEGDraw.h"



TFT_eSprite *_sprite; 
static File f;


void JPEGDraw_setup(TFT_eSprite *sprite) {
  _sprite = sprite;
}


// Function to draw pixels to the display
int JPEGDraw(JPEGDRAW *pDraw) {
  if ( pDraw->y >= _sprite->height() ) return 0;

  _sprite->pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);

  return 1;
}


void * JPEGOpenFile(const char *filename, int32_t *size) {
  f = LittleFS.open(filename);
  *size = f.size();
  return &f;
}

void JPEGCloseFile(void *handle) {
  if (f) f.close();
}

int32_t JPEGReadFile(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!f) return 0;
  return f.read(buffer, length);
}

int32_t JPEGSeekFile(JPEGFILE *handle, int32_t position) {
  if (!f) return 0;
  return f.seek(position);
}



