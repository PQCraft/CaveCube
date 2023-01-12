#include <main/main.h>
#include "config.h"
#include <common/common.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <errno.h>

void declareConfigKey(struct config* cfg, char* sect, char* key, char* val, bool overwrite) {
    int secti = -1;
    for (int i = 0; i < cfg->sects; ++i) {
        if (!cfg->sectdata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[i].name, sect)) {
            secti = i;
            break;
        }
    }
    if (secti < 0) {
        secti = cfg->sects++;
        cfg->sectdata = realloc(cfg->sectdata, cfg->sects * sizeof(*cfg->sectdata));
        memset(&cfg->sectdata[secti], 0, sizeof(*cfg->sectdata));
        cfg->sectdata[secti].name = strdup(sect);
    }
    int keyi = -1;
    for (int i = 0; i < cfg->sectdata[secti].keys; ++i) {
        if (!cfg->sectdata[secti].keydata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[secti].keydata[i].name, key)) {
            keyi = i;
            break;
        }
    }
    if (keyi < 0) {
        #if DBGLVL(1)
        printf("Adding key {%s} to section {%s} with value {%s}...\n", key, sect, val);
        #endif
        keyi = cfg->sectdata[secti].keys++;
        cfg->sectdata[secti].keydata = realloc(cfg->sectdata[secti].keydata, cfg->sectdata[secti].keys * sizeof(*cfg->sectdata[secti].keydata));
        memset(&cfg->sectdata[secti].keydata[keyi], 0, sizeof(*cfg->sectdata[secti].keydata));
        cfg->sectdata[secti].keydata[keyi].name = strdup(key);
        cfg->sectdata[secti].keydata[keyi].value = strdup(val);
        cfg->changed = true;
    } else if (overwrite) {
        #if DBGLVL(1)
        printf("Rewriting key {%s} in section {%s} with old value {%s} with new value {%s}...\n", key, sect, cfg->sectdata[secti].keydata[keyi].value, val);
        #endif
        free(cfg->sectdata[secti].keydata[keyi].value);
        cfg->sectdata[secti].keydata[keyi].value = strdup(val);
        cfg->changed = true;
    }
}

char* getConfigKey(struct config* cfg, char* sect, char* key) {
    int secti = -1;
    for (int i = 0; i < cfg->sects; ++i) {
        if (!cfg->sectdata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[i].name, sect)) {
            secti = i;
            break;
        }
    }
    if (secti < 0) return NULL;
    int keyi = -1;
    for (int i = 0; i < cfg->sectdata[secti].keys; ++i) {
        if (!cfg->sectdata[secti].keydata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[secti].keydata[i].name, key)) {
            keyi = i;
            break;
        }
    }
    if (keyi < 0) return NULL;
    return cfg->sectdata[secti].keydata[keyi].value;
}

void deleteConfigKey(struct config* cfg, char* sect, char* key) {
    int secti = -1;
    for (int i = 0; i < cfg->sects; ++i) {
        if (!cfg->sectdata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[i].name, sect)) {
            secti = i;
            break;
        }
    }
    if (secti < 0) return;
    int keyi = -1;
    for (int i = 0; i < cfg->sectdata[secti].keys; ++i) {
        if (!cfg->sectdata[secti].keydata[i].name) continue;
        if (!strcasecmp(cfg->sectdata[secti].keydata[i].name, key)) {
            keyi = i;
            break;
        }
    }
    if (keyi < 0) return;
    free(cfg->sectdata[secti].keydata[keyi].name);
    cfg->sectdata[secti].keydata[keyi].name = NULL;
    free(cfg->sectdata[secti].keydata[keyi].value);
    cfg->changed = true;
}

