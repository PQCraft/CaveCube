#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

enum {
    INPUT_MODE_GUI,
    INPUT_MODE_GAME,
};

enum {
    INPUT_ACTION_MULTI__NONE,
    INPUT_ACTION_MULTI_PLACE,
    INPUT_ACTION_MULTI_DESTROY,
    INPUT_ACTION_MULTI_PICK,
    INPUT_ACTION_MULTI_JUMP,
    INPUT_ACTION_MULTI_CROUCH,
    INPUT_ACTION_MULTI_RUN,
    INPUT_ACTION_MULTI_PLAYERLIST,
    INPUT_ACTION_MULTI__MAX,
};

#define INPUT_GETMAFLAG(x) (1 << ((x) - 1))

enum {
    INPUT_ACTION_SINGLE__NONE,
    INPUT_ACTION_SINGLE_ESC,
    INPUT_ACTION_SINGLE_LCLICK,
    INPUT_ACTION_SINGLE_RCLICK,
    INPUT_ACTION_SINGLE_INV,
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
    INPUT_ACTION_SINGLE__MAX,
};

typedef struct {
    uint32_t multi_actions;
    uint16_t single_action;
    uint16_t modifiers;
    bool movti;
    float zmov;
    float xmov;
    bool mmovti;
    float mxmov;
    float mymov;
} input_info;

void setInputMode(int);
bool initInput(void);
input_info getInput(void);

#define INPUT_EMPTY_INFO (input_info){INPUT_ACTION_MULTI__NONE, INPUT_ACTION_SINGLE__NONE, 0, false, 0.0, 0.0, false, 0.0, 0.0}

#endif