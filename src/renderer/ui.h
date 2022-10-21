#ifndef SERVER

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
};

enum {
    UI_ELEM_CONTAINER,
    UI_ELEM_BOX,
};

int newUIElem(int /*type*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void editUIElem(int /*id*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void deleteUIElem(int /*id*/);
struct ui_elem* getUIElemData(int /*id*/);
int getUIElemByName(char* /*name*/, bool /*reverse*/);
int* getUIElemsByName(char* /*name*/, int* /*count*/);
bool calcUIProperties(void);

extern float ui_scale;
extern int ui_elems;
extern struct ui_elem* ui_elemdata;

#endif

#endif
