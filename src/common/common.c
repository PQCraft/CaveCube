#include <main/main.h>
#include "common.h"
#include <zlib/zlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#ifdef _WIN32
    #include <windows.h>
#endif

#include <common/glue.h>

int getCoreCt() {
    int corect = 1;
    #if defined(_WIN32)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        corect = sysinfo.dwNumberOfProcessors;
    #elif defined(BSD) || defined(__APPLE__)
        int mib[4];
        size_t len = sizeof(corect);
        mib[0] = CTL_HW;
        mib[1] = HW_AVAILCPU;
        sysctl(mib, 2, &corect, &len, NULL, 0);
    #else
        corect = sysconf(_SC_NPROCESSORS_ONLN);
    #endif
    if (corect > MAX_THREADS) corect = MAX_THREADS;
    if (corect < 1) corect = 1;
    return corect;
}

#ifdef _WIN32
LARGE_INTEGER perfctfreq;
#endif

uint64_t altutime() {
    #ifndef _WIN32
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000 + time.tv_nsec / 1000;
    #else
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart * 1000000 / perfctfreq.QuadPart;
    #endif
}

void microwait(uint64_t d) {
    #ifndef _WIN32
    #ifndef __EMSCRIPTEN__
    struct timespec dts;
    dts.tv_sec = d / 1000000;
    dts.tv_nsec = (d % 1000000) * 1000;
    nanosleep(&dts, NULL);
    #else
    emscripten_sleep(round((double)(d) / 1000.0));
    #endif
    #else
    Sleep(round((double)(d) / 1000.0));
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
        char* tmp = realpath(argv[0], epbuf);
        (void)tmp;
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
    if (!path || !(*path)) return -1;
    struct stat pathstat;
    if (stat(path, &pathstat)) return -1;
    return !(S_ISDIR(pathstat.st_mode));
}

bool rm(char* path) {
    static int rmIndex = 0;
    if (isFile(path)) {
        return !remove(path);
    }
    char* odir = (rmIndex) ? NULL : getcwd(NULL, 0);
    ++rmIndex;
    if (chdir(path)) goto rm_fail;
    DIR* cwd = opendir(".");
    struct dirent* dir;
    struct stat pathstat;
    while ((dir = readdir(cwd))) {
        if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")) {
            stat(dir->d_name, &pathstat);
            if (S_ISDIR(pathstat.st_mode)) {rm(dir->d_name);}
            else {remove(dir->d_name);}
        }
    }
    --rmIndex;
    int ret = chdir((rmIndex) ? ".." : odir);
    if (!rmIndex) free(odir);
    return !rmdir(path);
    rm_fail:;
    --rmIndex;
    ret = chdir((rmIndex) ? ".." : odir);
    if (!rmIndex) free(odir);
    ret = rmdir(path);
    (void)ret;
    return false;
}

