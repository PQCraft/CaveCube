#ifndef RESOURCE_H
#define RESOURCE_H

#include "common.h"
#define RENDERER_H_STUB
#include <renderer.h>
#undef RENDERER_H_STUB
#include <bmd.h>

typedef file_data resdata_file;

typedef bmd_data resdata_bmd;

typedef struct {
    int width;
    int height;
    int channels;
    unsigned char* data;
} resdata_image;

typedef struct {
    int width;
    int height;
    int channels;
    unsigned int data;
} resdata_texture;

typedef struct {
    uint32_t parts;
    resdata_bmd* part;
    resdata_texture* texture;
} resdata_model;

enum {
    RESOURCE_TEXTFILE,
    RESOURCE_BINFILE,
    RESOURCE_BMD,
    RESOURCE_IMAGE,
    RESOURCE_TEXTURE,
    RESOURCE_SOUND,
};

void* loadResource(int, char*);
void freeResource(void*);
void freeAllResources(void);

#endif
