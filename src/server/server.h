#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <game/chunk.h>

#include <stdbool.h>
#include <stdarg.h>

#ifndef MAX_CLIENTS
    #define MAX_CLIENTS 256
#endif

#ifndef SERVER_SNDBUF_SIZE
    #define SERVER_SNDBUF_SIZE (1 << 16)
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
    #define CLIENT_RCVBUF_SIZE (1 << 16)
#endif

#ifndef CLIENT_OUTBUF_SIZE
    #define CLIENT_OUTBUF_SIZE (1 << 16)
#endif

#ifndef CLIENT_INBUF_SIZE
    #define CLIENT_INBUF_SIZE (1 << 20)
#endif

enum {
    SERVER__MIN = -1,
    SERVER_PONG,
    SERVER_COMPATINFO,
    SERVER_NEWUID,
    SERVER_LOGINOK,
    SERVER_DISCONNECT,
    SERVER_UPDATECHUNK,
    SERVER_SETSKYCOLOR,
    SERVER_SETNATCOLOR,
    SERVER_SETBLOCK,
    SERVER__MAX,
};

enum {
    CLIENT__MIN = -1,
    CLIENT_PING,
    CLIENT_COMPATINFO,
    CLIENT_NEWUID,
    CLIENT_LOGIN,
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
enum {
    SERVER_COMPATINFO_FLAG_NOAUTH = 1 << 0,
    SERVER_COMPATINFO_FLAG_PASSWD = 1 << 1,
};

struct server_data_newuid {
    uint64_t uid;
};

struct server_data_loginok {
    char* username;
};

struct server_data_disconnect {
    char* reason;
};

struct server_data_updatechunk {
    int64_t x;
    int64_t z;
    int len;
    unsigned char* cdata;
    struct blockdata* bdata;
};

struct server_data_setskycolor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct server_data_setnatcolor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct server_data_setblock {
    int64_t x;
    int64_t z;
    int16_t y;
    struct blockdata data;
};

struct client_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    char* client_str;
};

struct client_data_newuid {
    uint64_t password;
};

struct client_data_login {
    uint8_t flags;
    uint64_t uid;
    uint64_t password;
    char* username;
};
enum {
    CLIENT_LOGIN_FLAG_CONONLY = 1 << 0,
};

struct client_data_getchunk {
    int64_t x;
    int64_t z;
};

struct client_data_setblock {
    int64_t x;
    int64_t z;
    int16_t y;
    struct blockdata data;
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char* /*addr*/, int /*port*/, int /*mcli*/, char* /*world*/);
void stopServer(void);

#if MODULEID == MODULEID_GAME

struct cliSetupInfo {
    struct {
        int (*quit)(void);
        void (*settext)(char*, float);
        struct {
            bool new;
            uint8_t flags;
            uint64_t uid;
            uint64_t password;
            char* username;
        } login;
        int timeout;
    } in;
    struct {
        struct {
            bool failed;
            uint64_t uid;
            char* username;
            char* failreason;
        } login;
        struct {
            struct {
                int major;
                int minor;
                int patch;
            } ver;
            uint8_t flags;
            char* name;
        } srv;
    } out;
};

bool cliConnect(char* /*addr*/, int /*port*/, bool (*/*cb*/)(int /*id*/, void* /*data*/));
int cliConnectAndSetup(char* /*addr*/, int /*port*/, bool (*/*cb*/)(int /*id*/, void* /*data*/), char* /*err*/, int /*errlen*/, struct cliSetupInfo* /*inf*/);
void cliDisconnect(void);
void cliSend(int /*id*/, /*data*/...);

#endif

#endif
