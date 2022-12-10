#ifndef __GIFDRAW__
#define __GIFDRAW__

void GIFDraw_setup(TFT_eSPI *tft);
void GIFDraw(GIFDRAW *pDraw);
void * GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);

#endif // __GIFDRAW__