#include "config.h"
#include <common/common.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define h2i(x) ((x >= '0' && x <= '9') ? x - 48 : ((x >= 'A' && x <= 'F') ? x - 55 : -1))

struct config_keys* openConfig(char* path) {
    file_data fdata = getTextFile(path);
    if (!fdata.data) return NULL;
    --fdata.size;
    struct config* cfg = calloc(1, sizeof(*cfg));
    int linestart = 0;
    int lineend = 0;
    while (1) {
        bool inStr = false;
        bool esc = false;
        for (lineend = linestart; lineend < fdata.size; ++lineend) {
            if (fdata.data[lineend] == '"') {if (!esc) inStr = !inStr;}
            if (fdata.data[lineend] == '\n') {
                if (!esc || !inStr) break;
            }
            if (esc) esc = false;
            else if (fdata.data[lineend] == '\\') esc = true;
        }
        for (; linestart < lineend && (fdata.data[linestart] == ' ' || fdata.data[linestart] == '\t'); ++linestart) {}
        if (lineend > linestart) {
            int newlineend = lineend;
            for (; newlineend > linestart && (fdata.data[newlineend - 1] == ' ' || fdata.data[newlineend - 1] == '\t'); --newlineend) {}
            int linelen = newlineend - linestart;
            char* line = malloc(linelen + 1);
            memcpy(line, &fdata.data[linestart], linelen);
            line[linelen] = 0;
            printf("LINE PRE: {%s}\n", line);
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
            printf("LINE POST: {%s}\n", outline);
            free(line);
            free(outline);
        }
        if (lineend >= fdata.size) break;
        else linestart = lineend + 1;
    }
}
