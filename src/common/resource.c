#include "common.h"
#include "resource.h"
#include "stb_image.h"
#include <bmd.h>
#include <renderer.h>

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
            *(file_data*)data = getTextFile(path);
            break;
        case RESOURCE_BINFILE:;
            data = malloc(sizeof(resdata_file));
            *(file_data*)data = getBinFile(path);
            break;
        case RESOURCE_BMD:;
            resdata_bmd* bmddata = data = calloc(1, sizeof(resdata_bmd));
            temploadBMD(getBinFile(path), bmddata);
            break;
        case RESOURCE_IMAGE:;
            resdata_image* imagedata = data = calloc(1, sizeof(resdata_image));
            imagedata->data = stbi_load(path, &imagedata->width, &imagedata->height, &imagedata->channels, 0);
            break;
        case RESOURCE_TEXTURE:;
            resdata_texture* texturedata = data = calloc(1, sizeof(resdata_texture));
            unsigned char* idata = stbi_load(path, &texturedata->width, &texturedata->height, &texturedata->channels, 0);
            createTexture(idata, texturedata);
            free(idata);
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
    resentry* ent = NULL;
    for (int i = 0; i < reslist.entries; ++i) {
        if (!reslist.entry[i].data) {
            ent = &reslist.entry[i];
        }
    }
    if (!ent) {
        ++reslist.entries;
        reslist.entry = realloc(reslist.entry, reslist.entries * sizeof(resentry));
        ent = &reslist.entry[reslist.entries - 1];
    }
    *ent = (resentry){type, strdup(path), 1, data};
    return data;
}

void freeResStub(resentry* ent) {
    switch (ent->type) {
        case RESOURCE_TEXTFILE:;
        case RESOURCE_BINFILE:;
            freeFile(*(file_data*)ent->data);
            break;
        case RESOURCE_BMD:;
            freeBMD((bmd_data*)ent->data);
            break;
        case RESOURCE_IMAGE:;
            free(((resdata_image*)ent->data)->data);
            break;
        case RESOURCE_TEXTURE:;
            destroyTexture((resdata_texture*)ent->data);
            break;
    }
    free(ent->data);
    ent->data = NULL;
    free(ent->path);
}

void freeResource(void* data) {
    resentry* ent = NULL;
    for (int i = 0; i < reslist.entries; ++i) {
        if (data == reslist.entry[i].data) {
            ent = &reslist.entry[i];
        }
    }
    if (!ent) return;
    if (!(--ent->uses)) freeResStub(ent);
}

void freeAllResources() {
    for (int i = 0; i < reslist.entries; ++i) {
        if (reslist.entry[i].data) {
            freeResStub(&reslist.entry[i]);
        }
    }
}
