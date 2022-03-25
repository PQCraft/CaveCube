#include <renderer.h>
#include <common.h>
#include <resource.h>
#include <main.h>

#include <stdbool.h>
#include <string.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

renderer_info rendinf;

float gfx_aspect = 1.0;

void setMat4(GLuint prog, char* name, mat4 val) {
    int uHandle = glGetUniformLocation(prog, name);
    glUniformMatrix4fv(uHandle, 1, GL_FALSE, *val);
}

void setUniform3f(GLuint prog, char* name, float val[3]) {
    int uHandle = glGetUniformLocation(prog, name);
    glUniform3f(uHandle, val[0], val[1], val[2]);
}

void setFullscreen(bool fullscreen) {
    static int winox, winoy = 0;
    if (fullscreen) {
        gfx_aspect = (float)rendinf.full_width / (float)rendinf.full_height;
        rendinf.width = rendinf.full_width;
        rendinf.height = rendinf.full_height;
        rendinf.fps = rendinf.full_fps;
        glfwGetWindowPos(rendinf.window, &winox, &winoy);
        glfwSetWindowMonitor(rendinf.window, rendinf.monitor, 0, 0, rendinf.full_width, rendinf.full_height, rendinf.full_fps);
        rendinf.fullscr = true;
    } else {
        gfx_aspect = (float)rendinf.win_width / (float)rendinf.win_height;
        rendinf.width = rendinf.win_width;
        rendinf.height = rendinf.win_height;
        rendinf.fps = rendinf.win_fps;
        int twinx, twiny;
        if (rendinf.fullscr) {
            uint64_t offset = altutime();
            glfwSetWindowMonitor(rendinf.window, NULL, 0, 0, rendinf.win_width, rendinf.win_height, rendinf.win_fps);
            do {
                glfwSetWindowPos(rendinf.window, winox, winoy);
                glfwGetWindowPos(rendinf.window, &twinx, &twiny);
            } while (altutime() - offset < 3000000 && (twinx != winox || twiny != winoy));
        }
        rendinf.fullscr = false;
    }
    mat4 projection;
    glm_perspective(rendinf.camfov * M_PI / 180, gfx_aspect, 0.075, 2048, projection);
    setMat4(rendinf.shaderprog, "projection", projection);
}

void gfx_winch(GLFWwindow* win, int w, int h) {
    if (win == rendinf.window) glViewport(0, 0, w, h);
}

