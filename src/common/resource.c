#include <main/main.h>
#include "resource.h"
#include "common.h"
#include "stb_image.h"
#include <bmd/bmd.h>
#include <renderer/renderer.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <common/glue.h>

typedef struct {
    int type;
    char* path;
    long uses;
    void* data;
} resentry;

static struct {
    int entries;
    resentry* entry;
} reslist;

struct pack {
    char* dir;
    char* name;
    char* id;
};

static int packct;
static struct pack* packs;

void initResource() {
    packct = 1;
    packs = malloc(sizeof(struct pack));
    packs[0].dir = strdup("base");
}

bool addResourcePack(char* dir, int pos) {
    (void)dir; (void)pos;
    return true;
}

bool removeResourcePack(char* dir) {
    (void)dir;
    return true;
}

char* getResourcePath(char* path) {
    static char* npath = NULL;
    if (!npath) npath = malloc(MAX_PATH + 1);
    for (int i = packct - 1; i >= 0; --i) {
        if (!packs[i].dir) continue;
        sprintf(npath, "resources/%s/%s", packs[i].dir, path);
        if (isFile(npath) == 1) break;
    }
    return npath;
}

void* makeResource(int type, char* path) {
    if (isFile(path) != 1) {fprintf(stderr, "makeResource: Cannot load %s\n", path); return NULL;}
    void* data = NULL;
    switch (type) {
        case RESOURCE_TEXTFILE:; {
            data = malloc(sizeof(resdata_file));
            *(file_data*)data = getTextFile(path);
        } break;
        case RESOURCE_BINFILE:; {
            data = malloc(sizeof(resdata_file));
            *(file_data*)data = getBinFile(path);
        } break;
        case RESOURCE_BMD:; {
            resdata_bmd* bmddata = data = calloc(1, sizeof(resdata_bmd));
            temploadBMD(getBinFile(path), bmddata);
        } break;
        case RESOURCE_IMAGE:; {
            resdata_image* imagedata = data = calloc(1, sizeof(resdata_image));
            imagedata->data = stbi_load(path, &imagedata->width, &imagedata->height, &imagedata->channels, STBI_rgb_alpha);
            imagedata->channels = 4;
        } break;
        case RESOURCE_TEXTURE:; {
            #if MODULEID == MODULEID_GAME
            resdata_texture* texturedata = data = calloc(1, sizeof(resdata_texture));
            unsigned char* idata = stbi_load(path, &texturedata->width, &texturedata->height, &texturedata->channels, STBI_rgb_alpha);
            texturedata->channels = 4;
            createTexture(idata, texturedata);
            stbi_image_free(idata);
            #endif
        } break;
    }
    return data;
}

void* loadResource(int type, char* path) {
    for (int i = 0; i < reslist.entries; ++i) {
        if (reslist.entry[i].data && !strcmp(reslist.entry[i].path, path)) {
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

int resourceExists(char* path) {
    char* npath = getResourcePath(path);
    if (!npath) return -1;
    return isFile(npath);
}

void freeResStub(resentry* ent) {
    switch (ent->type) {
        case RESOURCE_TEXTFILE:;
        case RESOURCE_BINFILE:; {
            freeFile(*(file_data*)ent->data);
        } break;
        case RESOURCE_BMD:; {
            freeBMD((bmd_data*)ent->data);
        } break;
        case RESOURCE_IMAGE:; {
            stbi_image_free(((resdata_image*)ent->data)->data);
        } break;
        case RESOURCE_TEXTURE:; {
            #if MODULEID == MODULEID_GAME
            destroyTexture((resdata_texture*)ent->data);
            #endif
        } break;
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
    --ent->uses;
    if (ent->uses < 1) freeResStub(ent);
}

void freeAllResources() {
    for (int i = 0; i < reslist.entries; ++i) {
        if (reslist.entry[i].data) {
            freeResStub(&reslist.entry[i]);
        }
    }
}
