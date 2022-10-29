#include "hdl-util.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "hdl-cmp.h"

struct __attribute__((packed)) _BMP_ColorEntry {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t reserved;
};

struct __attribute__((packed)) _BMP_Head {
    // File header
    struct __attribute__((packed)) {
        // File signature, must be "BM"
        char signature[2];
        // File size
        uint32_t size;
        // Reserved
        uint8_t reserved0[4];
        // Pixel offset
        uint32_t pixelOffset;
    } fileHeader;

    // Image header
    struct __attribute__((packed)) {
        // Header size
        uint32_t headerSize;
        // Image width
        int32_t imageWidth;
        // Image height
        int32_t imageHeight;
        // Planes
        uint16_t planes;
        // Bits per pixel
        uint16_t bitsPerPixel;
        // Compression
        uint32_t compression;
        // Image size
        uint32_t imageSize;
        // X Pixels per meter
        int32_t xPixelsPerMeter;
        // Y Pixels per meter
        int32_t yPixelsPerMeter;
        // Total colors
        uint32_t totalColors;
        // Important colors
        uint32_t importantColors;
    } imageHeader;
};

// Print bitmap info
void _HDL_PrintBitmapInfo (struct _BMP_Head *header) {

    printf("Bitmap info: \n\tWidth: %i \n\tHeight: %i\n\tBits per pixel: %i\n\tOffset: %i\n", 
        header->imageHeader.imageWidth, 
        header->imageHeader.imageHeight, 
        header->imageHeader.bitsPerPixel,
        header->fileHeader.pixelOffset);
}

int HDL_BitmapFromBMP (const char *filename, struct HDL_Bitmap *bitmap) {
    struct _BMP_Head bmp_header;
    char *ext = NULL;
    // Check extension
    for(int i = strlen(filename) - 1; i > 0; i--) {
        if(filename[i] == '.') {
            ext = (char*)&filename[i];
            break;
        }
    }

    if(ext == NULL || strcmp(ext, ".bmp") != 0) {
        printf("Only .bmp files supported!\n");
        return 1;
    }
    // Buffer to combine path and filename for relative paths
    char buff[256];
    sprintf(buff, "%s%s", input_file_path, filename);
    FILE *file = fopen(buff, "r");

    if(file == NULL) {
        printf("File %s not found!\n", buff);
        return 1;
    }

    size_t file_len = 0;
    fseek(file, 0, SEEK_END);
    file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    if(file_len < sizeof(struct _BMP_Head)) {
        printf("BMP File too short\n");
        fclose(file);
        return 1;
    }

    fread(&bmp_header, 1, sizeof(struct _BMP_Head), file);

    _HDL_PrintBitmapInfo(&bmp_header);

    if(bmp_header.imageHeader.bitsPerPixel != 1) {
        printf("Bits per pixel is not 1. Non monochrome images not supported!\n");
        fclose(file);
        return 1;
    }

    int row_l = (bmp_header.imageHeader.imageWidth + 7) / 8;
    int row_l_pad = (((bmp_header.imageHeader.imageWidth + 31) & ~31) >> 3);

    bitmap->colorMode = HDL_COLORS_MONO;
    bitmap->width = bmp_header.imageHeader.imageWidth;
    bitmap->height = bmp_header.imageHeader.imageHeight;
    bitmap->size = row_l * bitmap->height;

    // Set sprite width, height if not set
    if(bitmap->sprite_width == 0)
        bitmap->sprite_width = bitmap->width;

    if(bitmap->sprite_height == 0) 
        bitmap->sprite_height = bitmap->height;

    bitmap->data = malloc(bitmap->size);
    memset(bitmap->data, 0, bitmap->size);
    
    fseek(file, bmp_header.fileHeader.pixelOffset, SEEK_SET);
    for(int i = bitmap->height - 1; i >= 0; i--) {
        fread(bitmap->data + (row_l * i), 1, row_l, file);

        // Invert color
        /*
        for(int x = 0; x < row_l; x++) {
            (bitmap->data + (row_l * i))[x] = ~(bitmap->data + (row_l * i))[x];
        }
        */
        if(row_l != row_l_pad) {
            // Skip padding
            fseek(file, row_l_pad - row_l, SEEK_CUR);
        }
    }

    fclose(file);

    return 0;
}