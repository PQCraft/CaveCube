#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>

#include "common.h"

#include "../lzmalib/LzmaLib.h"
#include "../lzmalib/7zTypes.h"

filedata getFile(char* name, char* mode) {
    struct stat fnst;
    memset(&fnst, 0, sizeof(struct stat));
    if (stat(name, &fnst)) {
        printf("getFile error: failed to open {%s} for {%s}", name, mode);
        return FILEDATA_ERROR;
    }
    if (!S_ISREG(fnst.st_mode)) {
        printf("getFile error: {%s} is not a file", name);
        return FILEDATA_ERROR;
    }
    FILE* file = fopen(name, mode);
    if (!file) {
        printf("getFile error: failed to open {%s} for {%s}", name, mode);
        return FILEDATA_ERROR;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    unsigned char* data = malloc(size + 1);
    fseek(file, 0, SEEK_SET);
    long i = 0;
    while (i < size && !feof(file)) {
        int tmpc = fgetc(file);
        if (tmpc < 0) tmpc = 0;
        data[i++] = (char)tmpc;
    }
    fclose(file);
    return (filedata){size, data};
}

filedata getBinFile(char* name) {
    return getFile(name, "rb");
}

filedata getTextFile(char* name) {
    filedata file = getFile(name, "r");
    if ((file.size > 0 && file.data[file.size - 1]) || file.size > -1) {
        file.data = realloc(file.data, file.size + 1);
        file.data[file.size++] = 0;
    }
    return file;
}

void freeFile(filedata file) {
    free(file.data);
}

unsigned char* decompressData(unsigned char* data, size_t insize, size_t outsize) {
    unsigned char* outbuf = malloc(outsize);
    int ret = LzmaUncompress(outbuf, &outsize, &data[LZMA_PROPS_SIZE], &insize, data, LZMA_PROPS_SIZE);
    if (ret != SZ_OK) {printf("decompressData error: [%d] at [%lu]\n", ret, outsize); free(outbuf); return NULL;}
    return outbuf;
}

unsigned char* compressData(unsigned char* data, size_t insize, size_t* outsize) {
    *outsize = insize + insize / 3 + 128;
    unsigned char* outbuf = malloc(LZMA_PROPS_SIZE + *outsize);
    memset(outbuf, 0, LZMA_PROPS_SIZE + *outsize);
    insize -= LZMA_PROPS_SIZE;
    size_t propsSize = LZMA_PROPS_SIZE;
    int ret = LzmaCompress(&outbuf[LZMA_PROPS_SIZE], outsize, data, insize, outbuf, &propsSize, 9, 0, -1, -1, -1, -1, 1);
    if (ret != SZ_OK) {printf("compressData error: [%d] at [%lu]\n", ret, *outsize); free(outbuf); return NULL;}
    *outsize += LZMA_PROPS_SIZE;
    return outbuf;
}

#define SEED_DEFAULT (0xDE8BADADBEF0EF0D)
#define SEED_OP1 (0xF0E1D2C3B4A59687)
#define SEED_OP2 (0x1F7BC9250917AD9C)

uint64_t randSeed = SEED_DEFAULT;

void setRandSeed(uint64_t val) {
    randSeed = val;
}

uint8_t getRandByte() {
    randSeed += SEED_OP1;
    randSeed *= SEED_OP1;
    randSeed += SEED_OP2;
    uintptr_t tmp = (randSeed >> 63);
    randSeed <<= 1;
    randSeed |= tmp;
    return randSeed;
}