static inline bool makeShaderProg(char* vstext, char* fstext, GLuint* p) {
    if (!vstext || !fstext) return false;
    GLuint vertexHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexHandle, 1, (const GLchar * const*)&vstext, NULL);
    glCompileShader(vertexHandle);
    GLint ret = GL_FALSE;
    glGetShaderiv(vertexHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(vertexHandle, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetShaderInfoLog(vertexHandle, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Vertex shader compile error: %s\n", (char*)log);
        free(log);
        glDeleteShader(vertexHandle);
        goto retfalse;
    }
    GLuint fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragHandle, 1, (const GLchar * const*)&fstext, NULL);
    glCompileShader(fragHandle);
    glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(fragHandle, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetShaderInfoLog(fragHandle, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Fragment shader compile error: %s\n", (char*)log);
        free(log);
        glDeleteShader(vertexHandle);
        glDeleteShader(fragHandle);
        goto retfalse;
    }
    *p = glCreateProgram();
    glAttachShader(*p, vertexHandle);
    glAttachShader(*p, fragHandle);
    glLinkProgram(*p);
    glGetProgramiv(*p, GL_LINK_STATUS, &ret);
    glDeleteShader(vertexHandle);
    glDeleteShader(fragHandle);
    if (!ret) {
        GLint logSize = 0;
        glGetProgramiv(*p, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetProgramInfoLog(*p, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Shader program link error: %s\n", (char*)log);
        free(log);
        goto retfalse;
    }
    return true;
    retfalse:;
    return false;
}

//extern float vertices[];
//extern uint32_t indices[];

model* loadModel(char* mpath, char* tpath) {
    resdata_bmd* mdata = loadResource(RESOURCE_BMD, mpath);
    resdata_image* tdata = loadResource(RESOURCE_IMAGE, tpath);
    model* m = malloc(sizeof(model));
    memset(m, 0, sizeof(model));
    glGenVertexArrays(1, &m->VAO);
    glGenBuffers(1, &m->VBO);
    glGenBuffers(1, &m->EBO);
    m->pos = (coord_3d){0.0, 0.0, 0.0};
    m->rot = (coord_3d){0.0, 0.0, 0.0};
    m->scale = (coord_3d){1.0, 1.0, 1.0};
    m->mesh = mdata;
    glGenTextures(1, &m->texture);
    glBindTexture(GL_TEXTURE_2D, m->texture);
    GLenum colors = GL_RGB;
    if (tdata->channels == 1) colors = GL_RED;
    else if (tdata->channels == 4) colors = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, colors, tdata->width, tdata->height, 0, colors, GL_UNSIGNED_BYTE, tdata->data);
    /*
    printf("[%d] [%d] [%d]\n", tdata->width, tdata->height, tdata->channels);
    for (int i = 0; i < tdata->width * tdata->height; ++i) {
        for (int j = 0; j < tdata->channels; ++j) {
            printf("[%u] ", (uint8_t)tdata->data[i * tdata->channels + j]);
        }
        putchar('\n');
    }
    */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    //freeResource(tdata);
    //m->mesh->indices = indices;
    //m->mesh->vertices = vertices;
    return m;
}

void updateCam() {
    mat4 view, projection;
    glm_perspective(rendinf.camfov * M_PI / 180.0, gfx_aspect, 0.075, 2048.0, projection);
    setMat4(rendinf.shaderprog, "projection", projection);
    vec3 direction = {cosf((rendinf.camrot.y - 90.0) * M_PI / 180.0) * cosf(rendinf.camrot.x * M_PI / 180.0),
        sin(rendinf.camrot.x * M_PI / 180.0),
        sinf((rendinf.camrot.y - 90.0) * M_PI / 180.0) * cosf(rendinf.camrot.x * M_PI / 180.0)};
    direction[0] += rendinf.campos.x;
    direction[1] += rendinf.campos.y;
    direction[2] += rendinf.campos.z;
    glm_lookat((vec3){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z},
        direction, (vec3){0.0, 1.0, 0.0}, view);
    setMat4(rendinf.shaderprog, "view", view);
    /*
    puts("view:");
    for (int i = 0; i < 4; ++i) {
        printf("[%f] [%f] [%f] [%f]\n", view[i][0], view[i][1], view[i][2], view[i][3]);
    }
    */
    //setUniform3f(rendinf.shaderprog, "viewPos", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
}

void renderModel(model* m) {
    glBindVertexArray(m->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m->VBO);
    glBufferData(GL_ARRAY_BUFFER, m->mesh->vsize, m->mesh->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->mesh->isize, m->mesh->indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    mat4 model = GFX_DEFAULT_MAT4;
    glm_translate(model, (vec3){m->pos.x, m->pos.y, m->pos.z});
    glm_rotate(model, m->rot.x * M_PI / 180, (vec3){1, 0, 0});
    glm_rotate(model, m->rot.y * M_PI / 180, (vec3){0, 1, 0});
    glm_rotate(model, m->rot.z * M_PI / 180, (vec3){0, 0, 1});
    glm_scale(model, (vec3){m->scale.x, m->scale.y, m->scale.z});
    setMat4(rendinf.shaderprog, "model", model);
    glBindTexture(GL_TEXTURE_2D, m->texture);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 0);
    glDrawElements(GL_TRIANGLES, m->mesh->isize / sizeof(uint32_t), GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

extern float posmult;
extern float rotmult;
extern float fpsmult;

void testRenderer() {
    model* m1 = loadModel("game/models/block/default.bmd", "game/textures/blocks/stone/0.png");
    model* m2 = loadModel("game/models/block/default.bmd", "game/textures/blocks/dirt/0.png");
    model* m3 = loadModel("game/models/block/default.bmd", "game/textures/blocks/gravel/0.png");
    model* m4 = loadModel("game/models/block/default.bmd", "game/textures/blocks/bedrock/0.png");
    m1->pos = (coord_3d){0.0, 0.5, -2.0};
    m2->pos = (coord_3d){0.0, 0.5, 2.0};
    m3->pos = (coord_3d){-2.0, 1.5, 0.0};
    m4->pos = (coord_3d){2.0, 1.5, 0.0};
    rendinf.campos.y = 1.5;
    initInput();
    float opm = posmult;
    float orm = rotmult;
    while (1) {
        uint64_t starttime = altutime();
        glfwPollEvents();
        testInput();
        updateCam();
        //printf("[%f]\n", rendinf.camrot.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderModel(m1);
        renderModel(m2);
        renderModel(m3);
        renderModel(m4);
        glfwSwapInterval(rendinf.vsync);
        glfwSwapBuffers(rendinf.window);
        fpsmult = (float)(altutime() - starttime) / (1000000.0f / 60.0f);
        posmult = opm * fpsmult;
        rotmult = orm * fpsmult;
    }
}

bool initRenderer() {
    glfwInit();
    rendinf.camfov = 50;
    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;
    sscanf(getConfigVarStatic(config, "renderer.resolution", "1024x768@0", 256), "%ux%u@%u",
        &rendinf.win_width, &rendinf.win_height, &rendinf.win_fps);
    sscanf(getConfigVarStatic(config, "renderer.fullresolution", "1280x720@0", 256), "%ux%u@%u",
        &rendinf.full_width, &rendinf.full_height, &rendinf.full_fps);
    if (!rendinf.win_width || rendinf.win_width > 32767) rendinf.win_width = 640;
    if (!rendinf.win_height || rendinf.win_height > 32767) rendinf.win_height = 480;
    rendinf.vsync = getConfigValBool(getConfigVarStatic(config, "renderer.vsync", "true", 64));
    rendinf.fullscr = getConfigValBool(getConfigVarStatic(config, "renderer.fullscreen", "false", 64));
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (!(rendinf.monitor = glfwGetPrimaryMonitor())) {
        fputs("glfwGetPrimaryMonitor: Failed to fetch primary monitor handle\n", stderr);
        return NULL;
    }
    if (!(rendinf.window = glfwCreateWindow(rendinf.win_width, rendinf.win_height, "CaveCube", NULL, NULL))) {
        fputs("glfwCreateWindow: Failed to create window\n", stderr);
        return NULL;
    }
    glfwMakeContextCurrent(rendinf.window);
    glfwSetInputMode(rendinf.window, GLFW_STICKY_KEYS, GLFW_TRUE);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fputs("gladLoadGLLoader: Failed to initialize GLAD\n", stderr);
        return NULL;
    }
    glfwSetFramebufferSizeCallback(rendinf.window, gfx_winch);
    filedata* vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/OpenGL/shaders/vertex.glsl");
    filedata* fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/OpenGL/shaders/fragment.glsl");
    if (!makeShaderProg((char*)vs->data, (char*)fs->data, &rendinf.shaderprog)) {
        return NULL;
    }
    glUseProgram(rendinf.shaderprog);
    setFullscreen(rendinf.fullscr);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClearColor(0, 0, 0.2, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(rendinf.window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(rendinf.window);
    updateCam();
    glfwPollEvents();
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "is2D"), 1);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fIs2D"), 1);
    //glDisable(GL_DEPTH_TEST);
    return false;
}
