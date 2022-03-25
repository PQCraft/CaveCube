#include "common.h"
#include "resource.h"
#include "stb_image.h"
#include <bmd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int type;
    char* path;
    long uses;
    void* data;
} resentry;

struct {
    int entries;
    resentry* entry;
} reslist;

struct {
    int packs;
    char** pack;
} packlist;

char* getResourcePath(char* path) {
    static char* npath = NULL;
    if (!npath) npath = malloc(32768);
    if (!packlist.pack) {
        packlist.packs = 1;
        packlist.pack = malloc(sizeof(char*));
        packlist.pack[0] = strdup("base");
    }
    for (int i = packlist.packs - 1; i > -1; --i) {
        sprintf(npath, "resources/%s/%s", packlist.pack[i], path);
        if (isFile(npath) == 1) break;
    }
    return npath;
}

void* makeResource(int type, char* path) {
    if (isFile(path) != 1) {fprintf(stderr, "makeResource: Cannot load %s", path); return NULL;}
    void* data = NULL;
    switch (type) {
        case RESOURCE_TEXTFILE:;
            data = malloc(sizeof(resdata_file));
            *(filedata*)data = getTextFile(path);
            break;
        case RESOURCE_BINFILE:;
            data = malloc(sizeof(resdata_file));
            *(filedata*)data = getBinFile(path);
            break;
        case RESOURCE_BMD:;
            resdata_bmd* bmddata = data = calloc(1, sizeof(resdata_bmd));
            temploadBMD(getBinFile(path), &bmddata->indices, &bmddata->isize, &bmddata->vertices, &bmddata->vsize);
            break;
        case RESOURCE_IMAGE:;
            stbi_set_flip_vertically_on_load(true);
            resdata_image* imagedata = data = calloc(1, sizeof(resdata_image));
            imagedata->data = stbi_load(path, &imagedata->width, &imagedata->height, &imagedata->channels, 0);
            break;
        case RESOURCE_SOUND:;
            break;
    }
    return data;
}

void* loadResource(int type, char* path) {
    for (int i = 0; i < reslist.entries; ++i) {
        if (!strcmp(reslist.entry[i].path, path)) {
            ++reslist.entry[i].uses;
            return reslist.entry[i].data;
        }
    }
    char* npath = getResourcePath(path);
    if (!npath) return NULL;
    void* data = makeResource(type, npath);
    if (!data) return NULL;
    ++reslist.entries;
    reslist.entry = realloc(reslist.entry, reslist.entries * sizeof(resentry));
    reslist.entry[reslist.entries - 1] = (resentry){type, path, 1, data};
    return data;
}
