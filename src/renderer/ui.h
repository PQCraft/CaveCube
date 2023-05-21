#if MODULEID == MODULEID_GAME

#ifndef RENDERER_UI_H
#define RENDERER_UI_H

#include <input/input.h>

#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>

#define isUIElemValid(l, i) ((i) >= 0 && (i) < (l)->elems)

enum {
    UI_ERROR_NONE,
    UI_ERROR_BADELEM,
};

enum {
    UI_END = -1,
};

enum {
    UI_ELEM_CONTAINER,
    UI_ELEM_BOX,
    UI_ELEM_BUTTON,
    UI_ELEM_CHECKBOX,
    UI_ELEM_RADIO,
    UI_ELEM_SLIDER,
    UI_ELEM_TOGGLE,
    UI_ELEM_TEXTBOX,
    UI_ELEM_PROGRESSBAR,
    UI_ELEM_SCROLLBAR,
    UI_ELEM_HOTBAR,
    UI_ELEM_ITEMGRID,
};

enum {
    UI_STATE_NORMAL,
    UI_STATE_HOVERED,
    UI_STATE_PRESSED,
};

enum {
    UI_BORDER_NONE,
    UI_BORDER_BLACK,
    UI_BORDER_WHITE,
    UI_BORDER_GREY,
    UI_BORDER_RAISED,
    UI_BORDER_SUNKEN,
    UI_BORDER_DOUBLERAISED,
    UI_BORDER_DOUBLESUNKEN,
};

enum {
    UI_EVENT_NONE,
    UI_EVENT_UPDATE,
    UI_EVENT_ENTER,
    UI_EVENT_LEAVE,
    UI_EVENT_HOVER,
    UI_EVENT_PRESS,
    UI_EVENT_RELEASE,
    UI_EVENT_CLICK,
};

enum {
    UI_ATTR_NAME,
    UI_ATTR_CALLBACK,
    UI_ATTR_HIDDEN,
    UI_ATTR_DISABLED,
    UI_ATTR_ANCHOR,
    UI_ATTR_SIZE,
    UI_ATTR_MINSIZE,
    UI_ATTR_MAXSIZE,
    UI_ATTR_Z,
    UI_ATTR_ALIGN,
    UI_ATTR_MARGIN,
    UI_ATTR_PADDING,
    UI_ATTR_BORDER,
    UI_ATTR_OFFSET,
    UI_ATTR_COLOR,
    UI_ATTR_ALPHA,
    UI_ATTR_TEXT,
    UI_ATTR_TEXTALIGN,
    UI_ATTR_TEXTOFFSET,
    UI_ATTR_TEXTSCALE,
    UI_ATTR_TEXTCOLOR,
    UI_ATTR_TEXTALPHA,
    UI_ATTR_TEXTATTRIB,
    UI_ATTR_RICHTEXT,
    UI_ATTR_FANCYTEXT,
    UI_ATTR_TOOLTIP,
    UI_ATTR_SLIDER_AMOUNT,
    UI_ATTR_TOGGLE_STATE,
    UI_ATTR_TEXTBOX_EDITABLE,
    UI_ATTR_TEXTBOX_SHADOWTEXT,
    UI_ATTR_PROGRESSBAR_PROGRESS,
    UI_ATTR_SCROLLBAR_MAXSCROLL,
    UI_ATTR_SCROLLBAR_SCROLL,
    UI_ATTR_HOTBAR_SLOT,
    UI_ATTR_HOTBAR_ITEMS,
    UI_ATTR_ITEMGRID_CELL,
    UI_ATTR_ITEMGRID_ITEMS,
};

struct ui_event;
typedef void (*ui_callback_t)(struct ui_event* /*event*/);

struct ui_attribs {
    char* name;
    int state;
    int anchor;
    ui_callback_t callback;
    struct {char* width; char* height;} size;
    struct {char* width; char* height;} minsize;
    struct {char* width; char* height;} maxsize;
    float z;
    struct {int8_t x; int8_t y;} align;
    struct {char* top; char* bottom; char* left; char* right;} margin;
    struct {char* top; char* bottom; char* left; char* right;} padding;
    int border;
    struct {char* x; char* y;} offset;
    struct {uint8_t r; uint8_t g; uint8_t b;} color;
    uint8_t alpha;
    bool hidden;
    bool disabled;
    char* text;
    struct {int8_t x; int8_t y;} textalign;
    struct {char* x; char* y;} textoffset;
    float textscale;
    struct {uint8_t fg : 4; uint8_t bg : 4;} textcolor;
    struct {uint8_t fg; uint8_t bg;} textalpha;
    struct {bool b : 1; bool u : 1; bool i : 1; bool s : 1;} textattrib;
    bool richtext;
    bool fancytext;
    char* tooltip;
    union {
        struct {
            float amount;
        } slider;
        struct {
            bool state;
        } toggle;
        struct {
            bool editable;
            char* shadowtext;
        } textbox;
        struct {
            float progress;
        } progressbar;
        struct {
            float maxscroll;
            float scroll;
        } scrollbar;
        struct {
            int8_t slot;
            char* items;
        } hotbar;
        struct {
            struct {int16_t x; int16_t y;} cell;
            char* items;
        } itemgrid;
    };
};

struct ui_textsect {
    uint8_t fgc : 4;
    uint8_t bgc : 4;
    uint8_t fga;
    uint8_t bga;
    uint8_t attribs;
    int start;
    int chars;
};

struct ui_textline {
    int start;
    int sects;
    struct ui_textsect* sectdata;
};

struct ui_text {
    int8_t alignx;
    int8_t aligny;
    float width;
    float height;
    float scale;
    char* str;
    int lines;
    struct ui_textline* linedata;
};

struct ui_calcattribs {
    float x;
    float y;
    float width;
    float height;
    float totalwidth;
    float totalheight;
    float visx0;
    float visy0;
    float visx1;
    float visy1;
    float viswidth;
    float visheight;
    struct ui_text* text;
};

struct ui_elem {
    int type;
    int parent;
    struct ui_attribs attribs;
    struct ui_calcattribs calcattribs;
    int children;
    int* childdata;
    bool changed;
    bool update;
};

struct ui_layer {
    pthread_mutex_t lock;
    char* name;
    bool hidden;
    float scale;
    float oldscale;
    int width;
    int height;
    int elems;
    struct ui_elem* elemdata;
    int children;
    int* childdata;
    bool recalc;
    bool remesh;
};

struct ui_event {
    int event;
    struct ui_layer* layer;
    int elemid;
    struct ui_elem* elem;
    union {
        struct {
            float x;
            float y;
        } hover;
        struct {
            float x;
            float y;
            int button;
        } press;
        struct {
            float x;
            float y;
            int button;
        } release;
        struct {
            float x;
            float y;
            int button;
        } click;
    };
};

struct ui_doeventinfo {
    bool init;
    bool tookfocus;
};

struct ui_layer* allocUI(char* /*name*/);
void freeUI(struct ui_layer* /*layer*/);
int newUIElem(struct ui_layer* /*layer*/, int /*type*/, int /*parent*/, ... /*attribs*/);
int editUIElem(struct ui_layer* /*layer*/, int /*id*/, ... /*attribs*/);
int deleteUIElem(struct ui_layer* /*layer*/, int /*id*/);
int doUIEvents(struct ui_layer* /*layer*/, struct input_info* /*input*/, struct ui_doeventinfo* /*info*/);
bool isUIElemInvalid(struct ui_layer* /*layer*/, int /*id*/);

void _calcUI(struct ui_layer* /*layer*/);

#endif

#endif
