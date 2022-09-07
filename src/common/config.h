#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <stdbool.h>

#define CONFIG struct config

struct config_key {
    char* name;
    char* value;
};

struct config_sect {
    char* name;
    int keys;
    struct config_key* keydata;
};

struct config {
    int sects;
    struct config_sect* sectdata;
    bool changed;
};

struct config* openConfig(char*);
void declareConfigKey(struct config*, char*, char*, char*, bool);
void deleteConfigKey(struct config*, char*, char*);
char* getConfigKey(struct config*, char*, char*);
bool writeConfig(struct config*, char*);
void closeConfig(struct config*);

#endif
