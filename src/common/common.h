#ifndef COMMON_H
#define COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    long size;
    unsigned char* data;
} file_data;

#define FILEDATA_NULL (file_data){0, NULL}
#define FILEDATA_ERROR (file_data){-1, NULL}

#define GCBUFSIZE 32768

file_data getFile(char*, char*);
file_data getBinFile(char*);
file_data getTextFile(char*);
void freeFile(file_data);
unsigned char* decompressData(unsigned char*, size_t, size_t);
unsigned char* compressData(unsigned char*, size_t, size_t*);
void setRandSeed(int, uint64_t);
uint8_t getRandByte(int);
uint16_t getRandWord(int);
uint32_t getRandDWord(int);
uint64_t getRandQWord(int);
int isFile(char*);
void getConfigVar(char*, char*, char*, long, char*);
char* getConfigVarAlloc(char*, char*, char*, long);
char* getConfigVarStatic(char*, char*, char*, long);
bool getConfigValBool(char*);
uint64_t altutime(void);
void microwait(uint64_t);
char* basefilename(char*);
char* pathfilename(char*);
char* execpath(void);
int getCoreCt(void);

#endif
