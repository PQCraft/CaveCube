#ifndef RESOURCE_H
#define RESOURCE_H

#include "common.h"

typedef filedata resdata_file;

typedef struct {
    uint32_t isize;
    uint32_t* indices;
    uint32_t vsize;
    float* vertices;
} resdata_bmd;

typedef struct {
    int width;
    int height;
    int channels;
    unsigned char* data;
} resdata_image;

enum {
    RESOURCE_TEXTFILE,
    RESOURCE_BINFILE,
    RESOURCE_BMD,
    RESOURCE_IMAGE,
    RESOURCE_SOUND,
};

void* loadResource(int, char*);

#endif
