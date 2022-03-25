#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <resource.h>

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

typedef struct {
    coord_3d pos;
    coord_3d rot;
    coord_3d scale;
    resdata_bmd* mesh;
    unsigned int VBO, VAO, EBO;
    unsigned int texture;
} model;

extern renderer_info rendinf;

bool initRenderer();

#define GFX_DEFAULT_POS (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_ROT (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_SCALE (coord_3d){1.0, 1.0, 1.0}
#define GFX_DEFAULT_MAT4 {{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}

#endif
