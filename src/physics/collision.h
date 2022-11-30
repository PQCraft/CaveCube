#ifndef PHYSICS_COLLISION_H
#define PHYSICS_COLLISION_H

struct phys_point {
    float x;
    float y;
    float z;
};

struct phys_ray {
    float x;
    float y;
    float z;
    float rotx;
    float roty;
    float rotz;
    float dist;
};

struct phys_box {
    float x;
    float y;
    float z;
    float sizex;
    float sizey;
    float sizez;
    float density;
};

void phys_box_collide_boxes(struct phys_box* /*box*/, struct phys_box* /*new_state*/, struct phys_box** /*colliders*/, int /*colider_count*/, float /*step*/);
struct phys_point phys_ray_collide_boxes(struct phys_ray* /*ray*/ , struct phys_box** /*colliders*/, int /*colider_count*/, float /*step*/);

#endif
