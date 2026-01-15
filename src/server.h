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
    unsigned long inflight;
    /* per-path request counters */
    unsigned long req_root;
    unsigned long req_healthz;
    unsigned long req_ready;
    unsigned long req_info;
    unsigned long req_identity;
    unsigned long req_echo;
    unsigned long req_fail;
    unsigned long req_sleep;
    unsigned long req_metrics;
    unsigned long req_rollout;
    unsigned long req_other;
    /* per-code failure counters */
    unsigned long fail_500;
    unsigned long fail_503;
    unsigned long fail_404;
    unsigned long fail_other;
    int port;
    int ready;
    int ready_delay;
    int shutdown_delay_ms;
    int fail_every_n;
} server_ctx_t;

int server_init(server_ctx_t *ctx);
int server_run(server_ctx_t *ctx);

#endif
