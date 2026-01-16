#ifndef KUBESRV_SERVER_H
#define KUBESRV_SERVER_H

#include <time.h>

typedef struct {
    char hostname[256];
    char message_buf[512];
    const char *message;
    time_t start_time;
    unsigned long requests;
    int port;
} server_ctx_t;

int server_init(server_ctx_t *ctx);
int server_run(server_ctx_t *ctx);

#endif
