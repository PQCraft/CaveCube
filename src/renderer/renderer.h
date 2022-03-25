#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct {
    float x;
    float y;
    float z;
} coord_3d;

typedef struct {
    float r;
    float g;
    float b;
} color;

typedef struct {
    unsigned int win_width;
    unsigned int win_height;
    unsigned int win_fps;
    unsigned int full_width;
    unsigned int full_height;
    unsigned int full_fps;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    bool vsync;
    bool fullscr;
    GLFWmonitor* monitor;
    GLFWwindow* window;
    coord_3d campos;
    coord_3d camrot;
    float camfov;
    GLuint shaderprog;
} renderer_info;

extern renderer_info rendinf;

bool initRenderer();

#endif
