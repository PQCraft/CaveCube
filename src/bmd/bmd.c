#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>

#include <common.h>

// Vertex format: position x, position y, position z, texture x, texture y

unsigned char* createBMD(uint32_t* indices, uint32_t isize, float* vertices, uint32_t vsize, uint32_t* size) {
    
    *size = isize + vsize + sizeof(uint32_t) * 2;
    unsigned char* outbuf = malloc(*size + sizeof(uint32_t));
    memset(outbuf, 0, *size + sizeof(uint32_t));
    memcpy(outbuf, size, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    int tsize = isize / sizeof(uint32_t);
    memcpy(outbuf, &tsize, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    memcpy(outbuf, indices, isize);
    outbuf += isize;
    tsize = vsize / sizeof(float);
    memcpy(outbuf, &tsize, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    memcpy(outbuf, vertices, vsize);
    outbuf += vsize;
    outbuf -= *size;
    size_t outsize = 0;
    unsigned char* data = compressData(outbuf, *size, &outsize);
    memcpy(outbuf, data, *size);
    outbuf -= sizeof(uint32_t);
    outsize += sizeof(uint32_t);
    outbuf = realloc(outbuf, outsize);
    free(data);
    *size = outsize;
    return outbuf;
}

bool readBMD(unsigned char* data, uint32_t size, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    if (!data) return false;
    uint32_t nsize = 0;
    memcpy(&nsize, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
    size -= sizeof(uint32_t);
    unsigned char* out = decompressData(data, size, nsize);
    unsigned char* tmpout = out;
    memcpy(isize, tmpout, sizeof(uint32_t));
    *isize *= sizeof(uint32_t);
    tmpout += sizeof(uint32_t);
    *indices = malloc(*isize);
    memcpy(*indices, tmpout, *isize);
    tmpout += *isize;
    memcpy(vsize, tmpout, sizeof(uint32_t));
    *vsize *= sizeof(float);
    tmpout += sizeof(uint32_t);
    *vertices = malloc(*vsize);
    memcpy(*vertices, tmpout, *vsize);
    free(out);
    return true;
}

bool loadBMD(filedata file, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    return readBMD(file.data, file.size, indices, isize, vertices, vsize);
}

bool temploadBMD(filedata file, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    bool ret = loadBMD(file, indices, isize, vertices, vsize);
    freeFile(file);
    return ret;
}