bool md(char* path) {
    switch (isFile(path)) {
        case 0:;
            return true;
            break;
        case 1:;
            fprintf(stderr, "'%s' is not a directory\n", path);
            return false;
            break;
    }
    char tmp[MAX_PATH];
    char *ptr = NULL;
    size_t len;
    strcpy(tmp, path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/' || tmp[len - 1] == PATHSEP) tmp[len - 1] = 0;
    for (ptr = tmp + 1; *ptr; ++ptr) {
        if (*ptr == '/' || *ptr == PATHSEP) {
            *ptr = 0;
            mkdir(tmp);
            *ptr = PATHSEP;
        }
    }
    return !mkdir(tmp);
}

char** dirstack = NULL;
int dirstackptr = 0;

bool pushdir(char* dir, bool mk) {
    if (mk && !md(dir)) return false;
    char* cwd = getcwd(NULL, 0);
    if (!cwd) return false;
    if (chdir(dir)) {free(cwd); return false;}
    dirstack = realloc(dirstack, (dirstackptr + 1) * sizeof(*dirstack));
    dirstack[dirstackptr] = cwd;
    ++dirstackptr;
    return true;
}

bool popdir() {
    --dirstackptr;
    if (chdir(dirstack[dirstackptr])) {++dirstackptr; return false;}
    free(dirstack[dirstackptr]);
    dirstack = realloc(dirstack, (dirstackptr + 1) * sizeof(*dirstack));
    return true;
}

file_data getFile(char* name, char* mode) {
    struct stat fnst;
    memset(&fnst, 0, sizeof(struct stat));
    if (stat(name, &fnst)) {
        fprintf(stderr, "getFile: failed to open {%s} for {%s}\n", name, mode);
        return FILEDATA_ERROR;
    }
    if (!S_ISREG(fnst.st_mode)) {
        fprintf(stderr, "getFile: {%s} is not a file\n", name);
        return FILEDATA_ERROR;
    }
    FILE* file = fopen(name, mode);
    if (!file) {
        fprintf(stderr, "getFile: failed to open {%s} for {%s}\n", name, mode);
        return FILEDATA_ERROR;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) size = 0;
    unsigned char* data = malloc(size);
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
    char* newdata = malloc(file.size + 1);
    int newsize = 0;
    for (int i = 0; i < file.size && file.data[i]; ++i) {
        if (file.data[i] != '\r') {
            newdata[newsize++] = file.data[i];
        }
    }
    newdata[newsize++] = 0;
    newdata = realloc(newdata, newsize);
    free(file.data);
    file.data = (unsigned char*)newdata;
    file.size = newsize;
    //printf("getTextFile: %s:\n%s\n", name, newdata);
    return file;
}

file_data catFiles(file_data file1, bool freefile1, file_data file2, bool freefile2) {
    file_data file;
    file.size = ((file1.size > 0) ? file1.size : 0) + ((file2.size > 0) ? file2.size : 0);
    file.data = malloc(file.size);
    if (file1.size > 0) memcpy(file.data, file1.data, file1.size);
    if (file2.size > 0) memcpy(file.data + ((file1.size > 0) ? file1.size : 0), file2.data, file2.size);
    if (freefile1) freeFile(file1);
    if (freefile2) freeFile(file2);
    return file;
}

file_data catTextFiles(file_data file1, bool freefile1, file_data file2, bool freefile2) {
    file_data file;
    file.size = ((file1.size > 1) ? file1.size : 1) + ((file2.size > 1) ? file2.size : 1) - 1;
    if (file.size < 1) file.size = 1;
    file.data = malloc(file.size);
    if (file1.size > 1) memcpy(file.data, file1.data, file1.size - 1);
    if (file2.size > 1) memcpy(file.data + ((file1.size > 1) ? file1.size - 1 : 0), file2.data, file2.size - 1);
    file.data[file.size - 1] = 0;
    if (freefile1) freeFile(file1);
    if (freefile2) freeFile(file2);
    return file;
}

void freeFile(file_data file) {
    if (file.data) free(file.data);
}

#define GCVWOUT(x) {++len; *out++ = (x);}

void getInfoVar(char* fdata, char* var, char* dval, long size, char* out) {
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

char* getInfoVarAlloc(char* fdata, char* var, char* dval, long size) {
    if (size < 1) size = GCBUFSIZE;
    char* out = malloc(size);
    getInfoVar(fdata, var, dval, size, out);
    out = realloc(out, strlen(out) + 1);
    return out;
}

char* getInfoVarStatic(char* fdata, char* var, char* dval, long size) {
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
    getInfoVar(fdata, var, dval, size, out);
    return out;
}

bool getBool(char* str) {
    return (!strcasecmp(str, "true") || !strcasecmp(str, "yes") || atoi(str) != 0);
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
#define SEED_OP1 (0xA101D0C304A09683)
#define SEED_OP2 (0xB070C0050907A09C)

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
    randSeed[s] *= SEED_OP1;
    randSeed[s] ^= SEED_OP2;
    randSeed[s] >>= 5;
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

char* spCharToStr(char c) {
    static char str[8];
    if (c >= 32 && c <= 126) {
        str[0] = c;
        str[1] = 0;
    } else {
        bool d = false;
        switch (c) {
            case 0:;
                str[1] = '0';
                break;
            case '\a':;
                str[1] = 'a';
                break;
            case '\b':;
                str[1] = 'b';
                break;
            case '\e':;
                str[1] = 'e';
                break;
            case '\f':;
                str[1] = 'f';
                break;
            case '\n':;
                str[1] = 'n';
                break;
            case '\r':;
                str[1] = 'r';
                break;
            case '\t':;
                str[1] = 't';
                break;
            case '\v':;
                str[1] = 'v';
                break;
            default:;
                d = true;
                sprintf(str, "\\x%02u", (unsigned char)c);
                break;
        }
        if (!d) {
            str[0] = '\\';
            str[2] = 0;
        }
    }
    return str;
}

int readStrUntil(char* input, char c, char* output) {
    int size = 0;
    while (*input && *input != c) {
        *output++ = *input;
        ++input;
        ++size;
    }
    *output = 0;
    return size;
}

int ezCompress(int level, unsigned insize, void* indata, unsigned outsize, void* outdata) {
    z_stream stream = {.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};
    stream.avail_in = insize;
    stream.next_in = indata;
    stream.avail_out = outsize;
    stream.next_out = outdata;
    deflateInit(&stream, level);
    int ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (ret < 0) return ret;
    return stream.total_out;
}

int ezDecompress(unsigned insize, void* indata, unsigned outsize, void* outdata) {
    z_stream stream = {.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};
    stream.avail_in = insize;
    stream.next_in = indata;
    stream.avail_out = outsize;
    stream.next_out = outdata;
    inflateInit(&stream);
    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (ret < 0) return ret;
    return stream.total_out;
}
