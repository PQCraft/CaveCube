#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <game/chunk.h>

#include <stdbool.h>
#include <stdarg.h>

#ifndef MAX_CLIENTS
    #define MAX_CLIENTS 256
#endif

#ifndef SERVER_SNDBUF_SIZE
    #define SERVER_SNDBUF_SIZE 262144
#endif

#ifndef CLIENT_SNDBUF_SIZE
    #define CLIENT_SNDBUF_SIZE -1
#endif

#ifndef SERVER_OUTBUF_SIZE
    #define SERVER_OUTBUF_SIZE (1 << 21)
#endif

#ifndef CLIENT_OUTBUF_SIZE
    #define CLIENT_OUTBUF_SIZE (1 << 16)
#endif

enum {
    SERVER_PONG,
    SERVER_COMPATINFO,
    SERVER_LOGININFO,
    SERVER_UPDATECHUNK,
    SERVER_UPDATECHUNKCOL,
};

enum {
    CLIENT_PING,
    CLIENT_COMPATINFO,
    CLIENT_LOGININFO,
    CLIENT_GETCHUNK,
    CLIENT_GETCHUNKCOL,
    CLIENT_SETCHUNKPOS,
};

enum {
    SERVER_FLAG_NOAUTH = 1 << 0,
    SERVER_FLAG_PASSWD = 1 << 1,
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, char*, int);
void stopServer(void);

bool cliConnect(char*, int, void (*)(int, ...));
void cliDisconnect(void);
void cliSend(int, ...);

#endif
