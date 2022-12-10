#include <JPEGDEC.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "JPEGDraw.h"


static TFT_eSPI *_tft;
static File f;


void JPEGDraw_setup(TFT_eSPI *tft) {
  _tft = tft;
}


// Function to draw pixels to the display
int JPEGDraw(JPEGDRAW *pDraw) {

  // snippet from TJpeg_Decoder
  // necessary for this library?
  // if display is rotated should x and width be compared?
  if ( pDraw->y >= _tft->height() ) return 0;

  //DEBUG_PRINTF("jpeg draw: x,y=%d,%d, cx,cy = %d,%d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  _tft->pushRect(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
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



