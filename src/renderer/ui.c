#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "ui.h"
#include "renderer.h"
#include <input/input.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#define idValid(x) isUIIdValid(elemdata, x)
#define elemValid(x) isUIElemValid(elemdata, x)

struct ui_data* allocUI() {
    struct ui_data* data = calloc(1, sizeof(*data));
    glGenBuffers(1, &data->renddata.VBO);
    data->scale = 1;
    data->scrw = -1;
    data->scrh = -1;
    return data;
}

int newUIElem(struct ui_data* elemdata, int type, ...) {
    int index = -1;
    for (int i = 0; i < elemdata->count; ++i) {
        if (!elemdata->data[i].valid) {index = i; break;}
    }
    if (index == -1) {
        index = elemdata->count++;
        elemdata->data = realloc(elemdata->data, elemdata->count * sizeof(*elemdata->data));
    }
    struct ui_elem* e = &elemdata->data[index];
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->parent = -1;
    e->prev = -1;
    e->children = 0;
    e->childdata = NULL;
    e->properties = 0;
    e->propertydata = NULL;
    e->changed = true;
    va_list args;
    va_start(args, type);
    while (1) {
        int attr = va_arg(args, int);
        if (attr == UI_ATTR_DONE) break;
        switch (attr) {
            case UI_ATTR_NAME:; {
                char* name = va_arg(args, char*);
                if (name && *name) {
                    free(e->name);
                    e->name = strdup(name);
                }
            } break;
            case UI_ATTR_PARENT:; {
                e->parent = va_arg(args, int);
            } break;
            case UI_ATTR_PREV:; {
                e->prev = va_arg(args, int);
            } break;
            case UI_ATTR_CALLBACK:; {
                e->callback = va_arg(args, ui_event_callback_t*);
            } break;
        }
    }
    for (int i = 0; ; ++i) {
        char* name = va_arg(args, char*);
        if (!name) break;
        char* val = va_arg(args, char*);
        if (!val) continue;
        e->propertydata = realloc(e->propertydata, (i + 1) * sizeof(*e->propertydata));
        e->propertydata[i].name = strdup(name);
        e->propertydata[i].value = strdup(val);
        ++e->properties;
    }
    va_end(args);
    e->valid = true;
    if (elemValid(e->parent)) {
        struct ui_elem* p = &elemdata->data[e->parent];
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

void editUIElem(struct ui_data* elemdata, int id, ...) {
    if (!elemValid(id)) return;
    struct ui_elem* e = &elemdata->data[id];
    bool newparent = false;
    int newparentid = -1;
    va_list args;
    va_start(args, id);
    while (1) {
        int attr = va_arg(args, int);
        if (attr == UI_ATTR_DONE) break;
        e->changed = true;
        switch (attr) {
            case UI_ATTR_NAME:; {
                free(e->name);
                char* name = va_arg(args, char*);
                if (name) {
                    if (*name) e->name = strdup(name);
                } else {
                    e->name = NULL;
                }
            } break;
            case UI_ATTR_PARENT:; {
                newparent = true;
                newparentid = va_arg(args, int);
            } break;
            case UI_ATTR_PREV:; {
                e->prev = va_arg(args, int);
            } break;
            case UI_ATTR_STATE:; {
                e->state = va_arg(args, int);
                e->changed = true;
            } break;
        }
    }
    while (1) {
        char* name = va_arg(args, char*);
        if (!name) break;
        char* val = va_arg(args, char*);
        e->changed = true;
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
    if (newparent) {
        if (elemValid(e->parent)) {
            struct ui_elem* p = &elemdata->data[e->parent];
            for (int i = 0; i < p->children; ++i) {
                if (p->childdata[i] == id) {
                    p->childdata[i] = -1;
                    break;
                }
            }
        }
        e->parent = newparentid;
        if (elemValid(e->parent)) {
            struct ui_elem* p = &elemdata->data[e->parent];
            int cindex = -1;
            for (int i = 0; i < p->children; ++i) {
                if (p->childdata[i] < 0) {cindex = i; break;}
            }
            if (cindex < 0) {
                cindex = p->children++;
                p->childdata = realloc(p->childdata, p->children * sizeof(*p->childdata));
            }
            p->childdata[cindex] = id;
        }
    }
}

void deleteUIElem(struct ui_data* elemdata, int id) {
    if (!elemValid(id)) return;
    elemdata->del = true;
    struct ui_elem* e = &elemdata->data[id];
    e->valid = false;
    if (elemValid(e->parent)) {
        struct ui_elem* p = &elemdata->data[e->parent];
        for (int i = 0; i < p->children; ++i) {
            if (p->childdata[i] == id) {
                p->childdata[i] = -1;
                break;
            }
        }
    }
    for (int i = 0; i < e->children; ++i) {
        if (e->childdata[i] >= 0) deleteUIElem(elemdata, e->childdata[i]);
    }
    free(e->name);
    free(e->childdata);
    for (int i = 0; i < e->properties; ++i) {
        free(e->propertydata[i].name);
        free(e->propertydata[i].value);
    }
    free(e->propertydata);
}

void clearUIElems(struct ui_data* elemdata) {
    for (int i = 0; i < elemdata->count; ++i) {
        if (elemdata->data[i].parent < 0) {
            deleteUIElem(elemdata, i);
        }
    }
}

struct ui_elem* getUIElemData(struct ui_data* elemdata, int id) {
    return (elemValid(id)) ? &elemdata->data[id] : NULL;
}

int getUIElemByName(struct ui_data* elemdata, char* name, bool reverse) {
    int i;
    if (reverse) {i = elemdata->count - 1;} else {i = 0;}
    while (1) {
        if (reverse) {if (i < 0) break;} else {if (i >= elemdata->count) break;}
        if (elemdata->data[i].valid && !strcasecmp(elemdata->data[i].name, name)) {
            return i;
        }
        if (reverse) {--i;} else {++i;}
    }
    return -1;
}

int* getUIElemsByName(struct ui_data* elemdata, char* name, int* _count) {
    int count = 0;
    int* output = malloc(1 * sizeof(*output));
    output[0] = -1;
    for (int i = 0; i < elemdata->count; ++i) {
        if (elemdata->data[i].valid && !strcasecmp(elemdata->data[i].name, name)) {
            output[count++] = i;
            output = realloc(output, (count + 1) * sizeof(*output));
            output[count] = -1;
        }
    }
    if (_count) *_count = count;
    return output;
}

bool doUIEvents(struct input_info* inf, struct ui_data* elemdata) {
    //printf("TEST: [%d]\n", elemdata->count);
    if (elemdata->hidden) goto resetstate;
    int z = -256;
    int hit = -1;
    for (int i = elemdata->count - 1; i >= 0; --i) {
        if (!elemValid(i)) continue;
        struct ui_elem* e = &elemdata->data[i];
        if (e->calcprop.hidden) {/*printf("hidden: {%s}\n", e->name);*/ continue;}
        //printf("[%d]: [%d, %d] vs [%d, %d, %d, %d]; [%d] vs [%d]\n", i,
        //    inf->ui_mouse_x, inf->ui_mouse_y, e->calcprop.x, e->calcprop.y, e->calcprop.x + e->calcprop.width, e->calcprop.y + e->calcprop.height,
        //    z, e->calcprop.z);
        if (inf->ui_mouse_x >= e->calcprop.x && inf->ui_mouse_x < e->calcprop.x + e->calcprop.width &&
        inf->ui_mouse_y >= e->calcprop.y && inf->ui_mouse_y < e->calcprop.y + e->calcprop.height) {
            if ((int)e->calcprop.z <= z) {
                continue;
            } else {
                z = (int)e->calcprop.z;
                //printf("NEW Z: [%d]\n", z);
            }
            //printf("Hit: {%s}\n", e->name);
            hit = i;
        }
    }
    //printf("DONE: [z:%d]\n", z);
    if (hit > -1) {
        struct ui_elem* e = &elemdata->data[hit];
        int newstate = (inf->ui_mouse_click) ? UI_STATE_CLICKED : UI_STATE_HOVERED;
        bool fireevent = false;
        if (e->state != newstate) {
            fireevent = true;
            for (int i = 0; i < hit; ++i) {
                if (elemdata->data[i].state != UI_STATE_NORMAL) editUIElem(elemdata, i, UI_ATTR_STATE, UI_STATE_NORMAL, UI_ATTR_DONE, NULL);
            }
            editUIElem(elemdata, hit, UI_ATTR_STATE, newstate, UI_ATTR_DONE, NULL);
            for (int i = hit + 1; i < elemdata->count; ++i) {
                if (elemdata->data[i].state != UI_STATE_NORMAL) editUIElem(elemdata, i, UI_ATTR_STATE, UI_STATE_NORMAL, UI_ATTR_DONE, NULL);
            }
        }
        if (fireevent && e->callback) {
            if (inf->ui_mouse_click) {
                e->callback(elemdata, hit, e, UI_EVENT_CLICK);
            }
        }
        return true;
    } else {
        resetstate:;
        for (int i = 0; i < elemdata->count; ++i) {
            if (elemdata->data[i].state != UI_STATE_NORMAL) editUIElem(elemdata, i, UI_ATTR_STATE, UI_STATE_NORMAL, UI_ATTR_DONE, NULL);
        }
    }
    return false;
}

static force_inline char* getProp(struct ui_elem* e, char* name) {
    for (int i = 0; i < e->properties; ++i) {
        if (e->propertydata[i].name && e->propertydata[i].value && *e->propertydata[i].value) {
            if (!strcasecmp(name, e->propertydata[i].name)) return e->propertydata[i].value;
        }
    }
    return NULL;
}

char* getUIElemProperty(struct ui_elem* e, char* name) {
    return getProp(e, name);
}

static force_inline float getSize(char* propval, float max) {
    float num = 0;
    char suff = 0;
    sscanf(propval, "%f%c", &num, &suff);
    if (suff == '%') {
        num = max * num * 0.01;
    }
    return num;
}

static force_inline bool calcProp(struct ui_data* elemdata, struct ui_elem* e, bool force) {
    struct ui_elem_calcprop p_prop;
    if (elemValid(e->parent)) {
        p_prop = elemdata->data[e->parent].calcprop;
    } else {
        p_prop = (struct ui_elem_calcprop){
            .hidden = false,
            .x = 0,
            .y = 0,
            .width = rendinf.width,
            .height = rendinf.height,
            .z = 0
        };
    }
    if (e->changed || force) {
        char* curprop;
        curprop = getProp(e, "margin");
        int mx0 = 0, my0 = 0, mx1 = 0, my1 = 0;
        if (curprop) {
            sscanf(curprop, "%d,%d,%d,%d", &mx0, &my0, &mx1, &my1);
            mx0 *= elemdata->scale;
            my0 *= elemdata->scale;
            mx1 *= elemdata->scale;
            my1 *= elemdata->scale;
            p_prop.x += mx0;
            p_prop.width -= mx0 + mx1;
            p_prop.y += my0;
            p_prop.height -= my0 + my1;
            e->calcprop.marginl = mx0;
            e->calcprop.margint = my0;
            e->calcprop.marginr = mx1;
            e->calcprop.marginb = my1;
        }
        switch (e->type) {
            case UI_ELEM_HOTBAR:; {
                e->calcprop.width = 302 * elemdata->scale;
                e->calcprop.height = 32 * elemdata->scale;
            } break;
            case UI_ELEM_ITEMGRID:; {
                curprop = getProp(e, "width");
                e->calcprop.width = (curprop) ? atoi(curprop) : 1;
                curprop = getProp(e, "height");
                e->calcprop.height = (curprop) ? atoi(curprop) : 1;
                e->calcprop.width = (e->calcprop.width * 30 + 2) * elemdata->scale;
                e->calcprop.height = (e->calcprop.height * 30 + 2) * elemdata->scale;
            } break;
            default:; {
                curprop = getProp(e, "width");
                e->calcprop.width = (curprop) ? getSize(curprop, (float)p_prop.width / elemdata->scale) * elemdata->scale : 0;
                curprop = getProp(e, "height");
                e->calcprop.height = (curprop) ? getSize(curprop, (float)p_prop.height / elemdata->scale) * elemdata->scale : 0;
            } break;
        }
        bool prev = elemValid(e->prev);
        struct ui_elem_calcprop l_prop;
        if (prev) {
            l_prop = elemdata->data[e->prev].calcprop;
        }
        {
            curprop = getProp(e, "align");
            if (curprop) sscanf(curprop, "%hhd,%hhd", &e->calcprop.alignx, &e->calcprop.aligny);
            float x0 = p_prop.x;
            float x1 = p_prop.x + p_prop.width;
            switch (e->calcprop.alignx) {
                case -1:;
                    e->calcprop.x = x0;
                    break;
                default:;
                    e->calcprop.x = roundf((float)(x0 + x1) / 2.0 - (float)e->calcprop.width / 2.0);
                    break;
                case 1:;
                    e->calcprop.x = x1 - e->calcprop.width;
                    break;
            }
            float y0 = (prev) ? l_prop.y + l_prop.height + my0 + l_prop.marginb : p_prop.y;
            float y1 = (prev) ? l_prop.y - my1 - l_prop.margint : p_prop.y + p_prop.height;
            switch (e->calcprop.aligny) {
                case -1:;
                    e->calcprop.y = y0;
                    break;
                default:;
                    e->calcprop.y = roundf((float)(y0 + y1) / 2.0 - (float)e->calcprop.height / 2.0);
                    break;
                case 1:;
                    e->calcprop.y = y1 - e->calcprop.height;
                    break;
            }
        }
        curprop = getProp(e, "x_offset");
        if (curprop) e->calcprop.x += atof(curprop) * elemdata->scale;
        curprop = getProp(e, "y_offset");
        if (curprop) e->calcprop.y += atof(curprop) * elemdata->scale;
        curprop = getProp(e, "z");
        e->calcprop.z = (curprop) ? atoi(curprop) : p_prop.z;
        if (p_prop.hidden) {
            e->calcprop.hidden = true;
        } else {
            curprop = getProp(e, "hidden");
            e->calcprop.hidden = (curprop) ? getBool(curprop) : false;
        }
        e->changed = false;
        e->calcprop.a = 255;
        curprop = getUIElemProperty(e, "alpha");
        if (curprop) e->calcprop.a = 255.0 * atof(curprop);
        e->calcprop.r = 127;
        e->calcprop.g = 127;
        e->calcprop.b = 127;
        curprop = getUIElemProperty(e, "color");
        if (curprop) sscanf(curprop, "#%02hhx%02hhx%02hhx", &e->calcprop.r, &e->calcprop.g, &e->calcprop.b);
        return true;
    }
    return false;
}

static inline bool calcPropTree(struct ui_data* elemdata, struct ui_elem* e, bool force) {
    bool ret = calcProp(elemdata, e, force);
    if (ret) {
        for (int i = 0; i < e->children; ++i) {
            if (idValid(e->childdata[i])) calcPropTree(elemdata, &elemdata->data[e->childdata[i]], true);
        }
    }
    return ret;
}

bool calcUIProperties(struct ui_data* elemdata) {
    bool ret = elemdata->del;
    if (ret) elemdata->del = false;
    bool force = false;
    if ((int)rendinf.width != elemdata->scrw || (int)rendinf.height != elemdata->scrh) {
        elemdata->scrw = rendinf.width;
        elemdata->scrh = rendinf.height;
        force = true;
    }
    for (int i = 0; i < elemdata->count; ++i) {
        struct ui_elem* e = &elemdata->data[i];
        if (e->parent < 0) {
            if (calcPropTree(elemdata, e, force)) ret = true;
        }
    }
    return ret;
}

void freeUI(struct ui_data* elemdata) {
    clearUIElems(elemdata);
    glDeleteBuffers(1, &elemdata->renddata.VBO);
    free(elemdata->data);
    free(elemdata);
}

#endif
