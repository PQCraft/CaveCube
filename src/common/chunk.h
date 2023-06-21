#ifndef COMMON_CHUNK_H
#define COMMON_CHUNK_H

#include <graphics/renderer.h>

#include <inttypes.h>

struct __attribute__((packed)) blockdata {
    uint8_t data[5];
};
// <8 bits: id>
// <2 bits: X rotation><6 bits: subid>
// <2 bits: Y rotation><2 bits: Z rotation><1 bit: nat light top bit><1 bit: light r top bit><1 bit: light g top bit><1 bit: light b top bit>
// <4 bits: nat light bottom bits><4 bits: light r bottom bits>
// <4 bits: light g bottom bits><4 bits: light b bottom bits>

static force_inline uint8_t bdgetid(struct blockdata b) {
    return b.data[0];
}
static force_inline uint8_t bdgetsubid(struct blockdata b) {
    return b.data[1] & 0x3F;
}
static force_inline uint8_t bdgetrotx(struct blockdata b) {
    return (b.data[1] >> 6) & 0x03;
}
static force_inline uint8_t bdgetroty(struct blockdata b) {
    return (b.data[2] >> 6) & 0x03;
}
static force_inline uint8_t bdgetrotz(struct blockdata b) {
    return (b.data[2] >> 4) & 0x03;
}
static force_inline uint8_t bdgetlightn(struct blockdata b) {
    return ((b.data[2] & 0x08) << 1) | ((b.data[3] >> 4) & 0x0F);
}
static force_inline uint8_t bdgetlightr(struct blockdata b) {
    return ((b.data[2] & 0x04) << 2) | (b.data[3] & 0x0F);
}
static force_inline uint8_t bdgetlightg(struct blockdata b) {
    return ((b.data[2] & 0x02) << 3) | ((b.data[4] >> 4) & 0x0F);
}
static force_inline uint8_t bdgetlightb(struct blockdata b) {
    return ((b.data[2] & 0x01) << 4) | (b.data[4] & 0x0F);
}

static force_inline void bdsetid(struct blockdata* b, uint8_t d) {
    b->data[0] = d;
}
static force_inline void bdsetsubid(struct blockdata* b, uint8_t d) {
    b->data[1] &= 0xC0;
    b->data[1] |= d /*& 0x3F*/;
}
static force_inline void bdsetrotx(struct blockdata* b, uint8_t d) {
    b->data[1] &= 0x3F;
    b->data[1] |= (d << 6) /*& 0xC0*/;
}
static force_inline void bdsetroty(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0x3F;
    b->data[2] |= (d << 6) /*& 0xC0*/;
}
static force_inline void bdsetrotz(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0xCF;
    b->data[2] |= (d << 4) /*& 0x30*/;
}
static force_inline void bdsetlightn(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0xF7;
    b->data[2] |= (d & 0x10) >> 1;
    b->data[3] &= 0x0F;
    b->data[3] |= (d & 0x0F) << 4;
}
static force_inline void bdsetlightr(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0xFB;
    b->data[2] |= (d & 0x10) >> 2;
    b->data[3] &= 0xF0;
    b->data[3] |= d & 0x0F;
}
static force_inline void bdsetlightg(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0xFD;
    b->data[2] |= (d & 0x10) >> 3;
    b->data[4] &= 0x0F;
    b->data[4] |= (d & 0x0F) << 4;
}
static force_inline void bdsetlightb(struct blockdata* b, uint8_t d) {
    b->data[2] &= 0xFE;
    b->data[2] |= (d & 0x10) >> 4;
    b->data[4] &= 0xF0;
    b->data[4] |= d & 0x0F;
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

#if defined(MODULE_GAME)
struct rendorder {
    uint32_t c;
    float dist;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
};

struct chunk_metadata {
    int top;        // highest block (-1 for an empty chunk)
    int alignedtop; // highest block aligned to the top of the max section (-1 for 0, 15 for 1, 31 for 2, ...)
    int sects;      // number of 16x16x16 sections allocated (max is 32)
};

struct chunkdata {
    pthread_mutex_t lock;
    struct chunkinfo info;
    struct rendorder* rordr;
    struct blockdata** data;
    int64_t xoff;
    int64_t zoff;
    struct chunk_metadata* metadata;
    struct chunk_renddata* renddata;
};

void getBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata*);
void setBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata);
void getChunkOfBlock(int64_t, int64_t, int64_t*, int64_t*);
struct chunkdata* allocChunks(int);
void resizeChunks(struct chunkdata*, int);
void freeChunks(struct chunkdata*);
void moveChunks(struct chunkdata*, int, int);
void extendChunk(struct chunkdata*, int);
void resizeChunk(struct chunkdata*, int);
void resizeChunkTo(struct chunkdata*, int, int);
int findChunkDataTop(struct blockdata*);
int findChunkTop(struct chunkdata*, int);

extern bool allocexactchunkheight;

#endif

#endif
