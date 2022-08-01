#ifndef _HDL_UTIL
#define _HDL_UTIL
#include "hdl-parse.h"

/**
 * @brief Parses a BMP image to HDL_Bitmap
 * 
 * @param filename 
 * @param bitmap 
 * @return int 
 */
int HDL_BitmapFromBMP (const char *filename, struct HDL_Bitmap *bitmap);
#endif