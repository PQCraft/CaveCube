#include "ui.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static int elems = 0;
static struct ui_elem* elemdata;

int newElem(int type, char* name, int parent, ...) {
    if (parent >= 0) {
        if (parent >= elems || !elemdata[parent].valid) return -1;
    }
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
    if (id >= elems || !elemdata[id].valid) return;
    struct ui_elem* e = &elemdata[id];
    if (name) {
        if (*name) {free(e->name); e->name = strdup(name);}
        else {free(e->name); e->name = NULL;}
    }
    if (parent >= 0 && parent < elems && elemdata[parent].valid) e->parent = parent;
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
    if (id >= elems || !elemdata[id].valid) return;
    struct ui_elem* e = &elemdata[id];
    struct ui_elem* p = &elemdata[e->parent];
    e->valid = false;
    for (int i = 0; i < p->children; ++i) {
        if (p->childdata[i] == id) {
            p->childdata[i] = -1;
            break;
        }
    }
    for (int i = 0; i < e->children; ++i) {
        if (p->childdata[i] >= 0) deleteElem(e->childdata[i]);
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
    if (id >= elems || !elemdata[id].valid) return NULL;
    return &elemdata[id];
}

int getElemByName(char* name, bool reverse) {
    int i;
    if (reverse) {
        i = elems - 1;
    } else {
        i = 0;
    }
    while (1) {
        if (reverse) {
            if (i < 0) break;
        } else {
            if (i >= elems) break;
        }
        if (elemdata[i].valid && !strcasecmp(elemdata[i].name, name)) {
            return i;
        }
        if (reverse) {
            --i;
        } else {
            ++i;
        }
    }
}

int* getElemsByName(char* name, int* _count) {
    int count = 0;
    int* output = malloc(1 * sizeof(*output));
    output[0] = -1;
    for (int i = 0; i < elems; ++i) {
        if (elemdata[i].valid && !strcasecmp(elemdata[i].name, name)) {
            ++count;
            output = realloc(output, (count + 1) * sizeof(*output));
            output[count - 1] = i;
            output[count] = -1;
        }
    }
    if (_count) *_count = count;
    return output;
}
