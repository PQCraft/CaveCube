#ifndef INPUT_H
#define INPUT_H

enum {
    INPUT_MODE_GUI,
    INPUT_MODE_GAME,
};

enum {
    INPUT_ACTION_NONE = 0,
    INPUT_ACTION_PLACE = 1,
    INPUT_ACTION_DESTROY = 1 << 1,
    INPUT_ACTION_PICK = 1 << 2,
    INPUT_ACTION_JUMP = 1 << 3,
    INPUT_ACTION_CROUCH = 1 << 4,
    INPUT_ACTION_RUN = 1 << 5,
    INPUT_ACTION_UI_ESC = 1 << 16,
};

typedef struct {
    uint32_t actions;
    float zmov;
    float xmov;
    float mxmov;
    float mymov;
} input_info;

void setInputMode(int);
bool initInput(void);
input_info getInput(void);

#define INPUT_EMPTY_INFO (input_info){}

#endif
