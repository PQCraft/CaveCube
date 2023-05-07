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
#include <ctype.h>

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
    e->attribs.fancytext = true;
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
            case UI_ATTR_FANCYTEXT:; {
                e->attribs.fancytext = va_arg(args, int);
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
            case UI_ATTR_FANCYTEXT:; {
                e->attribs.fancytext = va_arg(args, int);
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

static inline void newSect(struct ui_textline* line, struct ui_textsect* dval) {
    int i = line->sects++;
    line->sectdata = realloc(line->sectdata, line->sects * sizeof(*line->sectdata));
    memcpy(&line->sectdata[i], dval, sizeof(*line->sectdata));
}

static inline void newLine(struct ui_text* text, struct ui_textsect* dval) {
    int i = text->lines++;
    text->linedata = realloc(text->linedata, text->lines * sizeof(*text->linedata));
    text->linedata[i].start = 0;
    text->linedata[i].sects = 0;
    text->linedata[i].sectdata = NULL;
    newSect(&text->linedata[i], dval);
}

static inline struct ui_text* allocText(struct ui_textsect* dval) {
    struct ui_text* text = calloc(1, sizeof(*text));
    newLine(text, dval);
    return text;
}

static inline void freeText(struct ui_text* text) {
    for (int i = 0; i < text->lines; ++i) {
        free(text->linedata[i].sectdata);
    }
    free(text->linedata);
    free(text->str);
    free(text);
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
    struct ui_elem* p_elem;
    if (e->parent >= 0) {
        p_elem = &layer->elemdata[e->parent];
        p = &p_elem->calcattribs;
    } else {
        p_elem = NULL;
        _p.x = 0;
        _p.y = 0;
        _p.width = layer->width;
        _p.height = layer->height;
        p = &_p;
    }
    float width = getSize(e->attribs.size.width, p->width);
    float height = getSize(e->attribs.size.height, p->height);
    float minwidth = (*e->attribs.minsize.width) ? getSize(e->attribs.minsize.width, p->width) : width;
    float minheight = (*e->attribs.minsize.height) ? getSize(e->attribs.minsize.height, p->height) : height;
    float maxwidth = (*e->attribs.maxsize.width) ? getSize(e->attribs.maxsize.width, p->width) : width;
    float maxheight = (*e->attribs.maxsize.height) ? getSize(e->attribs.maxsize.height, p->height) : height;
    char* text = e->attribs.text;
    if (c->text) {
        freeText(c->text);
        c->text = NULL;
    }
    if (text) {
        bool richtext = e->attribs.richtext;
        bool fancytext = e->attribs.fancytext;
        float scale = e->attribs.textscale;
        float charw = 8.0, charh = 16.0;
        float tmpw = 0.0;
        struct ui_textsect ds; // default section
        struct ui_textsect cs; // current section
        memset(&ds, 0, sizeof(ds));
        memset(&cs, 0, sizeof(cs));
        ds.fgc = e->attribs.textcolor.fg;
        ds.bgc = e->attribs.textcolor.bg;
        ds.fga = e->attribs.textalpha.fg;
        ds.bga = e->attribs.textalpha.bg;
        ds.attribs = e->attribs.textattrib.b | (e->attribs.textattrib.i << 1);
        ds.attribs |= (e->attribs.textattrib.u << 2) | (e->attribs.textattrib.s << 3);
        memcpy(&cs, &ds, sizeof(cs)); // copy defaults into current
        c->text = allocText(&cs);
        struct ui_text* t = c->text;
        t->alignx = e->attribs.align.x;
        t->aligny = e->attribs.align.y;
        t->scale = scale;
        t->str = strdup(text);
        int line = 0;
        int sect = 0;
        int pos = 0;
        struct ui_textline* l = &t->linedata[0];
        struct ui_textsect* s = &t->linedata[0].sectdata[0];
        #if DBGLVL(2)
        printf("MAKING TEXT: maxwidth=%g\n", maxwidth);
        #endif
        bool sepchar = false;
        while (1) {
            char tmpc = text[pos];
            if (!tmpc) {
                s->chars = pos - s->start;
                goto done;
            }
            if (richtext) {
                if (tmpc == '\e') {
                    int newpos = pos;
                    char tmpc2 = text[++newpos];
                    switch (tmpc2) {
                        case '\e':; {
                            pos = newpos;
                            goto badseq;
                        } break;
                        case 'F':; {
                            tmpc2 = text[++newpos];
                            switch (tmpc2) {
                                case 'C':; {
                                    tmpc2 = text[++newpos];
                                    uint8_t fgc;
                                    if (isxdigit(tmpc2)) {
                                        cs.fgc = (tmpc2 & 15) + (tmpc2 >> 6) * 9;
                                    } else {
                                        goto badseq;
                                    }
                                    pos = newpos++;
                                    // create new sect with new fgc
                                    s->chars = pos - s->start;
                                    newSect(&t->linedata[line], &cs);
                                    s = &t->linedata[line].sectdata[++sect];
                                    s->start = newpos;
                                    goto goodseq;
                                } break;
                                case 'A':; {
                                    tmpc2 = text[++newpos];
                                    uint8_t fga;
                                    if (isxdigit(tmpc2)) {
                                        fga = (tmpc2 & 15) + (tmpc2 >> 6) * 9;
                                        fga <<= 4;
                                        tmpc2 = text[++newpos];
                                        if (isxdigit(tmpc2)) {
                                            cs.fga = fga | ((tmpc2 & 15) + (tmpc2 >> 6) * 9);
                                        } else {
                                            goto badseq;
                                        }
                                    } else {
                                        goto badseq;
                                    }
                                    pos = newpos++;
                                    // create new sect with new fga
                                    s->chars = pos - s->start;
                                    newSect(&t->linedata[line], &cs);
                                    s = &t->linedata[line].sectdata[++sect];
                                    s->start = newpos;
                                    goto goodseq;
                                } break;
                                default:; {
                                    goto badseq;
                                }
                            }
                        } break;
                        case 'B':; {
                            tmpc2 = text[++newpos];
                            switch (tmpc2) {
                                case 'C':; {
                                    tmpc2 = text[++newpos];
                                    uint8_t bgc;
                                    if (isxdigit(tmpc2)) {
                                        cs.bgc = (tmpc2 & 15) + (tmpc2 >> 6) * 9;
                                    } else {
                                        goto badseq;
                                    }
                                    pos = newpos++;
                                    // create new sect with new bgc
                                    s->chars = pos - s->start;
                                    newSect(&t->linedata[line], &cs);
                                    s = &t->linedata[line].sectdata[++sect];
                                    s->start = newpos;
                                    goto goodseq;
                                } break;
                                case 'A':; {
                                    tmpc2 = text[++newpos];
                                    uint8_t bga;
                                    if (isxdigit(tmpc2)) {
                                        bga = (tmpc2 & 15) + (tmpc2 >> 6) * 9;
                                        bga <<= 4;
                                        tmpc2 = text[++newpos];
                                        if (isxdigit(tmpc2)) {
                                            cs.bga = bga | ((tmpc2 & 15) + (tmpc2 >> 6) * 9);
                                        } else {
                                            goto badseq;
                                        }
                                    } else {
                                        goto badseq;
                                    }
                                    pos = newpos++;
                                    // create new sect with new bga
                                    s->chars = pos - s->start;
                                    newSect(&t->linedata[line], &cs);
                                    s = &t->linedata[line].sectdata[++sect];
                                    s->start = newpos;
                                    goto goodseq;
                                } break;
                                default:; {
                                    goto badseq;
                                }
                            }
                        } break;
                        case 'A':; {
                            tmpc2 = text[++newpos];
                            if (isxdigit(tmpc2)) {
                                cs.attribs = (tmpc2 & 15) + (tmpc2 >> 6) * 9;
                            } else {
                                goto badseq;
                            }
                            pos = newpos++;
                            // create new sect with new attribs
                            s->chars = pos - s->start;
                            newSect(&t->linedata[line], &cs);
                            s = &t->linedata[line].sectdata[++sect];
                            s->start = newpos;
                            goto goodseq;
                        }
                        case 'R':; {
                            pos = newpos++;
                            // create new sect and restore all vars to default
                            memcpy(&cs, &ds, sizeof(cs));
                            s->chars = pos - s->start;
                            newSect(&t->linedata[line], &cs);
                            s = &t->linedata[line].sectdata[++sect];
                            s->start = newpos;
                            goto goodseq;
                        }
                        default:; {
                            goto badseq;
                        }
                    }
                }
            }
            badseq:;
            {
                float tmpw2 = tmpw + charw;
                if (fancytext) {
                    if (tmpc == ' ' || sepchar) {
                        int newpos = pos + 1;
                        while (1) {
                            char tmpc2 = text[newpos];
                            if (!tmpc2 || tmpc2 == ' ' || tmpc2 == '\n') break;
                            tmpw2 += charw;
                            ++newpos;
                        }
                    }
                    if (sepchar) sepchar = false;
                    if (tmpc == '-') sepchar = true;
                }
                bool nextline = false;
                bool trimchar = false;
                if (tmpc == '\n') {
                    nextline = true;
                    trimchar = true;
                    #if DBGLVL(2)
                    puts("NL");
                    #endif
                } else if (pos > l->start && tmpw2 * scale > maxwidth) {
                    nextline = true;
                    #if DBGLVL(2)
                    printf("next line: %02X (%c)\n", tmpc, tmpc);
                    #endif
                    // if tmpc == ' ' and fancytext then remove from end of line
                    if (fancytext) {
                        if (tmpc == ' ') trimchar = true;
                    }
                }
                if (nextline) {
                    // finalize current line and create new
                    s->chars = pos - s->start;
                    newLine(t, &cs);
                    l = &t->linedata[++line];
                    s = &t->linedata[line].sectdata[(sect = 0)];
                    l->start = s->start = pos + (trimchar);
                    tmpw = 0.0;
                    sepchar = false;
                } else {
                    if (!trimchar) tmpw += charw;
                }
            }
            goodseq:;
            ++pos;
        }
        done:;
        #if DBGLVL(2)
        printf("MADE TEXT: %d lines\n", t->lines);
        for (int i = 0; i < t->lines; ++i) {
            struct ui_textline* l = &t->linedata[i];
            printf("    Line %d (%d sects): start=%d\n", i, l->sects, l->start);
            for (int j = 0; j < l->sects; ++j) {
                struct ui_textsect* s = &l->sectdata[j];
                printf("        Sect %d: start=%d, chars=%d, end=%d, fgc=%d, bgc=%d, fga=%d, bga=%d, attribs=%01X\n",
                    j, s->start, s->chars, s->start + s->chars, s->fgc, s->bgc, s->fga, s->bga, s->attribs);
                printf("            text=\"%.*s\"\n", s->chars, &t->str[s->start]);
            }
        }
        #endif
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

static void deleteSingleElem(struct ui_elem* e) {
    printf("deleteSingleElem: type=%d\n", e->type);
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
    if (e->calcattribs.text) {
        freeText(e->calcattribs.text);
    }
    e->type = -1;
    free(e->childdata);
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

void freeUI(struct ui_layer* layer) {
    for (int i = 0; i < layer->elems; ++i) {
        if (badElem(layer, i)) continue;
        deleteSingleElem(&layer->elemdata[i]);
    }
    free(layer->elemdata);
    free(layer->childdata);
    free(layer->name);
    pthread_mutex_destroy(&layer->lock);
}

#endif
