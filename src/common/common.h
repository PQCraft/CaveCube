#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef _WIN32
    #include <windows.h>
#endif

typedef struct {
    long size;
    unsigned char* data;
} file_data;

#define FILEDATA_NULL (file_data){0, NULL}
#define FILEDATA_ERROR (file_data){-1, NULL}

#define GCBUFSIZE 32768

int isFile(char*);
bool rm(char*);
bool md(char*);
file_data getFile(char*, char*);
file_data getBinFile(char*);
file_data getTextFile(char*);
file_data catFiles(file_data, bool, file_data, bool);
file_data catTextFiles(file_data, bool, file_data, bool);
void freeFile(file_data);
void getInfoVar(char*, char*, char*, long, char*);
char* getInfoVarAlloc(char*, char*, char*, long);
char* getInfoVarStatic(char*, char*, char*, long);
void setRandSeed(int, uint64_t);
uint8_t getRandByte(int);
uint16_t getRandWord(int);
uint32_t getRandDWord(int);
uint64_t getRandQWord(int);
uint64_t altutime(void);
void microwait(uint64_t);
char* basefilename(char*);
char* pathfilename(char*);
char* execpath(void);
int getCoreCt(void);
char* spCharToStr(char);
bool getBool(char*);
int readStrUntil(char*, char, char*);
int ezCompress(int, unsigned, void*, unsigned, void*);
int ezDecompress(unsigned, void*, unsigned, void*);

#ifdef _WIN32
extern LARGE_INTEGER perfctfreq;
#endif

#endif
