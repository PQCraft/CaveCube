#include "ui.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

float ui_scale = 2.0;

static int elems = 0;
static struct ui_elem* elemdata;

#define idValid(x) (x >= 0 && x < elem)
#define elemValid(x) (idValid(x) && elemdata[x].valid)

int newElem(int type, char* name, int parent, ...) {
    if (!elemValid(parent)) return -1;
    int index = -1;
    for (int i = 0; i < elems; ++i) {
        if (!elemdata[i].valid) {index = i; break;}
    }
    index = elems++;
    elemdata = realloc(elemdata, elems * sizeof(*elemdata));
    struct ui_elem* e = &elemdata[index];
    memset(e, 0, sizeof(*e));
    e->type = type;
    if (name && *name) e->name = strdup(name);
    e->parent = parent;
    e->children = 0;
    e->childdata = NULL;
    e->properties = 0;
    e->propertydata = NULL;
    va_list args;
    va_start(args, parent);
    for (int i = 0; ; ++i) {
        char* name = va_arg(args, char*);
        if (!name) break;
        char* val = va_arg(args, char*);
        if (!val) continue;
        e->propertydata = realloc(e->propertydata, i * sizeof(*e->propertydata));
        e->propertydata[i].name = strdup(name);
        e->propertydata[i].value = strdup(val);
    }
    va_end(args);
    e->valid = true;
    if (parent >= 0) {
        struct ui_elem* p = &elemdata[parent];
        int cindex = -1;
        for (int i = 0; i < p->children; ++i) {
            if (p->childdata[i] < 0) {cindex = i; break;}
        }
        if (cindex < 0) {
            cindex = p->children++;
            p->childdata = realloc(p->childdata, p->children * sizeof(*p->childdata));
        }
        p->childdata[cindex] = index;
    }
    return index;
}

void editElem(int id, char* name, int parent, ...) {
    if (!elemValid(id)) return;
    struct ui_elem* e = &elemdata[id];
    if (name) {
        if (*name) {free(e->name); e->name = strdup(name);}
        else {free(e->name); e->name = NULL;}
    }
    if (elemValid(parent)) e->parent = parent;
    va_list args;
    va_start(args, parent);
    while (1) {
        char* name = va_arg(args, char*);
        if (!name) break;
        char* val = va_arg(args, char*);
        if (!val) {
            for (int i = 0; i < e->properties; ++i) {
                if (e->propertydata[i].name && !strcasecmp(e->propertydata[i].name, name)) {
                    free(e->propertydata[i].name);
                    e->propertydata[i].name = NULL;
                    free(e->propertydata[i].value);
                    e->propertydata[i].value = NULL;
                    break;
                }
            }
        } else {
            int index = -1;
            for (int i = 0; i < e->properties; ++i) {
                if (e->propertydata[i].name && !strcasecmp(e->propertydata[i].name, name)) {
                    free(e->propertydata[i].value);
                    index = i;
                    break;
                }
            }
            if (index < 0) {
                for (int i = 0; i < e->properties; ++i) {
                    if (!e->propertydata[i].name) {
                        e->propertydata[i].name = strdup(name);
                        index = i;
                        break;
                    }
                }
                if (index < 0) {
                    index = e->properties++;
                    e->propertydata = realloc(e->propertydata, e->properties * sizeof(*e->propertydata));
                    e->propertydata[index].name = strdup(name);
                }
            }
            e->propertydata[index].value = strdup(val);
        }
    }
    va_end(args);
}

void deleteElem(int id) {
    if (!elemValid(id)) return;
    struct ui_elem* e = &elemdata[id];
    e->valid = false;
    if (elemValid(e->parent)) {
        struct ui_elem* p = &elemdata[e->parent];
        for (int i = 0; i < p->children; ++i) {
            if (p->childdata[i] == id) {
                p->childdata[i] = -1;
                break;
            }
        }
    }
    for (int i = 0; i < e->children; ++i) {
        if (e->childdata[i] >= 0) deleteElem(e->childdata[i]);
    }
    free(e->name);
    free(e->childdata);
    for (int i = 0; i < e->properties; ++i) {
        free(e->propertydata[i].name);
        free(e->propertydata[i].value);
    }
    free(e->propertydata);
}

struct ui_elem* getElemData(int id) {
    return (elemValid(id)) ? &elemdata[id] : NULL;
}

int getElemByName(char* name, bool reverse) {
    int i;
    if (reverse) {i = elems - 1;} else {i = 0;}
    while (1) {
        if (reverse) {if (i < 0) break;} else {if (i >= elems) break;}
        if (elemdata[i].valid && !strcasecmp(elemdata[i].name, name)) {
            return i;
        }
        if (reverse) {--i;} else {++i;}
    }
    return -1;
}

int* getElemsByName(char* name, int* _count) {
    int count = 0;
    int* output = malloc(1 * sizeof(*output));
    output[0] = -1;
    for (int i = 0; i < elems; ++i) {
        if (elemdata[i].valid && !strcasecmp(elemdata[i].name, name)) {
            output[count++] = i;
            output = realloc(output, (count + 1) * sizeof(*output));
            output[count] = -1;
        }
    }
    if (_count) *_count = count;
    return output;
}

static inline char* getProp(struct ui_elem* e, char* name) {
    for (int i = 0; i < e->properties; ++i) {
        if (e->propertydata[i].name && e->propertydata[i].value && *e->propertydata[i].value) {
            if (!strcasecmp(name, e->propertydata[i].name)) return e->propertydata[i].value;
        }
    }
}

static inline float getSize(char* propval, float max) {
    float num = 0;
    char suff = 0;
    sscanf(propval, "%f%c", &num, &suff);
    if (suff == '%') {
        num = max * num * 0.01;
    }
}

static inline void calcProp(int id) {
    if (!elemValid(id)) return;
    struct ui_elem* e = &elemdata[id];
    struct ui_elem_calcprop p_prop;
    if (elemValid(e->parent)) {
        p_prop = elemdata[e->parent].calcprop;
    } else {
        p_prop = {
            .x = 0,
            .y = 0,
            .width = rendinf.width,
            .height = rendinf.height,
            .z = 0.0
        };
    }
    {
        char* margin_x = getProp(e, "margin_x");
        char* margin_y = getProp(e, "margin_y");
        p_prop.width -= (margin_x) ? atoi(margin_x) * 2 : 0;
        p_prop.height -= (margin_y) ? atoi(margin_y) * 2 : 0;
    }
}
