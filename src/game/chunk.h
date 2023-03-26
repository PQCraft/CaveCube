#ifndef GAME_CHUNK_H
#define GAME_CHUNK_H

#include <renderer/renderer.h>

#include <inttypes.h>

struct __attribute__((packed)) blockdata {
    uint8_t data[6];
};
// <8 bits: id>
// <2 bits: 0><6 bits: subid>
// <2 bits: 0><2 bits: X rotation><2 bits: Y rotation><2 bits: Z rotation>
// <4 bits: charge><1 bit: nat light top bit><1 bit: light r top bit><1 bit: light g top bit><1 bit: light b top bit>
// <4 bits: nat light bottom bits><4 bits: light r bottom bits>
// <4 bits: light g bottom bits><4 bits: light b bottom bits>

static force_inline uint8_t bdgetid(struct blockdata b) {
    return b.data[0];
}
static force_inline uint8_t bdgetsubid(struct blockdata b) {
    //return d->data[1] & 0b00111111;
    return b.data[1];
}
static force_inline uint8_t bdgetrotx(struct blockdata b) {
    return (b.data[2] & 0b00110000) >> 4;
}
static force_inline uint8_t bdgetroty(struct blockdata b) {
    return (b.data[2] & 0b00001100) >> 2;
}
static force_inline uint8_t bdgetrotz(struct blockdata b) {
    return b.data[2] & 0b00000011;
}
static force_inline uint8_t bdgetcharge(struct blockdata b) {
    return (b.data[3] & 0b11110000) >> 4;
}
static force_inline uint8_t bdgetlightn(struct blockdata b) {
    return ((b.data[3] & 0b00001000) << 1) | ((b.data[4] >> 4) & 0b00001111);
}
static force_inline uint8_t bdgetlightr(struct blockdata b) {
    return ((b.data[3] & 0b00000100) << 2) | (b.data[4] & 0b00001111);
}
static force_inline uint8_t bdgetlightg(struct blockdata b) {
    return ((b.data[3] & 0b00000010) << 1) | ((b.data[5] >> 4) & 0b00001111);
}
static force_inline uint8_t bdgetlightb(struct blockdata b) {
    return ((b.data[3] & 0b00000001) << 2) | (b.data[5] & 0b00001111);
}

static force_inline void bdsetid(struct blockdata* b, uint8_t d) {
    b->data[0] = d;
}
static force_inline void bdsetsubid(struct blockdata* b, uint8_t d) {
    //b->data[1] = d & 0b00111111;
    b->data[1] = d;
}
static force_inline void bdsetrotx(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0b11001111;
    //b->data[2] |= (d & 0b00000011) << 4;
    b->data[2] |= d << 4;
}
static force_inline void bdsetroty(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0b11110011;
    //b->data[2] |= (d & 0b00000011) << 2;
    b->data[2] |= d << 2;
}
static force_inline void bdsetrotz(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0b11111100;
    //b->data[2] |= d & 0b00000011;
    b->data[2] |= d;
}
static force_inline void bdsetcharge(struct blockdata* b, uint8_t d) {
    b->data[3] &= 0b00001111;
    //b->data[3] |= (d & 0b00001111) << 4;
    b->data[3] |= d << 4;
}
static force_inline void bdsetlightn(struct blockdata* b, uint8_t d) {
    b->data[3] &= 0b11110111;
    b->data[3] |= (d & 0b00010000) >> 1;
    b->data[4] &= 0b00001111;
    b->data[4] |= (d & 0b00001111) << 4;
}
static force_inline void bdsetlightr(struct blockdata* b, uint8_t d) {
    b->data[3] &= 0b11111011;
    b->data[3] |= (d & 0b00010000) >> 2;
    b->data[4] &= 0b11110000;
    b->data[4] |= d & 0b00001111;
}
static force_inline void bdsetlightg(struct blockdata* b, uint8_t d) {
    b->data[3] &= 0b11111101;
    b->data[3] |= (d & 0b00010000) >> 3;
    b->data[5] &= 0b00001111;
    b->data[5] |= (d & 0b00001111) << 4;
}
static force_inline void bdsetlightb(struct blockdata* b, uint8_t d) {
    b->data[3] &= 0b11111110;
    b->data[3] |= (d & 0b00010000) >> 4;
    b->data[5] &= 0b11110000;
    b->data[5] |= d & 0b00001111;
}
static force_inline void bdsetlightrgb(struct blockdata* b, uint8_t dr, uint8_t dg, uint8_t db) {
    bdsetlightr(b, dr);
    bdsetlightg(b, dg);
    bdsetlightb(b, db);
}
static force_inline void bdsetlight(struct blockdata* b, uint8_t dr, uint8_t dg, uint8_t db, uint8_t dn) {
    bdsetlightr(b, dr);
    bdsetlightg(b, dg);
    bdsetlightb(b, db);
    bdsetlightn(b, dn);
}

#if MODULEID == MODULEID_GAME
struct rendorder {
    uint32_t c;
    float dist;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
};

struct chunkdata {
    pthread_mutex_t lock;
    struct chunkinfo info;
    struct rendorder* rordr;
    struct blockdata** data;
    int64_t xoff;
    int64_t zoff;
    struct chunk_renddata* renddata;
};

void getBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata*);
void setBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata);
void getChunkOfBlock(int64_t, int64_t, int64_t*, int64_t*);
struct chunkdata* allocChunks(int);
void resizeChunks(struct chunkdata*, int);
void freeChunks(struct chunkdata*);
void moveChunks(struct chunkdata*, int, int);
#endif

#endif
