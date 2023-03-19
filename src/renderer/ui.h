#if MODULEID == MODULEID_GAME

#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include "renderer.h"
#include <input/input.h>

#include <stdbool.h>
#include <limits.h>

#define isUIIdValid(elemdata, x) ((x) >= 0 && (x) < elemdata->count)
#define isUIElemValid(elemdata, x) (isUIIdValid(elemdata, (x)) && elemdata->data[(x)].valid)

struct ui_elem_property {
    char* name;
    char* value;
};

struct ui_elem_calcprop {
    bool hidden;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t marginl;
    int16_t margint;
    int16_t marginr;
    int16_t marginb;
    int8_t alignx;
    int8_t aligny;
    int8_t z;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct ui_data;
struct ui_elem;

typedef void (ui_event_callback_t)(struct ui_data*, int, struct ui_elem*, int);

struct ui_elem {
    bool valid;
    bool changed;
    int type;
    char* name;
    int prev;
    int parent;
    ui_event_callback_t* callback;
    int state;
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
    UI_ELEM_HOTBAR,
    UI_ELEM_ITEMGRID,
    UI_ELEM_BUTTON,
    UI_ELEM_PROGRESSBAR,
};

enum {
    UI_ATTR_DONE,
    UI_ATTR_NAME,
    UI_ATTR_PARENT,
    UI_ATTR_PREV,
    UI_ATTR_CALLBACK,
    UI_ATTR_STATE,
};

enum {
    UI_STATE_NORMAL,
    UI_STATE_HOVERED,
    UI_STATE_CLICKED,
};

enum {
    UI_EVENT_CLICK,
};

#define UI_ID_DETATCH INT_MIN

struct ui_data* allocUI(void);
int newUIElem(struct ui_data*, int /*type*/, ... /*prop_and_attr*/);
void editUIElem(struct ui_data*, int /*id*/, ... /*prop_and_attr*/);
void deleteUIElem(struct ui_data*, int /*id*/);
void clearUIElems(struct ui_data*);
struct ui_elem* getUIElemData(struct ui_data*, int /*id*/);
int getUIElemByName(struct ui_data*, char* /*name*/, bool /*reverse*/);
int* getUIElemsByName(struct ui_data*, char* /*name*/, int* /*count*/);
bool doUIEvents(struct input_info* inf, struct ui_data* elemdata);
char* getUIElemProperty(struct ui_elem*, char* /*name*/);
bool calcUIProperties(struct ui_data*);
void freeUI(struct ui_data*);

#endif

#endif
