#if MODULEID == MODULEID_GAME

#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include "renderer.h"

#include <stdbool.h>

#define isUIIdValid(elemdata, x) (x >= 0 && x < elemdata->count)
#define isUIElemValid(elemdata, x) (isUIIdValid(elemdata, x) && elemdata->data[x].valid)

struct ui_elem_property {
    char* name;
    char* value;
};

struct ui_elem_calcprop {
    bool changed;
    bool hidden;
    int16_t x;
    int16_t y;
    int width;
    int height;
    int8_t z;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct ui_elem {
    bool valid;
    int type;
    char* name;
    int parent;
    int children;
    int* childdata;
    int properties;
    struct ui_elem_property* propertydata;
    struct ui_elem_calcprop calcprop;
};

struct ui_data {
    int count;
    struct ui_elem* data;
    struct ui_renddata renddata;
    bool hidden;
    bool del;
    int scale;
    int scrw;
    int scrh;
};

enum {
    UI_ELEM_CONTAINER,
    UI_ELEM_BOX,
    UI_ELEM_FANCYBOX,
};

struct ui_data* allocUI(void);
int newUIElem(struct ui_data*, int /*type*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void editUIElem(struct ui_data*, int /*id*/, char* /*name*/, ... /*properties*/);
void deleteUIElem(struct ui_data*, int /*id*/);
void clearUIElems(struct ui_data*);
struct ui_elem* getUIElemData(struct ui_data*, int /*id*/);
int getUIElemByName(struct ui_data*, char* /*name*/, bool /*reverse*/);
int* getUIElemsByName(struct ui_data*, char* /*name*/, int* /*count*/);
char* getUIElemProperty(struct ui_elem*, char* /*name*/);
bool calcUIProperties(struct ui_data*);
void freeUI(struct ui_data*);

#endif

#endif
