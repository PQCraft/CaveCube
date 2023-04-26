#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "ui.h"
#include "renderer.h"
#include <input/input.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static inline bool badElem(struct ui_layer* layer, int id) {
    return id < 0 || id >= layer->elems || layer->elemdata[id].type < 0;
}

bool isUIElemInvalid(struct ui_layer* layer, int id) {
    return badElem(layer, id);
}

struct ui_layer* allocUI(char* name) {
    struct ui_layer* layer = malloc(sizeof(*layer));
    pthread_mutex_init(&layer->lock, NULL);
    layer->name = (name) ? strdup(name) : NULL;
    layer->hidden = true;
    layer->scale = 1.0;
    layer->width = -1;
    layer->height = -1;
    layer->elems = 0;
    layer->elemdata = NULL;
    layer->children = 0;
    layer->childdata = NULL;
    layer->recalc = true;
    layer->remesh = true;
    return layer;
}

static void deleteSingleElem(struct ui_elem* e) {
    free(e->attribs.name);
    free(e->attribs.size.width);
    free(e->attribs.size.height);
    free(e->attribs.minsize.width);
    free(e->attribs.minsize.height);
    free(e->attribs.maxsize.width);
    free(e->attribs.maxsize.height);
    free(e->attribs.margin.top);
    free(e->attribs.margin.bottom);
    free(e->attribs.margin.left);
    free(e->attribs.margin.right);
    free(e->attribs.padding.top);
    free(e->attribs.padding.bottom);
    free(e->attribs.padding.left);
    free(e->attribs.padding.right);
    free(e->attribs.offset.x);
    free(e->attribs.offset.y);
    free(e->attribs.text);
    free(e->attribs.textoffset.x);
    free(e->attribs.textoffset.y);
    free(e->attribs.tooltip);
    switch (e->type) {
        case UI_ELEM_TEXTBOX:;
            free(e->attribs.textbox.shadowtext);
            break;
        case UI_ELEM_HOTBAR:;
            free(e->attribs.hotbar.items);
            break;
        case UI_ELEM_ITEMGRID:;
            free(e->attribs.itemgrid.items);
            break;
    }
    e->type = -1;
    free(e->childdata);
}

void freeUI(struct ui_layer* layer) {
    for (int i = 0; i < layer->elems; ++i) {
        deleteSingleElem(&layer->elemdata[i]);
    }
    free(layer->elemdata);
    free(layer->childdata);
    free(layer->name);
    pthread_mutex_destroy(&layer->lock);
}

static void attachChild(int* children, int** childdata, int id) {
    for (int i = 0; i < *children; ++i) {
        if ((*childdata)[i] < 0) {
            (*childdata)[i] = id;
            return;
        }
    }
    int index = *children;
    *children += 4;
    *childdata = realloc(*childdata, *children * sizeof(**childdata));
    (*childdata)[index] = id;
    for (int i = index + 1; i < *children; ++i) {
        (*childdata)[i] = -1;
    }
}

static void dettachChild(int* children, int** childdata, int id) {
    for (int i = 0; i < *children; ++i) {
        if ((*childdata)[i] == id) {
            (*childdata)[i] = -1;
            return;
        }
    }
}

static inline char* strdupn(char* str) {
    if (!str) return NULL;
    return strdup(str);
}

