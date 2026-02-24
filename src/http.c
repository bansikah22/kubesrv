#include "http.h"
#include "config.h"
#include "generated/static_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    char uptime_str[32];
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

static void json_escape(char *dest, const char *src, size_t dest_size) {
    if (!src) {
        strncpy(dest, "N/A", dest_size - 1);
        dest[dest_size - 1] = '\0';
        return;
    }

    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < dest_size - 1) {
        if (src[i] == '"' || src[i] == '\\') {
            if (j + 1 < dest_size - 1) {
                dest[j++] = '\\';
            } else {
                break;
            }
        }
        dest[j++] = src[i++];
    }
    dest[j] = '\0';
}

static int build_body_k8s(char *buf, size_t size) {
    char pod_name_esc[256], namespace_esc[256], node_name_esc[256], pod_ip_esc[256], sa_esc[256], hostname_esc[256];

    json_escape(pod_name_esc, getenv("POD_NAME"), sizeof(pod_name_esc));
    json_escape(namespace_esc, getenv("POD_NAMESPACE"), sizeof(namespace_esc));
    json_escape(node_name_esc, getenv("NODE_NAME"), sizeof(node_name_esc));
    json_escape(pod_ip_esc, getenv("POD_IP"), sizeof(pod_ip_esc));
    json_escape(sa_esc, getenv("SERVICE_ACCOUNT"), sizeof(sa_esc));

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }
    json_escape(hostname_esc, hostname, sizeof(hostname_esc));

    return snprintf(buf, size,
        "{\"pod_name\":\"%s\","
        "\"namespace\":\"%s\","
        "\"node_name\":\"%s\","
        "\"pod_ip\":\"%s\","
        "\"service_account\":\"%s\","
        "\"hostname\":\"%s\"}\n",
        pod_name_esc, namespace_esc, node_name_esc, pod_ip_esc, sa_esc, hostname_esc);
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
    } else if (strcmp(req->path, "/debug/k8s") == 0) {
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_k8s(body, sizeof(body));
        if (blen >= sizeof(body)) {
            blen = sizeof(body) - 1;
        }
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
