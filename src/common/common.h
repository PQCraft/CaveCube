#ifndef COMMON_H
#define COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    long size;
    unsigned char* data;
} filedata;

#define FILEDATA_NULL (filedata){0, NULL}
#define FILEDATA_ERROR (filedata){-1, NULL}

filedata getFile(char*, char*);
filedata getBinFile(char*);
filedata getTextFile(char*);
void freeFile(filedata);
unsigned char* decompressData(unsigned char*, size_t, size_t);
unsigned char* compressData(unsigned char*, size_t, size_t*);
void setRandSeed(uint64_t);
uint8_t getRandByte(void);
int isFile(char*);
void getConfigVar(char*, char*, char*, long, char*);
char* getConfigVarAlloc(char*, char*, char*, long);
char* getConfigVarStatic(char*, char*, char*, long);
bool getConfigValBool(char*);
uint64_t altutime(void);

#define GCBUFSIZE 32768

#endif
