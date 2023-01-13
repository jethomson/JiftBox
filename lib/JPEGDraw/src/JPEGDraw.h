#ifndef __JPEGDRAW__
#define __JPEGDRAW__

//void JPEGDraw_setup(TFT_eSPI *tft);
void JPEGDraw_setup(TFT_eSprite *sprite);
int JPEGDraw(JPEGDRAW *pDraw);
void * JPEGOpenFile(const char *filename, int32_t *size);
void JPEGCloseFile(void *handle);
int32_t JPEGReadFile(JPEGFILE *handle, uint8_t *buffer, int32_t length);
int32_t JPEGSeekFile(JPEGFILE *handle, int32_t position);

#endif // __JPEGDRAW__
