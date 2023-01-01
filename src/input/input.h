#if MODULEID == MODULEID_GAME

#ifndef INPUT_INPUT_H
#define INPUT_INPUT_H

#include <stdbool.h>
#include <inttypes.h>

enum {
    INPUT_MODE_UI,
    INPUT_MODE_GAME,
    INPUT_MODE_TEXTINPUT,
};

enum {
    INPUT_ACTION_MULTI__NONE,
    INPUT_ACTION_MULTI_PLACE,
    INPUT_ACTION_MULTI_DESTROY,
    INPUT_ACTION_MULTI_PICK,
    INPUT_ACTION_MULTI_ZOOM,
    INPUT_ACTION_MULTI_JUMP,
    INPUT_ACTION_MULTI_CROUCH,
    INPUT_ACTION_MULTI_RUN,
    INPUT_ACTION_MULTI_PLAYERLIST,
    INPUT_ACTION_MULTI_DEBUG,
    INPUT_ACTION_MULTI__MAX,
};

#define INPUT_GETMAFLAG(x) (1 << ((x) - 1))

enum {
    INPUT_ACTION_SINGLE__NONE = -1,
    INPUT_ACTION_SINGLE_ESC,
    INPUT_ACTION_SINGLE_INV,
    INPUT_ACTION_SINGLE_INV_0,
    INPUT_ACTION_SINGLE_INV_1,
    INPUT_ACTION_SINGLE_INV_2,
    INPUT_ACTION_SINGLE_INV_3,
    INPUT_ACTION_SINGLE_INV_4,
    INPUT_ACTION_SINGLE_INV_5,
    INPUT_ACTION_SINGLE_INV_6,
    INPUT_ACTION_SINGLE_INV_7,
    INPUT_ACTION_SINGLE_INV_8,
    INPUT_ACTION_SINGLE_INV_9,
    INPUT_ACTION_SINGLE_INV_NEXT,
    INPUT_ACTION_SINGLE_INV_PREV,
    INPUT_ACTION_SINGLE_INVOFF_NEXT,
    INPUT_ACTION_SINGLE_INVOFF_PREV,
    INPUT_ACTION_SINGLE_ROT_X,
    INPUT_ACTION_SINGLE_ROT_Y,
    INPUT_ACTION_SINGLE_ROT_Z,
    INPUT_ACTION_SINGLE_CHAT,
    INPUT_ACTION_SINGLE_COMMAND,
    INPUT_ACTION_SINGLE_FULLSCR,
    INPUT_ACTION_SINGLE_DEBUG,
    //INPUT_ACTION_SINGLE_LCLICK, // move to INPUT_ACTION_UI
    //INPUT_ACTION_SINGLE_RCLICK, // move to INPUT_ACTION_UI
    INPUT_ACTION_SINGLE__MAX,
};

struct input_info {
    bool focus;
    unsigned multi_actions;
    int single_action;
    float mov_mult;
    float mov_up;
    float mov_right;
    float mov_bal;
    float rot_mult_x;
    float rot_mult_y;
    float rot_up;
    float rot_right;
    int ui_mouse_x;
    int ui_mouse_y;
    int ui_mouse_click;
};

extern int inputMode;

void setInputMode(int);
bool initInput(void);
void resetInput(void);
void getInput(struct input_info*);

#define INPUT_EMPTY_INFO (struct input_info){false, INPUT_ACTION_MULTI__NONE, INPUT_ACTION_SINGLE__NONE, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0}

#endif

#endif
