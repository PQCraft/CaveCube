#ifndef SERVER

#ifndef RENDERER_GLAD_PLATFORM_H
#define RENDERER_GLAD_PLATFORM_H

#ifndef _WIN32
    #include "glad/glx/glad_glx.h"
#else
    #include "glad/wgl/glad_wgl.h"
#endif

#endif

#endif
