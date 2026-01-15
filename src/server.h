#ifndef KUBESRV_SERVER_H
#define KUBESRV_SERVER_H

#include <time.h>

typedef struct {
    char hostname[256];
    char pod_name[256];
    char pod_namespace[256];
    char pod_ip[64];
    char node_name[256];
    const char *message;
    time_t start_time;
    unsigned long requests;
    unsigned long failures;
    int port;
    int ready;
} server_ctx_t;

int server_init(server_ctx_t *ctx);
int server_run(server_ctx_t *ctx);

#endif
