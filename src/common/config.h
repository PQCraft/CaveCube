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

struct config* openConfig(char* /*path*/);
void declareConfigKey(struct config* /*cfg*/, char* /*sect*/, char* /*key*/, char* /*val*/, bool /*overwrite*/);
void deleteConfigKey(struct config* /*cfg*/, char* /*sect*/, char* /*key*/);
char* getConfigKey(struct config* /*cfg*/, char* /*sect*/, char* /*key*/);
bool writeConfig(struct config* /*cfg*/, char* /*path*/);
void closeConfig(struct config* /*cfg*/);

#endif