int newUIElem(struct ui_layer* layer, int type, int parent, ...) {
    pthread_mutex_lock(&layer->lock);
    if (parent >= 0 && badElem(layer, parent)) {
        pthread_mutex_unlock(&layer->lock);
        return -UI_ERROR_BADELEM;
    }
    int elemid = -1;
    for (int i = 0; i < layer->elems; ++i) {
        if (layer->elemdata[i].type < 0) {
            elemid = i;
            break;
        }
    }
    if (elemid == -1) {
        elemid = layer->elems++;
        layer->elemdata = realloc(layer->elemdata, layer->elems * sizeof(*layer->elemdata));
    }
    struct ui_elem* e = &layer->elemdata[elemid];
    e->type = type;
    e->parent = parent;
    memset(&e->attribs, 0, sizeof(e->attribs));
    memset(&e->calcattribs, 0, sizeof(e->calcattribs));
    e->children = 0;
    e->childdata = NULL;
    e->changed = true;
    e->update = false;
    e->attribs.anchor = -1;
    e->attribs.color.r = 255;
    e->attribs.color.g = 255;
    e->attribs.color.b = 255;
    e->attribs.alpha = 255;
    e->attribs.textscale = 1.0;
    e->attribs.textcolor.fg = 15;
    e->attribs.textalpha.fg = 255;
    switch (type) {
        case UI_ELEM_TEXTBOX:;
            e->attribs.textalign.x = -1;
            break;
        case UI_ELEM_HOTBAR:;
            e->attribs.hotbar.slot = -1;
            break;
        case UI_ELEM_ITEMGRID:;
            e->attribs.itemgrid.cell.x = -1;
            e->attribs.itemgrid.cell.y = -1;
            break;
    }
    va_list args;
    va_start(args, parent);
    while (1) {
        int attrib = va_arg(args, int);
        switch (attrib) {
            case UI_END:; {
                goto longbreak;
            } break;
            case UI_ATTR_NAME:; {
                e->attribs.name = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_CALLBACK:; {
                e->attribs.callback = va_arg(args, ui_callback_t);
            } break;
            case UI_ATTR_HIDDEN:; {
                e->attribs.hidden = va_arg(args, int);
            } break;
            case UI_ATTR_DISABLED:; {
                e->attribs.disabled = va_arg(args, int);
            } break;
            case UI_ATTR_ANCHOR:; {
                e->attribs.anchor = va_arg(args, int);
            } break;
            case UI_ATTR_SIZE:; {
                e->attribs.size.width = strdupn(va_arg(args, char*));
                e->attribs.size.height = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_MINSIZE:; {
                e->attribs.minsize.width = strdupn(va_arg(args, char*));
                e->attribs.minsize.height = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_MAXSIZE:; {
                e->attribs.maxsize.width = strdupn(va_arg(args, char*));
                e->attribs.maxsize.height = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_Z:; {
                e->attribs.z = va_arg(args, double);
            } break;
            case UI_ATTR_ALIGN:; {
                e->attribs.align.x = va_arg(args, int);
                e->attribs.align.y = va_arg(args, int);
            } break;
            case UI_ATTR_MARGIN:; {
                e->attribs.margin.top = strdupn(va_arg(args, char*));
                e->attribs.margin.bottom = strdupn(va_arg(args, char*));
                e->attribs.margin.left = strdupn(va_arg(args, char*));
                e->attribs.margin.right = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_PADDING:; {
                e->attribs.padding.top = strdupn(va_arg(args, char*));
                e->attribs.padding.bottom = strdupn(va_arg(args, char*));
                e->attribs.padding.left = strdupn(va_arg(args, char*));
                e->attribs.padding.right = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_OFFSET:; {
                e->attribs.offset.x = strdupn(va_arg(args, char*));
                e->attribs.offset.y = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_COLOR:; {
                e->attribs.color.r = va_arg(args, int);
                e->attribs.color.g = va_arg(args, int);
                e->attribs.color.b = va_arg(args, int);
            } break;
            case UI_ATTR_ALPHA:; {
                e->attribs.alpha = va_arg(args, int);
            } break;
            case UI_ATTR_TEXT:; {
                e->attribs.text = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_TEXTALIGN:; {
                e->attribs.textalign.x = va_arg(args, int);
                e->attribs.textalign.y = va_arg(args, int);
            } break;
            case UI_ATTR_TEXTOFFSET:; {
                e->attribs.textoffset.x = strdupn(va_arg(args, char*));
                e->attribs.textoffset.y = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_TEXTSCALE:; {
                e->attribs.textscale = va_arg(args, double);
            } break;
            case UI_ATTR_TEXTCOLOR:; {
                int fg = va_arg(args, int);
                if (fg >= 0) e->attribs.textcolor.fg = fg;
                int bg = va_arg(args, int);
                if (bg >= 0) e->attribs.textcolor.bg = bg;
            } break;
            case UI_ATTR_TEXTALPHA:; {
                int fg = va_arg(args, int);
                if (fg >= 0) e->attribs.textalpha.fg = fg;
                int bg = va_arg(args, int);
                if (bg >= 0) e->attribs.textalpha.bg = bg;
            } break;
            case UI_ATTR_TEXTATTRIB:; {
                e->attribs.textattrib.b = va_arg(args, int);
                e->attribs.textattrib.u = va_arg(args, int);
                e->attribs.textattrib.i = va_arg(args, int);
                e->attribs.textattrib.s = va_arg(args, int);
            } break;
            case UI_ATTR_RICHTEXT:; {
                e->attribs.richtext = va_arg(args, int);
            } break;
            case UI_ATTR_TOOLTIP:; {
                e->attribs.tooltip = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_TOGGLE_STATE:; {
                int state = va_arg(args, int);
                if (type == UI_ELEM_TOGGLE) e->attribs.toggle.state = state;
            } break;
            case UI_ATTR_TEXTBOX_EDITABLE:; {
                int editable = va_arg(args, int);
                if (type == UI_ELEM_TEXTBOX) e->attribs.textbox.editable = editable;
            } break;
            case UI_ATTR_TEXTBOX_SHADOWTEXT:; {
                char* shadowtext = va_arg(args, char*);
                if (type == UI_ELEM_TEXTBOX) e->attribs.textbox.shadowtext = strdupn(shadowtext);
            } break;
            case UI_ATTR_PROGRESSBAR_PROGRESS:; {
                double progress = va_arg(args, double);
                if (type == UI_ELEM_PROGRESSBAR) e->attribs.progressbar.progress = progress;
            } break;
            case UI_ATTR_SCROLLBAR_MAXSCROLL:; {
                double maxscroll = va_arg(args, double);
                if (type == UI_ELEM_SCROLLBAR) e->attribs.scrollbar.maxscroll = maxscroll;
            } break;
            case UI_ATTR_SCROLLBAR_SCROLL:; {
                double scroll = va_arg(args, double);
                if (type == UI_ELEM_SCROLLBAR) e->attribs.scrollbar.scroll = scroll;
            } break;
            case UI_ATTR_HOTBAR_SLOT:; {
                int slot = va_arg(args, int);
                if (type == UI_ELEM_HOTBAR) e->attribs.hotbar.slot = slot;
            } break;
            case UI_ATTR_HOTBAR_ITEMS:; {
                char* items = va_arg(args, char*);
                if (type == UI_ELEM_HOTBAR) e->attribs.hotbar.items = strdupn(items);
            } break;
            case UI_ATTR_ITEMGRID_CELL:; {
                int x = va_arg(args, int);
                int y = va_arg(args, int);
                if (type == UI_ELEM_ITEMGRID) {
                    e->attribs.itemgrid.cell.x = x;
                    e->attribs.itemgrid.cell.y = y;
                }
            } break;
            case UI_ATTR_ITEMGRID_ITEMS:; {
                char* items = va_arg(args, char*);
                if (type == UI_ELEM_ITEMGRID) e->attribs.itemgrid.items = strdupn(items);
            } break;
        }
    }
    longbreak:;
    va_end(args);
    if (!e->attribs.size.width) e->attribs.size.width = strdup("1t");
    if (!e->attribs.size.height) e->attribs.size.height = strdup("1t");
    if (!e->attribs.minsize.width) e->attribs.minsize.width = strdup("");
    if (!e->attribs.minsize.height) e->attribs.minsize.height = strdup("");
    if (!e->attribs.maxsize.width) e->attribs.maxsize.width = strdup("");
    if (!e->attribs.maxsize.height) e->attribs.maxsize.height = strdup("");
    if (!e->attribs.margin.top) e->attribs.margin.top = strdup("0");
    if (!e->attribs.margin.bottom) e->attribs.margin.bottom = strdup("0");
    if (!e->attribs.margin.left) e->attribs.margin.left = strdup("0");
    if (!e->attribs.margin.right) e->attribs.margin.right = strdup("0");
    switch (type) {
        case UI_ELEM_FANCYBOX:;
            if (!e->attribs.padding.top) e->attribs.padding.top = strdup("8");
            if (!e->attribs.padding.bottom) e->attribs.padding.bottom = strdup("8");
            if (!e->attribs.padding.left) e->attribs.padding.left = strdup("8");
            if (!e->attribs.padding.right) e->attribs.padding.right = strdup("8");
            break;
        case UI_ELEM_BUTTON:;
            if (!e->attribs.padding.top) e->attribs.padding.top = strdup("1");
            if (!e->attribs.padding.bottom) e->attribs.padding.bottom = strdup("1");
            if (!e->attribs.padding.left) e->attribs.padding.left = strdup("4");
            if (!e->attribs.padding.right) e->attribs.padding.right = strdup("4");
            break;
        case UI_ELEM_HOTBAR:;
            if (!e->attribs.hotbar.items) e->attribs.hotbar.items = strdup("");
            break;
        case UI_ELEM_ITEMGRID:;
            if (!e->attribs.itemgrid.items) e->attribs.itemgrid.items = strdup("");
            break;
        default:;
            if (!e->attribs.padding.top) e->attribs.padding.top = strdup("0");
            if (!e->attribs.padding.bottom) e->attribs.padding.bottom = strdup("0");
            if (!e->attribs.padding.left) e->attribs.padding.left = strdup("0");
            if (!e->attribs.padding.right) e->attribs.padding.right = strdup("0");
            break;
    }
    if (parent >= 0) {
        struct ui_elem* p = &layer->elemdata[parent];
        attachChild(&p->children, &p->childdata, elemid);
    } else {
        attachChild(&layer->children, &layer->childdata, elemid);
    }
    layer->recalc = true;
    pthread_mutex_unlock(&layer->lock);
    return elemid;
}

int editUIElem(struct ui_layer* layer, int id, ...) {
    pthread_mutex_lock(&layer->lock);
    if (badElem(layer, id)) {
        pthread_mutex_unlock(&layer->lock);
        return UI_ERROR_BADELEM;
    }
    struct ui_elem* e = &layer->elemdata[id];
    va_list args;
    va_start(args, id);
    while (1) {
        int attrib = va_arg(args, int);
        switch (attrib) {
            case UI_END:; {
                goto longbreak;
            } break;
            case UI_ATTR_NAME:; {
                free(e->attribs.name);
                e->attribs.name = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_CALLBACK:; {
                e->attribs.callback = va_arg(args, ui_callback_t);
            } break;
            case UI_ATTR_HIDDEN:; {
                e->attribs.hidden = va_arg(args, int);
            } break;
            case UI_ATTR_DISABLED:; {
                e->attribs.disabled = va_arg(args, int);
            } break;
            case UI_ATTR_ANCHOR:; {
                e->attribs.anchor = va_arg(args, int);
            } break;
            case UI_ATTR_SIZE:; {
                char* width = va_arg(args, char*);
                char* height = va_arg(args, char*);
                if (width) {
                    free(e->attribs.size.width);
                    e->attribs.size.width = strdupn(width);
                }
                if (height) {
                    free(e->attribs.size.height);
                    e->attribs.size.height = strdupn(height);
                }
            } break;
            case UI_ATTR_MINSIZE:; {
                char* width = va_arg(args, char*);
                char* height = va_arg(args, char*);
                if (width) {
                    free(e->attribs.minsize.width);
                    e->attribs.minsize.width = strdupn(width);
                }
                if (height) {
                    free(e->attribs.minsize.height);
                    e->attribs.minsize.height = strdupn(height);
                }
            } break;
            case UI_ATTR_MAXSIZE:; {
                char* width = va_arg(args, char*);
                char* height = va_arg(args, char*);
                if (width) {
                    free(e->attribs.maxsize.width);
                    e->attribs.maxsize.width = strdupn(width);
                }
                if (height) {
                    free(e->attribs.maxsize.height);
                    e->attribs.maxsize.height = strdupn(height);
                }
            } break;
            case UI_ATTR_Z:; {
                e->attribs.z = va_arg(args, double);
            } break;
            case UI_ATTR_ALIGN:; {
                e->attribs.align.x = va_arg(args, int);
                e->attribs.align.y = va_arg(args, int);
            } break;
            case UI_ATTR_MARGIN:; {
                char* top = va_arg(args, char*);
                char* bottom = va_arg(args, char*);
                char* left = va_arg(args, char*);
                char* right = va_arg(args, char*);
                if (top) {
                    free(e->attribs.margin.top);
                    e->attribs.margin.top = strdupn(top);
                }
                if (bottom) {
                    free(e->attribs.margin.bottom);
                    e->attribs.margin.bottom = strdupn(bottom);
                }
                if (left) {
                    free(e->attribs.margin.left);
                    e->attribs.margin.left = strdupn(left);
                }
                if (right) {
                    free(e->attribs.margin.right);
                    e->attribs.margin.right = strdupn(right);
                }
            } break;
            case UI_ATTR_PADDING:; {
                char* top = va_arg(args, char*);
                char* bottom = va_arg(args, char*);
                char* left = va_arg(args, char*);
                char* right = va_arg(args, char*);
                if (top) {
                    free(e->attribs.padding.top);
                    e->attribs.padding.top = strdupn(top);
                }
                if (bottom) {
                    free(e->attribs.padding.bottom);
                    e->attribs.padding.bottom = strdupn(bottom);
                }
                if (left) {
                    free(e->attribs.padding.left);
                    e->attribs.padding.left = strdupn(left);
                }
                if (right) {
                    free(e->attribs.padding.right);
                    e->attribs.padding.right = strdupn(right);
                }
            } break;
            case UI_ATTR_OFFSET:; {
                char* x = va_arg(args, char*);
                char* y = va_arg(args, char*);
                if (x) {
                    free(e->attribs.offset.x);
                    e->attribs.offset.x = strdupn(x);
                }
                if (y) {
                    free(e->attribs.offset.y);
                    e->attribs.offset.y = strdupn(y);
                }
            } break;
            case UI_ATTR_COLOR:; {
                e->attribs.color.r = va_arg(args, int);
                e->attribs.color.g = va_arg(args, int);
                e->attribs.color.b = va_arg(args, int);
            } break;
            case UI_ATTR_ALPHA:; {
                e->attribs.alpha = va_arg(args, int);
            } break;
            case UI_ATTR_TEXT:; {
                free(e->attribs.text);
                e->attribs.text = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_TEXTALIGN:; {
                e->attribs.textalign.x = va_arg(args, int);
                e->attribs.textalign.y = va_arg(args, int);
            } break;
            case UI_ATTR_TEXTOFFSET:; {
                char* x = va_arg(args, char*);
                char* y = va_arg(args, char*);
                if (x) {
                    free(e->attribs.textoffset.x);
                    e->attribs.textoffset.x = strdupn(x);
                }
                if (y) {
                    free(e->attribs.textoffset.y);
                    e->attribs.textoffset.y = strdupn(y);
                }
            } break;
            case UI_ATTR_TEXTSCALE:; {
                e->attribs.textscale = va_arg(args, double);
            } break;
            case UI_ATTR_TEXTCOLOR:; {
                int fg = va_arg(args, int);
                if (fg >= 0) e->attribs.textcolor.fg = fg;
                int bg = va_arg(args, int);
                if (bg >= 0) e->attribs.textcolor.bg = bg;
            } break;
            case UI_ATTR_TEXTALPHA:; {
                int fg = va_arg(args, int);
                if (fg >= 0) e->attribs.textalpha.fg = fg;
                int bg = va_arg(args, int);
                if (bg >= 0) e->attribs.textalpha.bg = bg;
            } break;
            case UI_ATTR_TEXTATTRIB:; {
                e->attribs.textattrib.b = va_arg(args, int);
                e->attribs.textattrib.u = va_arg(args, int);
                e->attribs.textattrib.i = va_arg(args, int);
                e->attribs.textattrib.s = va_arg(args, int);
            } break;
            case UI_ATTR_RICHTEXT:; {
                e->attribs.richtext = va_arg(args, int);
            } break;
            case UI_ATTR_TOOLTIP:; {
                free(e->attribs.tooltip);
                e->attribs.tooltip = strdupn(va_arg(args, char*));
            } break;
            case UI_ATTR_TOGGLE_STATE:; {
                int state = va_arg(args, int);
                if (e->type == UI_ELEM_TOGGLE) e->attribs.toggle.state = state;
            } break;
            case UI_ATTR_TEXTBOX_EDITABLE:; {
                int editable = va_arg(args, int);
                if (e->type == UI_ELEM_TEXTBOX) e->attribs.textbox.editable = editable;
            } break;
            case UI_ATTR_TEXTBOX_SHADOWTEXT:; {
                char* shadowtext = va_arg(args, char*);
                if (e->type == UI_ELEM_TEXTBOX) {
                    free(e->attribs.textbox.shadowtext);
                    e->attribs.textbox.shadowtext = strdupn(shadowtext);
                }
            } break;
            case UI_ATTR_PROGRESSBAR_PROGRESS:; {
                double progress = va_arg(args, double);
                if (e->type == UI_ELEM_PROGRESSBAR) e->attribs.progressbar.progress = progress;
            } break;
            case UI_ATTR_SCROLLBAR_MAXSCROLL:; {
                double maxscroll = va_arg(args, double);
                if (e->type == UI_ELEM_SCROLLBAR) e->attribs.scrollbar.maxscroll = maxscroll;
            } break;
            case UI_ATTR_SCROLLBAR_SCROLL:; {
                double scroll = va_arg(args, double);
                if (e->type == UI_ELEM_SCROLLBAR) e->attribs.scrollbar.scroll = scroll;
            } break;
            case UI_ATTR_HOTBAR_SLOT:; {
                int slot = va_arg(args, int);
                if (e->type == UI_ELEM_HOTBAR) e->attribs.hotbar.slot = slot;
            } break;
            case UI_ATTR_HOTBAR_ITEMS:; {
                char* items = va_arg(args, char*);
                if (e->type == UI_ELEM_HOTBAR) {
                    free(e->attribs.hotbar.items);
                    e->attribs.hotbar.items = strdupn(items);
                }
            } break;
            case UI_ATTR_ITEMGRID_CELL:; {
                int x = va_arg(args, int);
                int y = va_arg(args, int);
                if (e->type == UI_ELEM_ITEMGRID) {
                    e->attribs.itemgrid.cell.x = x;
                    e->attribs.itemgrid.cell.y = y;
                }
            } break;
            case UI_ATTR_ITEMGRID_ITEMS:; {
                char* items = va_arg(args, char*);
                if (e->type == UI_ELEM_ITEMGRID) {
                    free(e->attribs.itemgrid.items);
                    e->attribs.itemgrid.items = strdupn(items);
                }
            } break;
        }
    }
    longbreak:;
    va_end(args);
    e->changed = true;
    layer->recalc = true;
    pthread_mutex_unlock(&layer->lock);
    return UI_ERROR_NONE;
}

static void deleteElemTree(struct ui_layer* layer, struct ui_elem* e) {
    for (int i = 0; i < e->children; ++i) {
        int id = e->childdata[i];
        if (badElem(layer, id)) continue;
        deleteElemTree(layer, &layer->elemdata[id]);
    }
    deleteSingleElem(e);
}

int deleteUIElem(struct ui_layer* layer, int id) {
    pthread_mutex_lock(&layer->lock);
    if (badElem(layer, id)) {
        pthread_mutex_unlock(&layer->lock);
        return UI_ERROR_BADELEM;
    }
    struct ui_elem* e = &layer->elemdata[id];
    if (e->parent >= 0) {
        struct ui_elem* p = &layer->elemdata[e->parent];
        dettachChild(&p->children, &p->childdata, id);
        p->changed = true;
    } else {
        dettachChild(&layer->children, &layer->childdata, id);
    }
    deleteElemTree(layer, &layer->elemdata[id]);
    layer->recalc = true;
    pthread_mutex_unlock(&layer->lock);
    return UI_ERROR_NONE;
}

static inline void genUpdateUp(struct ui_layer* layer, struct ui_elem* e) {
    while (e->parent >= 0) {
        e = &layer->elemdata[e->parent];
        e->update = true;
    }
}

static void genUpdateDown(struct ui_layer* layer, struct ui_elem* e) {
    e->update = true;
    for (int i = 0; i < e->children; ++i) {
        int child = e->childdata[i];
        if (child >= 0) genUpdateDown(layer, &layer->elemdata[child]);
    }
}

static void genUpdate(struct ui_layer* layer, struct ui_elem* e) {
    genUpdateUp(layer, e);
    genUpdateDown(layer, e);
}

static inline float getSize(char* propval, float max) {
    float num = 0;
    char suff = 0;
    char op = 0;
    float opnum = 0;
    sscanf(propval, "%f%c%c%f", &num, &suff, &op, &opnum);
    if (suff == '%') {
        num = max * num * 0.01;
    }
    if (op == '+') {
        num += opnum;
    } else if (op == '-') {
        num -= opnum;
    }
    return num;
}

static inline void calcElem(struct ui_layer* layer, struct ui_elem* e) {
    struct ui_calcattribs* c = &e->calcattribs;
    struct ui_calcattribs _p;
    struct ui_calcattribs* p;
    if (e->parent >= 0) {
        p = &layer->elemdata[e->parent].calcattribs;
    } else {
        _p.x = 0;
        _p.y = 0;
        _p.width = layer->width;
        _p.y = layer->width;
        p = &_p;
    }
}

static void calcRecursive(struct ui_layer* layer, struct ui_elem* e) {
    if (e->changed) {
        genUpdate(layer, e);
        e->changed = false;
    }
    calcElem(layer, e);
    for (int i = 0; i < e->children; ++i) {
        int child = e->childdata[i];
        if (child >= 0) calcRecursive(layer, &layer->elemdata[child]);
    }
}

void _calcUI(struct ui_layer* layer) {
    bool reschanged = false;
    if (layer->width != (int)rendinf.width || layer->height != (int)rendinf.height) {
        layer->recalc = true;
        reschanged = true;
        layer->width = rendinf.width;
        layer->height = rendinf.height;
    }
    if (!layer->recalc) return;
    for (int i = 0; i < layer->children; ++i) {
        int child = layer->childdata[i];
        if (child >= 0) {
            struct ui_elem* e = &layer->elemdata[child];
            if (reschanged) genUpdateDown(layer, e);
            calcRecursive(layer, e);
        }
    }
    layer->recalc = false;
    layer->remesh = true;
}

int doUIEvents(struct ui_layer* layer, struct input_info* input) {
    pthread_mutex_lock(&layer->lock);
    _calcUI(layer);
    for (int i = 0; i < layer->elems; ++i) {
        struct ui_elem* e = &layer->elemdata[i];
        if (e->update) {
            if (e->attribs.callback) {
                struct ui_event event;
                memset(&event, 0, sizeof(event));
                event.event = UI_EVENT_UPDATE;
                event.layer = layer;
                event.elemid = i;
                event.elem = e;
                e->attribs.callback(&event);
            }
            e->update = false;
        }
    }
    pthread_mutex_unlock(&layer->lock);
    return false;
}

#endif