struct config* openConfig(char* path) {
    #if DBGLVL(1)
    printf("openConfig: {%s}\n", path);
    #endif
    if (!path) return calloc(1, sizeof(struct config));
    file_data fdata = getTextFile(path);
    if (!fdata.data) return NULL;
    --fdata.size;
    struct config* cfg = calloc(1, sizeof(*cfg));
    int linestart = 0;
    int lineend = 0;
    char* sect = strdup("");
    while (1) {
        bool inStr = false;
        bool esc = false;
        int eqpos = -1;
        for (; linestart < fdata.size && (fdata.data[linestart] == ' ' || fdata.data[linestart] == '\t'); ++linestart) {}
        int type = (fdata.data[linestart] == '#' || fdata.data[linestart] == '\n') ? 0 : ((fdata.data[linestart] == '[') ? 2 : 1);
        int namestart = linestart;
        int nameend = -1;
        if (type == 0) {
            eqpos = linestart;
        }
        for (lineend = linestart; lineend < fdata.size; ++lineend) {
            if (eqpos >= 0) {
                if (type == 1) {
                    if (fdata.data[lineend] == '"' && !esc) inStr = !inStr;
                    else if ((!esc || !inStr) && fdata.data[lineend] == '\n') break;
                    if (esc) esc = false;
                    else if (fdata.data[lineend] == '\\') esc = true;
                } else {
                    if (fdata.data[lineend] == '\n') break;
                }
            } else {
                //printf("on: {%c} (%d)\n", fdata.data[lineend], fdata.data[lineend]);
                if (type == 2) {
                    if (fdata.data[lineend] == ']') {
                        if (nameend < 0) {
                            nameend = lineend;
                            eqpos = lineend;
                            linestart = eqpos + 1;
                        }
                    } else if (fdata.data[lineend] == '\n') {
                        break;
                    }
                } else {
                    if (fdata.data[lineend] == ' ' || fdata.data[lineend] == '\t') {
                        if (nameend < 0) nameend = lineend;
                    } else if (fdata.data[lineend] == '=') {
                        if (nameend < 0) {
                            nameend = lineend;
                        }
                        eqpos = lineend;
                        linestart = eqpos + 1;
                        //printf("set: [%d]\n", linestart);
                    }
                }
            }
        }
        for (; linestart < lineend && (fdata.data[linestart] == ' ' || fdata.data[linestart] == '\t'); ++linestart) {}
        if (type == 2) {
            for (; namestart < nameend && (fdata.data[namestart] == ' ' || fdata.data[namestart] == '\t'); ++namestart) {}
            for (; nameend > namestart && (fdata.data[nameend - 1] == ' ' || fdata.data[nameend - 1] == '\t'); --nameend) {}
        }
        //printf("type: [%d], [%d, %d], [%d, %d]\n", type, namestart, nameend, linestart, lineend);
        if (nameend > namestart && type > 0) {
            if (type == 2) {
                ++namestart;
                for (; namestart < nameend && (fdata.data[namestart] == ' ' || fdata.data[namestart] == '\t'); ++namestart) {}
                int sectlen = nameend - namestart;
                sect = realloc(sect, sectlen + 1);
                {
                    char* tmpsect = sect;
                    for (int i = namestart; i < nameend; ++i) {
                        *tmpsect++ = fdata.data[i];
                    }
                    *tmpsect = 0;
                }
                //printf("SECTION: {%s}\n", sect);
            } else if (lineend >= linestart) {
                int namelen = nameend - namestart;
                char* name = malloc(namelen + 1);
                {
                    char* tmpname = name;
                    for (int i = namestart; i < nameend; ++i) {
                        *tmpname++ = fdata.data[i];
                    }
                    *tmpname = 0;
                }
                //printf("NAME: {%s}\n", name);
                int newlineend = lineend;
                for (; newlineend > linestart && (fdata.data[newlineend - 1] == ' ' || fdata.data[newlineend - 1] == '\t'); --newlineend) {}
                int linelen = newlineend - linestart;
                char* line = malloc(linelen + 1);
                memcpy(line, &fdata.data[linestart], linelen);
                line[linelen] = 0;
                //printf("LINE PRE: {%s}\n", line);
                char* outline = malloc(linelen + 1);
                int olptr = 0;
                inStr = false;
                for (int i = 0; i < linelen; ++i) {
                    char chr = line[i];
                    if (chr == '"') {
                        inStr = !inStr;
                        chr = 0;
                    } else if (inStr && chr == '\\') {
                        char tmpchr = line[++i];
                        switch (tmpchr) {
                            case 'a':;
                                chr = '\a';
                                break;
                            case 'b':;
                                chr = '\b';
                                break;
                            case 'e':;
                                chr = '\e';
                                break;
                            case 'f':;
                                chr = '\f';
                                break;
                            case 'n':;
                                chr = '\n';
                                break;
                            case 'r':;
                                chr = '\r';
                                break;
                            case 't':;
                                chr = '\t';
                                break;
                            case 'v':;
                                chr = '\v';
                                break;
                            case '\\':;
                                chr = '\\';
                                break;
                            case '"':;
                                chr = '"';
                                break;
                            case '\n':;
                                chr = 0;
                                break;
                            case 'x':;
                                char c1, c2;
                                if ((c1 = line[++i])) {
                                    if ((c2 = line[++i])) {
                                        if (c1 >= 'a' && c1 <= 'f') c1 -= 32;
                                        if (c2 >= 'a' && c2 <= 'f') c2 -= 32;
                                        #define h2i(x) ((x >= '0' && x <= '9') ? x - 48 : ((x >= 'A' && x <= 'F') ? x - 55 : -1))
                                        int h1, h2;
                                        if ((h1 = h2i(c1)) < 0 || (h2 = h2i(c2)) < 0) i -= 3;
                                        else chr = h1 * 16 + h2;
                                    } else {
                                        i -= 3;
                                    }
                                } else {
                                    i -= 2;
                                }
                                break;
                            default:;
                                --i;
                                break;
                        }
                    }
                    if (chr) outline[olptr++] = chr;
                }
                outline[olptr] = 0;
                //printf("LINE POST: {%s}\n", outline);
                declareConfigKey(cfg, sect, name, outline, true);
                free(name);
                free(line);
                free(outline);
            }
        }
        if (lineend >= fdata.size) break;
        else linestart = lineend + 1;
    }
    free(sect);
    cfg->changed = false;
    freeFile(fdata);
    return cfg;
}

