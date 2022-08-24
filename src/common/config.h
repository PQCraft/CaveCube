#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <stdbool.h>

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
};

struct config_keys* openConfig(char*);
bool appendConfig(char*, struct config_keys*);
void declareKey(struct config*, char*, char*, char*, bool);
struct config_key* getConfigKey(char*, char*);
void writeConfig(struct config*, char*);
void closeConfig(struct config*);

#endif
