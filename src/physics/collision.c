#include <main/main.h>
#include "collision.h"

struct _phys_box {
    float maxx;
    float maxy;
    float maxz;
    float minx;
    float miny;
    float minz;
    float density;
};
