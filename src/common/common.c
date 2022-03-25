#include "common.h"

#include <LzmaLib.h>
#include <7zTypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

uint64_t altutime() {
    struct timeval time1;
    gettimeofday(&time1, NULL);
    return time1.tv_sec * 1000000 + time1.tv_usec;
}

int isFile(char* path) {
    struct stat pathstat;
    if (stat(path, &pathstat)) return -1;
    return !(S_ISDIR(pathstat.st_mode));
}

filedata getFile(char* name, char* mode) {
    struct stat fnst;
    memset(&fnst, 0, sizeof(struct stat));
    if (stat(name, &fnst)) {
        fprintf(stderr, "getFile error: failed to open {%s} for {%s}\n", name, mode);
        return FILEDATA_ERROR;
    }
    if (!S_ISREG(fnst.st_mode)) {
        fprintf(stderr, "getFile error: {%s} is not a file\n", name);
        return FILEDATA_ERROR;
    }
    FILE* file = fopen(name, mode);
    if (!file) {
        fprintf(stderr, "getFile error: failed to open {%s} for {%s}\n", name, mode);
        return FILEDATA_ERROR;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    unsigned char* data = calloc(size + 1, 1);
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

#define GCVWOUT(x) {++len; *out++ = (x);}

void getConfigVar(char* fdata, char* var, char* dval, long size, char* out) {
    if (size < 1) size = GCBUFSIZE;
    if (!out) return;
    if (!var) return;
    int spcoff = 0;
    char* oout = out;
    *out = 0;
    if (!fdata) goto retdef;
    spcskip:;
    while (*fdata == ' ' || *fdata == '\t' || *fdata == '\n') {++fdata;}
    if (!*fdata) goto retdef;
    if (*fdata == '#') {while (*fdata && *fdata != '\n') {++fdata;} goto spcskip;}
    for (char* tvar = var; *fdata; ++fdata, ++tvar) {
        bool upc = false;
        if (*fdata >= 'A' && *fdata <= 'Z') {upc = true;}
        if (*fdata == ' ' || *fdata == '\t' || *fdata == '=') {
            while (*fdata == ' ' || *fdata == '\t') {++fdata;}
            if (*tvar || *fdata != '=') {while (*fdata && *fdata != '\n') {++fdata;} goto spcskip;}
            else {++fdata; goto getval;}
        } else if (upc && (*fdata) + 32 == *tvar) {
        } else if (upc && *fdata == *tvar) {
        } else if (!upc && *fdata == (*tvar) + 32) {
        } else if (*fdata != *tvar) {
            while (*fdata && *fdata != '\n') {++fdata;}
            goto spcskip;
        }
    }
    getval:;
    while (*fdata == ' ' || *fdata == '\t') {++fdata;}
    if (!*fdata) goto retdef;
    else if (*fdata == '\n') goto spcskip;
    bool inStr = false;
    long len = 0;
    while (*fdata && *fdata != '\n') {
        if (len < size) {
            if (*fdata == '"') {
                inStr = !inStr;
            } else {
                if (*fdata == '\\') {
                    if (inStr) {
                        ++fdata;
                        if (*fdata == '"') {GCVWOUT('"');}
                        else if (*fdata == 'n') {GCVWOUT('\n');}
                        else if (*fdata == 'r') {GCVWOUT('\r');}
                        else if (*fdata == 't') {GCVWOUT('\t');}
                        else if (*fdata == 'e') {GCVWOUT('\e');}
                        else if (*fdata == '\n') {GCVWOUT('\n');}
                        else if (*fdata == '\r' && *(fdata + 1) == '\n') {GCVWOUT('\n'); ++fdata;}
                        else {
                            GCVWOUT('\\');
                            GCVWOUT(*fdata);
                        }
                    } else {
                        ++fdata;
                        if (*fdata == '"') {GCVWOUT('"');}
                        else if (*fdata == '\n') {GCVWOUT('\n');}
                        else {
                            GCVWOUT('\\');
                            GCVWOUT(*fdata);
                        }
                    }
                } else {
                    GCVWOUT(*fdata);
                }
                if (!inStr && (*fdata == ' ' || *fdata == '\t')) ++spcoff;
                else {spcoff = 0;}
            }
        }
        ++fdata;
    }
    goto retok;
    retdef:;
    if (dval) {
        strcpy(oout, dval);
    } else {
        *oout = 0;
    }
    return;
    retok:;
    out -= spcoff;
    *out = 0;
}

char* getConfigVarAlloc(char* fdata, char* var, char* dval, long size) {
    if (size < 1) size = GCBUFSIZE;
    char* out = malloc(size);
    getConfigVar(fdata, var, dval, size, out);
    out = realloc(out, strlen(out) + 1);
    return out;
}

char* getConfigVarStatic(char* fdata, char* var, char* dval, long size) {
    if (size < 1) size = GCBUFSIZE;
    static long outsize = 0;
    static char* out = NULL;
    if (!out) {
        out = malloc(size);
        outsize = size;
    } else if (size != outsize) {
        out = realloc(out, size);
        outsize = size;
    }
    getConfigVar(fdata, var, dval, size, out);
    return out;
}

bool getConfigValBool(char* val) {
    if (!val) return -1;
    char* nval = strdup(val);
    for (char* nvalp = nval; *nvalp; ++nvalp) {
        if (*nvalp >= 'A' && *nvalp <= 'Z') *nvalp += 32;
    }
    bool ret = (!strcmp(nval, "true") || !strcmp(nval, "yes") || atoi(nval) != 0);
    free(nval);
    return ret;
}

unsigned char* decompressData(unsigned char* data, size_t insize, size_t outsize) {
    unsigned char* outbuf = malloc(outsize);
    outsize -= LZMA_PROPS_SIZE;
    int ret = LzmaUncompress(outbuf, &outsize, &data[LZMA_PROPS_SIZE], &insize, data, LZMA_PROPS_SIZE);
    if (ret != SZ_OK) {fprintf(stderr, "decompressData error: [%d] at [%lu]\n", ret, outsize); free(outbuf); return NULL;}
    return outbuf;
}

unsigned char* compressData(unsigned char* data, size_t insize, size_t* outsize) {
    *outsize = insize + insize / 3 + 128;
    unsigned char* outbuf = malloc(LZMA_PROPS_SIZE + *outsize);
    memset(outbuf, 0, LZMA_PROPS_SIZE + *outsize);
    insize -= LZMA_PROPS_SIZE;
    size_t propsSize = LZMA_PROPS_SIZE;
    int ret = LzmaCompress(&outbuf[LZMA_PROPS_SIZE], outsize, data, insize, outbuf, &propsSize, 9, 0, -1, -1, -1, -1, 1);
    if (ret != SZ_OK) {fprintf(stderr, "compressData error: [%d] at [%lu]\n", ret, *outsize); free(outbuf); return NULL;}
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