static force_inline void writeKeys(struct config* cfg, int i, FILE* outfile) {
    for (int j = 0; j < cfg->sectdata[i].keys; ++j) {
        fputs(cfg->sectdata[i].keydata[j].name, outfile);
        fputs(" = ", outfile);
        bool q = false;
        {
            bool f = true;
            for (unsigned char* tmpstr = (unsigned char*)cfg->sectdata[i].keydata[j].value; *tmpstr; ++tmpstr) {
                if (*tmpstr < 32 || *tmpstr > 126 || (!*(tmpstr + 1) && (*tmpstr == ' ' || *tmpstr == '\t'))) {q = true; break;}
                if (f) {f = false; if (*tmpstr == ' ' || *tmpstr == '\t') {q = true; break;}}
            }
        }
        if (q) fputc('"', outfile);
        for (unsigned char* tmpstr = (unsigned char*)cfg->sectdata[i].keydata[j].value; *tmpstr; ++tmpstr) {
            if (*tmpstr == '"') {
                fputs("\\\"", outfile);
            } else if (*tmpstr == '\\') {
                switch (*tmpstr) {
                    case 'a':;
                    case 'b':;
                    case 'e':;
                    case 'f':;
                    case 'n':;
                    case 'r':;
                    case 't':;
                    case 'v':;
                    case '\n':;
                    case 0:;
                        fputs("\\\\", outfile);
                        break;
                    default:;
                        fputs("\\", outfile);
                        break;
                }
            } else if (*tmpstr < 32 || *tmpstr > 126) {
                switch (*tmpstr) {
                    case '\a':;
                        fputs("\\a", outfile);
                        break;
                    case '\b':;
                        fputs("\\b", outfile);
                        break;
                    case '\e':;
                        fputs("\\e", outfile);
                        break;
                    case '\f':;
                        fputs("\\f", outfile);
                        break;
                    case '\n':;
                        fputs("\\n", outfile);
                        break;
                    case '\r':;
                        fputs("\\r", outfile);
                        break;
                    case '\t':;
                        fputc('\t', outfile);
                        break;
                    case '\v':;
                        fputs("\\v", outfile);
                        break;
                    default:;
                        fprintf(outfile, "\\x%02X", *tmpstr);
                        break;
                }
            } else {
                fputc(*tmpstr, outfile);
            }
        }
        if (q) fputc('"', outfile);
        fputc('\n', outfile);
    }
}

bool writeConfig(struct config* cfg, char* name) {
    #if DBGLVL(1)
    printf("Writing config to '%s'...\n", name);
    #endif
    FILE* outfile = fopen(name, "w");
    if (!outfile) {
        #if DBGLVL(1)
        printf("Failed to open file: '%s'\n", strerror(errno));
        #endif
        return false;
    }
    bool first = true;
    for (int i = 0; i < cfg->sects; ++i) {
        if (!cfg->sectdata[i].keys || *cfg->sectdata[i].name) continue;
        writeKeys(cfg, i, outfile);
        first = false;
    }
    for (int i = 0; i < cfg->sects; ++i) {
        if (!cfg->sectdata[i].keys || !*cfg->sectdata[i].name) continue;
        if (first) {first = false;} else {fputc('\n', outfile);}
        fputc('[', outfile);
        fputs(cfg->sectdata[i].name, outfile);
        fputc(']', outfile);
        fputc('\n', outfile);
        writeKeys(cfg, i, outfile);
    }
    fclose(outfile);
    return true;
}

void closeConfig(struct config* cfg) {
    for (int i = 0; i < cfg->sects; ++i) {
        for (int j = 0; j < cfg->sectdata[i].keys; ++j) {
            if (cfg->sectdata[i].keydata[j].name) {
                free(cfg->sectdata[i].keydata[j].name);
                free(cfg->sectdata[i].keydata[j].value);
            }
        }
        free(cfg->sectdata[i].name);
        free(cfg->sectdata[i].keydata);
    }
    free(cfg->sectdata);
    free(cfg);
}
