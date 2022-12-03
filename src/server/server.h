#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <game/chunk.h>

#include <stdbool.h>
#include <stdarg.h>

#ifndef MAX_CLIENTS
    #define MAX_CLIENTS 256
#endif

#ifndef SERVER_SNDBUF_SIZE
    #define SERVER_SNDBUF_SIZE (1 << 19)
#endif

#ifndef SERVER_RCVBUF_SIZE
    #define SERVER_RCVBUF_SIZE (1 << 16)
#endif

#ifndef SERVER_OUTBUF_SIZE
    #define SERVER_OUTBUF_SIZE (1 << 20)
#endif

#ifndef SERVER_INBUF_SIZE
    #define SERVER_INBUF_SIZE (1 << 16)
#endif

#ifndef CLIENT_SNDBUF_SIZE
    #define CLIENT_SNDBUF_SIZE (1 << 16)
#endif

#ifndef CLIENT_RCVBUF_SIZE
    #define CLIENT_RCVBUF_SIZE (1 << 21)
#endif

#ifndef CLIENT_OUTBUF_SIZE
    #define CLIENT_OUTBUF_SIZE (1 << 17)
#endif

#ifndef CLIENT_INBUF_SIZE
    #define CLIENT_INBUF_SIZE (1 << 21)
#endif

enum {
    SERVER__MIN = -1,
    SERVER_PONG,
    SERVER_COMPATINFO,
    SERVER_LOGININFO,
    SERVER_UPDATECHUNK,
    SERVER_SETSKYCOLOR,
    SERVER_SETBLOCK,
    SERVER__MAX,
};

enum {
    CLIENT__MIN = -1,
    CLIENT_PING,
    CLIENT_COMPATINFO,
    CLIENT_LOGININFO,
    CLIENT_GETCHUNK,
    CLIENT_SETBLOCK,
    CLIENT__MAX,
};

struct server_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint8_t flags;
    char* server_str;
};

struct server_data_logininfo {
    uint8_t failed;
    char* reason;
    uint64_t uid;
    uint64_t password;
};

struct server_data_updatechunk {
    int64_t x;
    int64_t z;
    int len;
    unsigned char* cdata;
    struct blockdata data[65536];
};

struct server_data_setskycolor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct server_data_setblock {
    int64_t x;
    int64_t z;
    uint8_t y;
    struct blockdata data;
};

struct client_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint8_t flags;
    char* client_str;
};

struct client_data_logininfo {
    uint64_t uid;
    uint64_t password;
    char* username;
};

struct client_data_getchunk {
    int64_t x;
    int64_t z;
};

struct client_data_setblock {
    int64_t x;
    int64_t z;
    uint8_t y;
    struct blockdata data;
};

enum {
    SERVER_FLAG_NOAUTH = 1 << 0,
    SERVER_FLAG_PASSWD = 1 << 1,
};

enum {
    CLIENT_FLAG_CONONLY = 1 << 0,
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, int, char*);
void stopServer(void);

bool cliConnect(char*, int, void (*)(int, void*));
void cliDisconnect(void);
void cliSend(int, ...);

#endif
