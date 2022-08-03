#ifndef COMMON_RESOURCE_H
#define COMMON_RESOURCE_H

#include <common/common.h>
#define RENDERER_H_STUB
#include <renderer/renderer.h>
#undef RENDERER_H_STUB
#include <bmd/bmd.h>

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

void initResource(void);
void* loadResource(int, char*);
int resourceExists(char*);
void freeResource(void*);
void freeAllResources(void);
bool addResourcePack(char*, int);
bool removeResourcePack(char*);

#endif
