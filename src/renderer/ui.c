#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "ui.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

void freeUI(struct ui_layer* layer) {
    for (int i = 0; i < layer->elems; ++i) {
        //deleteSingleElem(&layer->elemdata[i]);
    }
    free(layer->elemdata);
    free(layer->childdata);
    free(layer->name);
    pthread_mutex_destroy(&layer->lock);
}

int newUIElem(struct ui_layer* layer, int type, int parent, ...) {
    pthread_mutex_lock(&layer->lock);
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
    e->attribs.anchor = -1;
    memset(&e->calcattribs, 0, sizeof(e->calcattribs));
    e->children = 0;
    e->childdata = NULL;
    pthread_mutex_unlock(&layer->lock);
    return elemid;
}

void editUIElem(struct ui_layer* layer, int id, ...) {
    
}

void deleteUIElem(struct ui_layer* layer, int id) {
    
}

int doUIEvents(struct ui_layer* layer, struct input_info* input) {
    
}

#endif
