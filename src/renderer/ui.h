#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include <stdbool.h>

struct ui_elem_property {
    char* name;
    char* value;
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
};

int newElem(int /*type*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void editElem(int /*id*/, char* /*name*/, int /*parent*/, ... /*properties*/);
void deleteElem(int /*id*/);
struct ui_elem* getElemData(int /*id*/);
int getElemByName(char* /*name*/, bool /*reverse*/);
int* getElemsByName(char* /*name*/, int* /*count*/);

#endif
