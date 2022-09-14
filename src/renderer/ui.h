#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include <stdbool.h>

struct ui_elem {
    bool valid;
    int type;
    char* name;
    int children;
    int* childdata;
    int properties;
    char** propertydata;
};

int newElem(int, char*, int, ...);
void editElem(int, char* /*name*/, int /*parent*/, ... /*properties*/);
void deleteElem(int);
struct ui_elem* getElemData(int);
int getElemByName(char*, bool /*last*/);
int* getElemsByName(char*, int* /*count*/);

#endif
