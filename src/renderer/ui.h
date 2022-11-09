#if MODULEID == MODULEID_GAME

#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include <stdbool.h>

struct ui_elem_property {
    char* name;
    char* value;
};

struct ui_elem_calcprop {
    bool changed;
    bool hidden;
    int x;
    int y;
    int width;
    int height;
    float z;
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
    struct ui_renddata renddata;
};

struct ui_data {
    int count;
    struct ui_elem* data;
};

enum {
    UI_ELEM_CONTAINER,
    UI_ELEM_BOX,
};

struct ui_data* allocUI(void);
int newUIElem(struct ui_data*, int /*type*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void editUIElem(struct ui_data*, int /*id*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void deleteUIElem(struct ui_data*, int /*id*/);
void clearUIElems(struct ui_data*);
struct ui_elem* getUIElemData(struct ui_data*, int /*id*/);
int getUIElemByName(struct ui_data*, char* /*name*/, bool /*reverse*/);
int* getUIElemsByName(struct ui_data*, char* /*name*/, int* /*count*/);
bool calcUIProperties(struct ui_data*);
void freeUI(struct ui_data*);

extern float ui_scale;

#endif

#endif
