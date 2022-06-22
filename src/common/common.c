#include "main.h"
#include "common.h"

#include <LzmaLib.h>
#include <7zTypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

int getCoreCt() {
    #if defined(_WIN32)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #elif defined(BSD) || defined(__APPLE__)
        int mib[4];
        int corect;
        size_t len = sizeof(corect);
        mib[0] = CTL_HW;
        mib[1] = HW_AVAILCPU;
        sysctl(mib, 2, &corect, &len, NULL, 0);
        return (corect > 0) ? corect : 1;
    #else
        return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
}

uint64_t altutime() {
    struct timeval time1;
    gettimeofday(&time1, NULL);
    return time1.tv_sec * 1000000 + time1.tv_usec;
}

void microwait(uint64_t d) {
    #ifndef _WIN32
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #else
    uint64_t t = d + altutime();
    while (t > altutime()) {}
    #endif
}

static char* bfnbuf = NULL;

char* basefilename(char* fn) {
    int32_t fnlen = strlen(fn);
    int32_t i;
    for (i = fnlen; i > -1; --i) {
        if (fn[i] == '/') break;
        #ifdef _WIN32
        if (fn[i] == '\\') break;
        #endif
    }
    bfnbuf = realloc(bfnbuf, fnlen - i);
    strcpy(bfnbuf, fn + i + 1);
    return bfnbuf;
}

char* pathfilename(char* fn) {
    int32_t fnlen = strlen(fn);
    int32_t i;
    for (i = fnlen; i > -1; --i) {
        if (fn[i] == '/') break;
        #ifdef _WIN32
        if (fn[i] == '\\') break;
        #endif
    }
    bfnbuf = realloc(bfnbuf, i + 2);
    char tmp = fn[i + 1];
    fn[i + 1] = 0;
    strcpy(bfnbuf, fn);
    fn[i + 1] = tmp;
    return bfnbuf;
}

static char* epbuf = NULL;

char* execpath() {
    if (!epbuf) epbuf = malloc(MAX_PATH + 1);
    #ifndef _WIN32
        #ifndef __APPLE__
            #ifndef __FreeBSD__
                int32_t scsize;
                if ((scsize = readlink("/proc/self/exe", epbuf, MAX_PATH)) == -1)
                    #ifndef __linux__
                    if ((scsize = readlink("/proc/curproc/file", epbuf, MAX_PATH)) == -1)
                    #endif
                        goto scargv;
                epbuf[scsize] = 0;
            #else
                int mib[4];
                mib[0] = CTL_KERN;
                mib[1] = KERN_PROC;
                mib[2] = KERN_PROC_PATHNAME;
                mib[3] = -1;
                size_t cb = MAX_PATH;
                sysctl(mib, 4, epbuf, &cb, NULL, 0);
                char* tmpstartcmd = realpath(epbuf, NULL);
                strcpy(epbuf, tmpstartcmd);
                nfree(tmpstartcmd);
            #endif
        #else
            uint32_t tmpsize = MAX_PATH;
            if (_NSGetExecutablePath(epbuf, &tmpsize)) {
                goto scargv;
            }
            
            char* tmpstartcmd = realpath(epbuf, NULL);
            strcpy(epbuf, tmpstartcmd);
            nfree(tmpstartcmd);
        #endif
    #else
        if (!GetModuleFileName(NULL, epbuf, MAX_PATH)) {
            goto scargv;
        }
    #endif
    goto skipscargv;
    scargv:;
    if (strcmp(argv[0], basefilename(argv[0]))) {
        #ifndef _WIN32
        realpath(argv[0], epbuf);
        #else
        _fullpath(epbuf, argv[0], MAX_PATH);
        #endif
    } else {
        strcpy(epbuf, argv[0]);
    }
    skipscargv:;
    return epbuf;
}

int isFile(char* path) {
    struct stat pathstat;
    if (stat(path, &pathstat)) return -1;
    return !(S_ISDIR(pathstat.st_mode));
}

