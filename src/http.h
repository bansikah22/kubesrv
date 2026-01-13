#ifndef KUBESRV_HTTP_H
#define KUBESRV_HTTP_H

#include "server.h"
#include <stddef.h>

typedef struct {
    char method[8];
    char path[256];
} http_request_t;

void http_parse_request(const char *raw, http_request_t *req);
int http_build_response(const http_request_t *req, server_ctx_t *ctx, 
                        char *buf, size_t size);

#endif
