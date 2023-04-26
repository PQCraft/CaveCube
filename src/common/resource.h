#ifndef COMMON_RESOURCE_H
#define COMMON_RESOURCE_H

#include <common/common.h>

typedef file_data resdata_file;

typedef struct {
    int width;
    int height;
    int channels;
    unsigned char* data;
} resdata_image;

enum {
    RESOURCE_TEXTFILE,
    RESOURCE_BINFILE,
    RESOURCE_IMAGE,
    RESOURCE_SOUND,
};

void initResource(void);
void* loadResource(int, char*);
int resourceExists(char*);
void setResourcePacks(int, char**);
void freeResource(void*);
void freeAllResources(void);

#endif
