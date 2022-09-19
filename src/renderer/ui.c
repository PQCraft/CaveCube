#include "ui.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static int elems = 0;
static struct ui_elem* elemdata;

int newElem(int type, char* name, int parent, ...) {
    (void)type;
    int index = -1;
    for (int i = 0; i < elems; ++i) {
        if (!elemdata[i].valid) {index = i; break;}
    }
    index = elems++;
    elemdata = realloc(elemdata, elems * sizeof(*elemdata));
    memset(&elemdata[index], 0, sizeof(*elemdata));
    elemdata[index].name = name;
    elemdata[index].children = 0;
    elemdata[index].childdata = NULL;
    elemdata[index].properties = 0;
    elemdata[index].propertydata = NULL;
    va_list args;
    va_start(args, parent);
    
    va_end(args);
    elemdata[index].valid = true;
    return -1;
}
