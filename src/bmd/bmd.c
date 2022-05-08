#include "bmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <string.h>

#include <common.h>

#include <stdlib.h>
#include <stdio.h>

// Vertex format: position x, position y, position z, texture x, texture y

unsigned char* createBMD(bmd_data* data, uint32_t* size) {
    *size = sizeof(uint32_t) + sizeof(uint32_t) * 2 * data->parts;
    for (unsigned i = 0; i < data->parts; ++i) {
        *size += data->part[i].isize + data->part[i].vsize;
    }
    unsigned char* outbuf = malloc(*size + sizeof(uint32_t));
    memset(outbuf, 0, *size + sizeof(uint32_t));
    memcpy(outbuf, size, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    memcpy(outbuf, &data->parts, sizeof(uint32_t));
    outbuf += sizeof(uint32_t);
    for (unsigned i = 0; i < data->parts; ++i) {
        int tsize = data->part[i].isize / sizeof(uint32_t);
        memcpy(outbuf, &tsize, sizeof(uint32_t));
        outbuf += sizeof(uint32_t);
        memcpy(outbuf, data->part[i].indices, data->part[i].isize);
        outbuf += data->part[i].isize;
        tsize = data->part[i].vsize / sizeof(float);
        memcpy(outbuf, &tsize, sizeof(uint32_t));
        outbuf += sizeof(uint32_t);
        memcpy(outbuf, data->part[i].vertices, data->part[i].vsize);
        outbuf += data->part[i].vsize;
    }
    outbuf -= *size;
    size_t outsize = 0;
    {
        FILE* fp = fopen("output.dat", "w");
        fwrite(outbuf - sizeof(uint32_t), 1, *size, fp);
        fclose(fp);
    }
    unsigned char* cdata = compressData(outbuf, *size, &outsize);
    memcpy(outbuf, cdata, *size);
    outbuf -= sizeof(uint32_t);
    outsize += sizeof(uint32_t);
    outbuf = realloc(outbuf, outsize);
    free(cdata);
    *size = outsize;
    return outbuf;
}

bool readBMD(unsigned char* data, uint32_t size, bmd_data* bmd) {
    if (!data) return false;
    uint32_t nsize = 0;
    memcpy(&nsize, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
    size -= sizeof(uint32_t);
    unsigned char* out = decompressData(data, size, nsize);
    unsigned char* tmpout = out;
    uint32_t count = 0;
    memcpy(&count, tmpout, sizeof(uint32_t));
    //printf("count: [%u]\n", count);
    tmpout += sizeof(uint32_t);
    bmd->parts = count;
    bmd->part = malloc(count * sizeof(bmd_part));
    for (unsigned i = 0; i < count; ++i) {
        memcpy(&bmd->part[i].isize, tmpout, sizeof(uint32_t));
        //printf("isize[%u]: [%u]\n", i, bmd->part[i].isize);
        bmd->part[i].isize *= sizeof(uint32_t);
        tmpout += sizeof(uint32_t);
        bmd->part[i].indices = malloc(bmd->part[i].isize);
        memcpy(bmd->part[i].indices, tmpout, bmd->part[i].isize);
        tmpout += bmd->part[i].isize;
        memcpy(&bmd->part[i].vsize, tmpout, sizeof(uint32_t));
        //printf("vsize[%u]: [%u]\n", i, bmd->part[i].vsize);
        bmd->part[i].vsize *= sizeof(float);
        tmpout += sizeof(uint32_t);
        bmd->part[i].vertices = malloc(bmd->part[i].vsize);
        memcpy(bmd->part[i].vertices, tmpout, bmd->part[i].vsize);
        tmpout += bmd->part[i].vsize;
    }
    free(out);
    return true;
}

bool loadBMD(file_data file, bmd_data* bmd) {
    return readBMD(file.data, file.size, bmd);
}

bool temploadBMD(file_data file, bmd_data* bmd) {
    bool ret = loadBMD(file, bmd);
    freeFile(file);
    return ret;
}

void freeBMD(bmd_data* bmd) {
    for (unsigned i = 0; i < bmd->parts; ++i) {
        free(bmd->part[i].indices);
        free(bmd->part[i].vertices);
    }
    free(bmd->part);
}
