#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "ui.h"

#include <pthread.h>
#include <stdlib.h>
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
    layer->elems = 0;
    layer->elemdata = NULL;
    layer->children = 0;
    layer->childdata = NULL;
    return layer;
}

static void deleteSingleElem(struct ui_elem* e) {
    free(e->attribs.name);
    free(e->attribs.size.width);
    free(e->attribs.size.height);
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

static inline char* strdupn(char* str) {
    if (!str) return NULL;
    return strdup(str);
}

int newUIElem(struct ui_layer* layer, int type, int parent, ...) {
    pthread_mutex_lock(&layer->lock);
    if (badElem(layer, parent)) {
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
    memset(&e->attribs, 0, sizeof(e->attribs));
    memset(&e->calcattribs, 0, sizeof(e->calcattribs));
    e->children = 0;
    e->childdata = NULL;
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
            e->attribs.hotbar.cell.x = -1;
            e->attribs.hotbar.cell.y = -1;
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
            case UI_ATTR_STATE:; {
                e->attribs.state = va_arg(args, int);
            } break;
            case UI_ATTR_ANCHOR:; {
                e->attribs.anchor = va_arg(args, int);
            } break;
            case UI_ATTR_CALLBACK:; {
                e->attribs.callback = va_arg(args, ui_callback_t);
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
            case UI_ATTR_HIDDEN:; {
                e->attribs.hidden = va_arg(args, int);
            } break;
            case UI_ATTR_DISABLED:; {
                e->attribs.disabled = va_arg(args, int);
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
                e->attribs.textcolor.fg = va_arg(args, int);
                e->attribs.textcolor.bg = va_arg(args, int);
            } break;
            case UI_ATTR_TEXTALPHA:; {
                e->attribs.textalpha.fg = va_arg(args, int);
                e->attribs.textalpha.bg = va_arg(args, int);
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
                char* shadowtext = strdupn(va_arg(args, char*));
                if (type == UI_ELEM_TEXTBOX) e->attribs.textbox.shadowtext = shadowtext;
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
                if (type == UI_ELEM_HOTBAR) e->attribs.hotbar.items = items;
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
                if (type == UI_ELEM_ITEMGRID) e->attribs.itemgrid.items = items;
            } break;
        }
    }
    longbreak:;
    va_end(args);
    if (!e->attribs.size.width) strdup(e->attribs.size.width = "1t");
    if (!e->attribs.size.height) strdup(e->attribs.size.height = "1t");
    if (!e->attribs.margin.top) strdup(e->attribs.margin.top = "0");
    if (!e->attribs.margin.bottom) strdup(e->attribs.margin.bottom = "0");
    if (!e->attribs.margin.left) strdup(e->attribs.margin.left = "0");
    if (!e->attribs.margin.right) strdup(e->attribs.margin.right = "0");
    switch (type) {
        case UI_ELEM_FANCYBOX:;
            if (!e->attribs.padding.top) strdup(e->attribs.padding.top = "8");
            if (!e->attribs.padding.bottom) strdup(e->attribs.padding.bottom = "8");
            if (!e->attribs.padding.left) strdup(e->attribs.padding.left = "8");
            if (!e->attribs.padding.right) strdup(e->attribs.padding.right = "8");
            break;
        case UI_ELEM_BUTTON:;
            if (!e->attribs.padding.top) strdup(e->attribs.padding.top = "1");
            if (!e->attribs.padding.bottom) strdup(e->attribs.padding.bottom = "1");
            if (!e->attribs.padding.left) strdup(e->attribs.padding.left = "4");
            if (!e->attribs.padding.right) strdup(e->attribs.padding.right = "4");
            break;
        case UI_ELEM_HOTBAR:;
            if (!e->attribs.hotbar.items) e->attribs.hotbar.items = strdup("");
            break;
        case UI_ELEM_ITEMGRID:;
            if (!e->attribs.itemgrid.items) e->attribs.itemgrid.items = strdup("");
            break;
        default:;
            if (!e->attribs.padding.top) strdup(e->attribs.padding.top = "0");
            if (!e->attribs.padding.bottom) strdup(e->attribs.padding.bottom = "0");
            if (!e->attribs.padding.left) strdup(e->attribs.padding.left = "0");
            if (!e->attribs.padding.right) strdup(e->attribs.padding.right = "0");
            break;
    }
    pthread_mutex_unlock(&layer->lock);
    return elemid;
}

int editUIElem(struct ui_layer* layer, int id, ...) {
    pthread_mutex_lock(&layer->lock);
    if (badElem(layer, id)) {
        pthread_mutex_unlock(&layer->lock);
        return UI_ERROR_BADELEM;
    }
    
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
    deleteElemTree(layer, &layer->elemdata[id]);
    pthread_mutex_unlock(&layer->lock);
    return UI_ERROR_NONE;
}

int doUIEvents(struct ui_layer* layer, struct input_info* input) {
    
}

#endif
