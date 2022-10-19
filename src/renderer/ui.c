#include "ui.h"

#include "renderer.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

float ui_scale = 1.0;

int ui_elems = 0;
struct ui_elem* ui_elemdata;

#define idValid(x) (x >= 0 && x < ui_elems)
#define elemValid(x) (idValid(x) && ui_elemdata[x].valid)

int newUIElem(int type, char* name, int parent, ...) {
    int index = -1;
    for (int i = 0; i < ui_elems; ++i) {
        if (!ui_elemdata[i].valid) {index = i; break;}
    }
    index = ui_elems++;
    //printf("elems: [%d]\n", ui_elems);
    ui_elemdata = realloc(ui_elemdata, ui_elems * sizeof(*ui_elemdata));
    struct ui_elem* e = &ui_elemdata[index];
    memset(e, 0, sizeof(*e));
    e->type = type;
    if (name && *name) e->name = strdup(name);
    e->parent = parent;
    e->children = 0;
    e->childdata = NULL;
    e->properties = 0;
    e->propertydata = NULL;
    e->calcprop.changed = true;
    va_list args;
    va_start(args, parent);
    for (int i = 0; ; ++i) {
        char* name = va_arg(args, char*);
        if (!name) break;
        char* val = va_arg(args, char*);
        if (!val) continue;
        e->propertydata = realloc(e->propertydata, (i + 1) * sizeof(*e->propertydata));
        e->propertydata[i].name = strdup(name);
        e->propertydata[i].value = strdup(val);
    }
    va_end(args);
    e->valid = true;
    if (elemValid(parent)) {
        struct ui_elem* p = &ui_elemdata[parent];
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

void editUIElem(int id, char* name, int parent, ...) {
    if (!elemValid(id)) return;
    struct ui_elem* e = &ui_elemdata[id];
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
        e->calcprop.changed = true;
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

static bool del = false;

void deleteUIElem(int id) {
    if (!elemValid(id)) return;
    del = true;
    struct ui_elem* e = &ui_elemdata[id];
    e->valid = false;
    if (elemValid(e->parent)) {
        struct ui_elem* p = &ui_elemdata[e->parent];
        for (int i = 0; i < p->children; ++i) {
            if (p->childdata[i] == id) {
                p->childdata[i] = -1;
                break;
            }
        }
    }
    for (int i = 0; i < e->children; ++i) {
        if (e->childdata[i] >= 0) deleteUIElem(e->childdata[i]);
    }
    free(e->name);
    free(e->childdata);
    for (int i = 0; i < e->properties; ++i) {
        free(e->propertydata[i].name);
        free(e->propertydata[i].value);
    }
    free(e->propertydata);
}

struct ui_elem* getUIElemData(int id) {
    return (elemValid(id)) ? &ui_elemdata[id] : NULL;
}

int getUIElemByName(char* name, bool reverse) {
    int i;
    if (reverse) {i = ui_elems - 1;} else {i = 0;}
    while (1) {
        if (reverse) {if (i < 0) break;} else {if (i >= ui_elems) break;}
        if (ui_elemdata[i].valid && !strcasecmp(ui_elemdata[i].name, name)) {
            return i;
        }
        if (reverse) {--i;} else {++i;}
    }
    return -1;
}

int* getUIElemsByName(char* name, int* _count) {
    int count = 0;
    int* output = malloc(1 * sizeof(*output));
    output[0] = -1;
    for (int i = 0; i < ui_elems; ++i) {
        if (ui_elemdata[i].valid && !strcasecmp(ui_elemdata[i].name, name)) {
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
    return NULL;
}

static inline float getSize(char* propval, float max) {
    float num = 0;
    char suff = 0;
    sscanf(propval, "%f%c", &num, &suff);
    if (suff == '%') {
        num = max * num * 0.01;
    }
    return num;
}

static inline bool calcProp(struct ui_elem* e, bool force) {
    struct ui_elem_calcprop p_prop;
    if (elemValid(e->parent)) {
        p_prop = ui_elemdata[e->parent].calcprop;
    } else {
        static int scrw = -1;
        static int scrh = -1;
        p_prop = (struct ui_elem_calcprop){
            .hidden = false,
            .x = 0,
            .y = 0,
            .width = rendinf.width,
            .height = rendinf.height,
            .z = 0.0
        };
        if ((int)rendinf.width != scrw || (int)rendinf.height != scrh) {
            scrw = rendinf.width;
            scrh = rendinf.height;
            force = true;
        }
    }
    if (e->calcprop.changed || force) {
        char* curprop;
        curprop = getProp(e, "margin_x");
        if (curprop) {
            int offset = atoi(curprop) * ui_scale;
            p_prop.x += offset;
            p_prop.width -= offset;
        }
        curprop = getProp(e, "margin_y");
        if (curprop) {
            int offset = atoi(curprop) * ui_scale;
            p_prop.y += offset;
            p_prop.height -= offset;
        }
        curprop = getProp(e, "width");
        e->calcprop.width = (curprop) ? getSize(curprop, (float)p_prop.width / ui_scale) * ui_scale : 0;
        curprop = getProp(e, "height");
        e->calcprop.height = (curprop) ? getSize(curprop, (float)p_prop.height / ui_scale) * ui_scale : 0;
        curprop = getProp(e, "align");
        {
            int ax = -1, ay = -1;
            if (curprop) sscanf(curprop, "%d,%d", &ax, &ay);
            switch (ax) {
                default:;
                    e->calcprop.x = p_prop.x;
                    break;
                case 0:;
                    e->calcprop.x = (p_prop.x + p_prop.width / 2) - (e->calcprop.width + 1) / 2;
                    break;
                case 1:;
                    e->calcprop.x = p_prop.x + (p_prop.width - e->calcprop.width);
                    break;
            }
            switch (ay) {
                default:;
                    e->calcprop.y = p_prop.y;
                    break;
                case 0:;
                    e->calcprop.y = (p_prop.y + p_prop.height / 2) - (e->calcprop.height + 1) / 2;
                    break;
                case 1:;
                    e->calcprop.y = p_prop.y + (p_prop.height - e->calcprop.height);
                    break;
            }
        }
        curprop = getProp(e, "x_offset");
        if (curprop) e->calcprop.x += atoi(curprop) * ui_scale;
        curprop = getProp(e, "y_offset");
        if (curprop) e->calcprop.y += atoi(curprop) * ui_scale;
        curprop = getProp(e, "z");
        e->calcprop.z = (curprop) ? atof(curprop) : p_prop.z;
        curprop = getProp(e, "hidden");
        e->calcprop.z = (curprop) ? getBool(curprop) : false;
        e->calcprop.changed = false;
        return true;
    }
    return false;
}

static inline bool calcPropTree(struct ui_elem* e, bool force) {
    bool ret = calcProp(e, force);
    if (ret) {
        for (int i = 0; i < e->children; ++i) {
            if (idValid(e->childdata[i])) calcPropTree(&ui_elemdata[e->childdata[i]], true);
        }
    }
    return ret;
}

bool calcUIProperties() {
    bool ret = del;
    if (del) del = false;
    for (int i = 0; i < ui_elems; ++i) {
        struct ui_elem* e = &ui_elemdata[i];
        if (e->parent < 0) {
            if (calcPropTree(e, false)) ret = true;
        }
    }
    return ret;
}
