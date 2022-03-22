#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>

#include "../common/common.h"

// Vertex format: position x, position y, position z, texture x, texture y

// Create a BMD 3D model and return a pointer to the data (free pointer when done)
unsigned char* createBMD(uint32_t* indices, uint32_t isize, float* vertices, uint32_t vsize, uint32_t* size) {
    // Calculate uncompressed data size
    *size = isize + vsize + sizeof(uint32_t) * 2;
    // Allocate buffer
    unsigned char* outbuf = malloc(*size + sizeof(uint32_t));
    memset(outbuf, 0, *size + sizeof(uint32_t));
    // Copy uncompressed data size into buffer and increment buffer
    memcpy(outbuf, size, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    // Copy index array size into buffer and increment buffer
    int tsize = isize / sizeof(uint32_t);
    memcpy(outbuf, &tsize, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    // Copy index array into buffer and increment buffer
    memcpy(outbuf, indices, isize);
    outbuf += isize;
    // Copy vertex array size into buffer and increment buffer
    tsize = vsize / sizeof(float);
    memcpy(outbuf, &tsize, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    // Copy vertex array into buffer and increment buffer
    memcpy(outbuf, vertices, vsize);
    outbuf += vsize;
    // Move buffer to front of data
    outbuf -= *size;
    // Compress data
    size_t outsize = 0;
    unsigned char* data = compressData(outbuf, *size, &outsize);
    // Copy new compressed data over old uncompressed data
    memcpy(outbuf, data, *size);
    outbuf -= sizeof(uint32_t);
    outsize += sizeof(uint32_t);
    // Resize buffer and free old compressed data
    outbuf = realloc(outbuf, outsize);
    free(data);
    // Return size and buffer
    *size = outsize;
    return outbuf;
}

// Read a BMD 3D model and create an array of indices and vertices at the pointers passed in (free pointers when done)
bool readBMD(unsigned char* data, uint32_t size, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    // Check if given invalid or empty data
    if (!data) return false;
    // Get original size
    uint32_t nsize = 0;
    memcpy(&nsize, data, sizeof(uint32_t));
    // Offset buffer
    data += sizeof(uint32_t);
    size -= sizeof(uint32_t);
    // Decompress data
    unsigned char* out = decompressData(data, size, nsize);
    unsigned char* tmpout = out;
    // Get size of indices
    memcpy(isize, tmpout, sizeof(uint32_t));
    tmpout += sizeof(uint32_t);
    // Allocate and copy over indices
    *indices = malloc(*isize * sizeof(uint32_t));
    memcpy(*indices, tmpout, *isize * sizeof(uint32_t));
    tmpout += *isize * sizeof(uint32_t);
    // Get size of vertices
    memcpy(vsize, tmpout, sizeof(uint32_t));
    tmpout += sizeof(uint32_t);
    // Allocate and copy over vertices
    *vertices = malloc(*vsize * sizeof(float));
    memcpy(*vertices, tmpout, *vsize * sizeof(uint32_t));
    free(out);
    return true;
}

// Calls readBMD using the data in a filedata structure
bool loadBMD(filedata file, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    return readBMD(file.data, file.size, indices, isize, vertices, vsize);
}

// Calls readBMD using the data in a filedata structure and frees the filedata structure when done
bool temploadBMD(filedata file, uint32_t** indices, uint32_t* isize, float** vertices, uint32_t* vsize) {
    bool ret = loadBMD(file, indices, isize, vertices, vsize);
    freeFile(file);
    return ret;
}
