#if defined(MODULE_GAME) || defined(MODULE_SERVER)

#include <main/main.h>
#include "collision.h"

#include <common/glue.h>

struct _phys_box {
    float maxx;
    float maxy;
    float maxz;
    float minx;
    float miny;
    float minz;
    float density;
};

#endif
