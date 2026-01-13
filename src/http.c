#include "http.h"
#include "config.h"
#include "generated/static_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void http_parse_request(const char *raw, http_request_t *req) {
    const char *p, *end;
    size_t len;
    
    memset(req, 0, sizeof(*req));
    strcpy(req->method, "GET");
    strcpy(req->path, "/");
    
    p = raw;
    end = strchr(p, ' ');
    if (end == NULL) return;
    
    len = end - p;
    if (len >= sizeof(req->method)) len = sizeof(req->method) - 1;
    memcpy(req->method, p, len);
    req->method[len] = '\0';
    
    p = end + 1;
    end = strchr(p, ' ');
    if (end == NULL) return;
    
    len = end - p;
    if (len >= sizeof(req->path)) len = sizeof(req->path) - 1;
    memcpy(req->path, p, len);
    req->path[len] = '\0';
}

static int build_body_healthz(char *buf, size_t size) {
    return snprintf(buf, size, "OK\n");
}

static int build_body_info(server_ctx_t *ctx, char *buf, size_t size) {
    char ts[32];
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    long uptime = now - ctx->start_time;
    
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    
    return snprintf(buf, size,
        "{\"hostname\":\"%s\","
        "\"version\":\"%s\","
        "\"uptime\":%ld,"
        "\"requests\":%lu,"
        "\"timestamp\":\"%s\","
        "\"message\":\"%s\"}\n",
        ctx->hostname, KUBESRV_VERSION, uptime, 
        ctx->requests, ts, ctx->message);
}

static int build_body_metrics(server_ctx_t *ctx, char *buf, size_t size) {
    long uptime = time(NULL) - ctx->start_time;
    
    return snprintf(buf, size,
        "# HELP kubesrv_requests_total Total requests\n"
        "# TYPE kubesrv_requests_total counter\n"
        "kubesrv_requests_total %lu\n"
        "# HELP kubesrv_uptime_seconds Uptime\n"
        "# TYPE kubesrv_uptime_seconds gauge\n"
        "kubesrv_uptime_seconds %ld\n",
        ctx->requests, uptime);
}

static int build_body_index(server_ctx_t *ctx, char *buf, size_t size) {
    long uptime = time(NULL) - ctx->start_time;
    char uptime_str[16];
    char out[KUBESRV_BUFFER_SIZE];
    char *dst = out;
    const char *src = INDEX_HTML;
    
    snprintf(uptime_str, sizeof(uptime_str), "%02ld:%02ld:%02ld",
             uptime / 3600, (uptime % 3600) / 60, uptime % 60);
    
    while (*src != '\0') {
        if (strncmp(src, "{{HOSTNAME}}", 12) == 0) {
            dst += sprintf(dst, "%s", ctx->hostname);
            src += 12;
        } else if (strncmp(src, "{{VERSION}}", 11) == 0) {
            dst += sprintf(dst, "%s", KUBESRV_VERSION);
            src += 11;
        } else if (strncmp(src, "{{UPTIME}}", 10) == 0) {
            dst += sprintf(dst, "%s", uptime_str);
            src += 10;
        } else if (strncmp(src, "{{REQUESTS}}", 12) == 0) {
            dst += sprintf(dst, "%lu", ctx->requests);
            src += 12;
        } else if (strncmp(src, "{{MESSAGE}}", 11) == 0) {
            dst += sprintf(dst, "%s", ctx->message);
            src += 11;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    return snprintf(buf, size, "%s", out);
}

int http_build_response(const http_request_t *req, server_ctx_t *ctx,
                        char *buf, size_t size) {
    const char *status;
    const char *ctype;
    char body[KUBESRV_BUFFER_SIZE];
    int blen;
    
    if (strcmp(req->path, "/healthz") == 0) {
        status = "200 OK";
        ctype = "text/plain";
        blen = build_body_healthz(body, sizeof(body));
    } else if (strcmp(req->path, "/info") == 0) {
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_info(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/metrics") == 0) {
        status = "200 OK";
        ctype = "text/plain; version=0.0.4";
        blen = build_body_metrics(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/") == 0) {
        status = "200 OK";
        ctype = "text/html; charset=utf-8";
        blen = build_body_index(ctx, body, sizeof(body));
    } else {
        status = "404 Not Found";
        ctype = "text/plain";
        blen = snprintf(body, sizeof(body), "Not Found\n");
    }
    
    return snprintf(buf, size,
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Server: kubesrv/%s\r\n"
        "X-Hostname: %s\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        status, ctype, blen, KUBESRV_VERSION, ctx->hostname, body);
}
