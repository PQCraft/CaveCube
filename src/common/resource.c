#include <main/main.h>
#include "resource.h"
#include "common.h"
#include "stb_image.h"

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

void setResourcePacks(int ct, char** dirs) {
    for (int i = 1; i < packct; ++i) {
        free(packs[i].dir);
        free(packs[i].name);
        free(packs[i].id);
    }
    packct = 1 + ct;
    packs = realloc(packs, sizeof(struct pack) * packct);
    char* npath = malloc(MAX_PATH + 1);
    file_data fdata;
    for (int i = 1; i < packct; ++i) {
        packs[i].dir = strdup(dirs[i - 1]);
        sprintf(npath, "resources/%s/info.inf", packs[i].dir);
        fdata = getTextFile(npath);
        if (fdata.data) {
            packs[i].name = getInfoVarAlloc((char*)fdata.data, "name", packs[i].dir, 256);
            packs[i].id = getInfoVarAlloc((char*)fdata.data, "id", packs[i].dir, 256);
            freeFile(fdata);
        } else {
            packs[i].name = strdup(packs[i].dir);
            packs[i].id = strdup(packs[i].dir);
        }
    }
    #if DBGLVL(1)
    for (int i = 0; i < packct; ++i) {
        printf("Resource pack #%d:\n", i);
        printf("    dir: \"%s\"\n", packs[i].dir);
        printf("    name: \"%s\"\n", packs[i].name);
        printf("    id: \"%s\"\n", packs[i].id);
    }
    #endif
    free(npath);
}

static char* getResourcePath(char* path) {
    char* npath = malloc(MAX_PATH + 1);
    for (int i = packct - 1; i >= 0; --i) {
        if (!packs[i].dir) continue;
        sprintf(npath, "resources/%s/%s", packs[i].dir, path);
        if (isFile(npath) == 1) break;
    }
    return npath;
}

static inline bool mkres_isFile(char* path) {
    if (isFile(path) != 1) {
        fprintf(stderr, "makeResource: No such file: %s\n", path);
        return false;
    }
    return true;
}

static void* makeResource(int type, char* path) {
    void* data = NULL;
    switch (type) {
        case RESOURCE_TEXTFILE:; {
            if (!mkres_isFile(path)) return NULL;
            data = malloc(sizeof(resdata_file));
            *(file_data*)data = getTextFile(path);
        } break;
        case RESOURCE_BINFILE:; {
            if (!mkres_isFile(path)) return NULL;
            data = malloc(sizeof(resdata_file));
            *(file_data*)data = getBinFile(path);
        } break;
        case RESOURCE_IMAGE:; {
            resdata_image* imagedata = data = calloc(1, sizeof(resdata_image));
            imagedata->data = stbi_load(path, &imagedata->width, &imagedata->height, &imagedata->channels, STBI_rgb_alpha);
            if (!imagedata->data) {
                fprintf(stderr, "makeResource: stbi_load: Failed to load %s\n", path);
                char* npath = getResourcePath("game/textures/common/missing.png");
                if (!npath) {
                    free(npath);
                    return NULL;
                }
                data = makeResource(RESOURCE_IMAGE, npath);
                free(npath);
            } else {
                imagedata->channels = 4;
            }
        } break;
    }
    return data;
}

void* loadResource(int type, char* path) {
    for (int i = 0; i < reslist.entries; ++i) {
        resentry* ent = &reslist.entry[i];
        if (ent->type == type && ent->data && !strcmp(ent->path, path)) {
            ++ent->uses;
            return ++ent->data;
        }
    }
    char* npath = getResourcePath(path);
    if (!npath) return NULL;
    void* data = makeResource(type, npath);
    if (!data) {
        free(npath);
        return NULL;
    }
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
    *ent = (resentry){type, npath, 1, data};
    return data;
}

int resourceExists(char* path) {
    char* npath = getResourcePath(path);
    int ret;
    if (!npath) {
        return -1;
    } else {
        ret = isFile(npath);
        free(npath);
    }
    return ret;
}

void initResource() {
    packct = 1;
    packs = malloc(sizeof(struct pack));
    packs[0].dir = strdup("base");
    char* npath = malloc(MAX_PATH + 1);
    strcpy(npath, "resources/base/info.inf");
    file_data data = getTextFile(npath);
    packs[0].name = getInfoVarAlloc((char*)data.data, "name", "base", 256);
    packs[0].id = getInfoVarAlloc((char*)data.data, "id", "base", 256);
    freeFile(data);
    free(npath);
    char* extrapacks[] = {};
    setResourcePacks(sizeof(extrapacks) / sizeof(*extrapacks), extrapacks);
}

static void freeResStub(resentry* ent) {
    switch (ent->type) {
        case RESOURCE_TEXTFILE:;
        case RESOURCE_BINFILE:; {
            freeFile(*(file_data*)ent->data);
        } break;
        case RESOURCE_IMAGE:; {
            stbi_image_free(((resdata_image*)ent->data)->data);
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