file_data getFile(char* name, char* mode) {
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
    return (file_data){size, data};
}

file_data getBinFile(char* name) {
    return getFile(name, "rb");
}

file_data getTextFile(char* name) {
    file_data file = getFile(name, "r");
    if ((file.size > 0 && file.data[file.size - 1]) || file.size > -1) {
        file.data = realloc(file.data, file.size + 1);
        file.data[file.size++] = 0;
    }
    return file;
}

void freeFile(file_data file) {
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
    //printf("decompressData: [%lu]\n", outsize + LZMA_PROPS_SIZE);
    if (ret != SZ_OK) {fprintf(stderr, "decompressData: error [%d] at [%"PRIuz"]\n", ret, outsize); free(outbuf); return NULL;}
    return outbuf;
}

unsigned char* compressData(unsigned char* data, size_t insize, size_t* outsize) {
    *outsize = insize + insize / 3 + 128;
    unsigned char* outbuf = malloc(LZMA_PROPS_SIZE + *outsize);
    memset(outbuf, 0, LZMA_PROPS_SIZE + *outsize);
    insize -= LZMA_PROPS_SIZE;
    size_t propsSize = LZMA_PROPS_SIZE;
    int ret = LzmaCompress(&outbuf[LZMA_PROPS_SIZE], outsize, data, insize, outbuf, &propsSize, 9, 0, -1, -1, -1, -1, 1);
    if (ret != SZ_OK) {fprintf(stderr, "compressData: error [%d] at [%"PRIuz"]\n", ret, *outsize); free(outbuf); return NULL;}
    *outsize += LZMA_PROPS_SIZE;
    return outbuf;
}

uint64_t qhash(char* str, int max) {
    uint64_t hash = 0x7FBA0FC3DEADBEEF;
    uint16_t len = 0;
    int tmp;
    while ((max < 0 && *str) || len < max) {
        ++len;
        hash ^= ((*str) & 0xFF) * 0xF65C403B1034;
        for (int i = -1; i < ((*str) & 0x0F); ++i) {
            tmp = hash & 1;
            hash = ((uint64_t)tmp << 63) | (hash >> 1);
        }
        hash ^= (uint64_t)len * 0xE578E432;
        tmp = hash & 1;
        hash = ((uint64_t)tmp << 63) | (hash >> 1);
        ++str;
    }
    return hash;
}

#define SEED_DEFAULT (0xDE8BADADBEF0EF0D)
#define SEED_OP1 (0xF0E1D2C3B4A59687)
#define SEED_OP2 (0x1F7BC9250917AD9C)

uint64_t randSeed[16] = {
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
    SEED_DEFAULT,
};

void setRandSeed(int s, uint64_t val) {
    randSeed[s] = val;
}

uint8_t getRandByte(int s) {
    randSeed[s] += SEED_OP1;
    randSeed[s] *= SEED_OP1;
    randSeed[s] += SEED_OP2;
    uintptr_t tmp = (randSeed[s] >> 63);
    randSeed[s] <<= 1;
    randSeed[s] |= tmp;
    return randSeed[s];
}

uint16_t getRandWord(int s) {
    return (getRandByte(s) << 8) |
           getRandByte(s);
}

uint32_t getRandDWord(int s) {
    return (getRandByte(s) << 24) |
           (getRandByte(s) << 16) |
           (getRandByte(s) << 8) |
           getRandByte(s);
}

uint64_t getRandQWord(int s) {
    return ((uint64_t)getRandByte(s) << 56) |
           ((uint64_t)getRandByte(s) << 48) |
           ((uint64_t)getRandByte(s) << 40) |
           ((uint64_t)getRandByte(s) << 32) |
           ((uint64_t)getRandByte(s) << 24) |
           ((uint64_t)getRandByte(s) << 16) |
           ((uint64_t)getRandByte(s) << 8) |
           (uint64_t)getRandByte(s);;
}
