#include "http.h"
#include "config.h"
#include "generated/static_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int parse_query_param(const char *path, const char *key, char *value, size_t size) {
    const char *q = strchr(path, '?');
    if (!q) return 0;
    
    char *kv = strstr(q, key);
    if (!kv) return 0;
    
    kv += strlen(key);
    if (*kv != '=') return 0;
    kv++;
    
    const char *end = strchr(kv, '&');
    size_t len = end ? (size_t)(end - kv) : strlen(kv);
    if (len >= size) len = size - 1;
    
    memcpy(value, kv, len);
    value[len] = '\0';
    return 1;
}

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

    /* simple header parsing: iterate lines after first CRLF */
    const char *hdrs = strstr(end, "\r\n");
    if (!hdrs) return;
    hdrs += 2;
    int hcount = 0;
    while (*hdrs && hcount < KUBESRV_MAX_HEADERS) {
        const char *line_end = strstr(hdrs, "\r\n");
        if (!line_end) break;
        if (line_end == hdrs) { /* empty line */
            break;
        }
        const char *colon = memchr(hdrs, ':', (size_t)(line_end - hdrs));
        if (colon && (colon > hdrs)) {
            size_t nlen = (size_t)(colon - hdrs);
            size_t vlen = (size_t)(line_end - colon - 1);
            if (nlen > 0 && vlen > 0) {
                if (nlen >= sizeof(req->headers[hcount].name)) nlen = sizeof(req->headers[hcount].name) - 1;
                memcpy(req->headers[hcount].name, hdrs, nlen);
                req->headers[hcount].name[nlen] = '\0';
                const char *vstart = colon + 1;
                while (*vstart == ' ' && vlen > 0) { vstart++; vlen--; }
                if (vlen >= sizeof(req->headers[hcount].value)) vlen = sizeof(req->headers[hcount].value) - 1;
                memcpy(req->headers[hcount].value, vstart, vlen);
                req->headers[hcount].value[vlen] = '\0';
                hcount++;
            }
        }
        hdrs = line_end + 2;
    }
    req->header_count = hcount;
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
        "kubesrv_requests_total{path=\"/\"} %lu\n"
        "kubesrv_requests_total{path=\"/healthz\"} %lu\n"
        "kubesrv_requests_total{path=\"/ready\"} %lu\n"
        "kubesrv_requests_total{path=\"/info\"} %lu\n"
        "kubesrv_requests_total{path=\"/identity\"} %lu\n"
        "kubesrv_requests_total{path=\"/echo\"} %lu\n"
        "kubesrv_requests_total{path=\"/fail\"} %lu\n"
        "kubesrv_requests_total{path=\"/sleep\"} %lu\n"
        "kubesrv_requests_total{path=\"/metrics\"} %lu\n"
        "kubesrv_requests_total{path=\"/rollout\"} %lu\n"
        "kubesrv_requests_total{path=\"other\"} %lu\n"
        "# HELP kubesrv_failures_total Total failures\n"
        "# TYPE kubesrv_failures_total counter\n"
        "kubesrv_failures_total %lu\n"
        "kubesrv_failures_total{code=\"500\"} %lu\n"
        "kubesrv_failures_total{code=\"503\"} %lu\n"
        "kubesrv_failures_total{code=\"404\"} %lu\n"
        "kubesrv_failures_total{code=\"other\"} %lu\n"
        "# HELP kubesrv_inflight_requests Inflight requests\n"
        "# TYPE kubesrv_inflight_requests gauge\n"
        "kubesrv_inflight_requests %lu\n"
        "# HELP kubesrv_uptime_seconds Uptime\n"
        "# TYPE kubesrv_uptime_seconds gauge\n"
        "kubesrv_uptime_seconds %ld\n",
        ctx->requests,
        ctx->req_root, ctx->req_healthz, ctx->req_ready, ctx->req_info,
        ctx->req_identity, ctx->req_echo, ctx->req_fail, ctx->req_sleep,
        ctx->req_metrics, ctx->req_rollout, ctx->req_other,
        ctx->failures, ctx->fail_500, ctx->fail_503, ctx->fail_404, ctx->fail_other,
        ctx->inflight,
        uptime);
}

static int build_body_identity(server_ctx_t *ctx, char *buf, size_t size) {
    return snprintf(buf, size,
        "{\"pod\":\"%s\","
        "\"namespace\":\"%s\","
        "\"node\":\"%s\","
        "\"ip\":\"%s\","
        "\"hostname\":\"%s\"}\n",
        ctx->pod_name, ctx->pod_namespace, 
        ctx->node_name, ctx->pod_ip, ctx->hostname);
}

static int build_body_echo(const http_request_t *req, const char *client_ip, 
char *buf, size_t size) {
char headers[2048];
char *p = headers;
*p = '\0';
for (int i = 0; i < req->header_count; i++) {
int n = snprintf(p, (size_t)(headers + sizeof(headers) - p),
"\"%s\":\"%s\"%s",
req->headers[i].name, req->headers[i].value,
(i == req->header_count - 1) ? "" : ",");
if (n < 0) break;
p += n;
if ((size_t)(p - headers) >= sizeof(headers)) break;
}
return snprintf(buf, size,
"{\"method\":\"%s\"," 
"\"path\":\"%s\"," 
"\"client_ip\":\"%s\","
"\"headers\":{%s}}\n",
req->method, req->path, client_ip, headers);
}

static int build_body_ready(server_ctx_t *ctx, char *buf, size_t size) {
    long uptime = time(NULL) - ctx->start_time;
    if (uptime < ctx->ready_delay) {
        return snprintf(buf, size, "Not Ready (warming up: %ld/%d)\n", 
                       uptime, ctx->ready_delay);
    }
    return snprintf(buf, size, "Ready\n");
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

int http_build_response(const http_request_t *req, server_ctx_t *ctx,
                        char *buf, size_t size) {
    const char *status;
    const char *ctype;
    char body[KUBESRV_BUFFER_SIZE];
    char param[64];
    int blen;
    long uptime;
    
    if (strcmp(req->path, "/healthz") == 0) {
        ctx->req_healthz++;
        status = "200 OK";
        ctype = "text/plain";
        blen = build_body_healthz(body, sizeof(body));
    } else if (strncmp(req->path, "/ready", 6) == 0) {
        ctx->req_ready++;
        uptime = time(NULL) - ctx->start_time;
        if (uptime < ctx->ready_delay) {
            status = "503 Service Unavailable";
        } else {
            status = "200 OK";
        }
        ctype = "text/plain";
        blen = build_body_ready(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/info") == 0) {
        ctx->req_info++;
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_info(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/rollout") == 0) {
        ctx->req_rollout++;
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_info(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/identity") == 0) {
        ctx->req_identity++;
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_identity(ctx, body, sizeof(body));
    } else if (strncmp(req->path, "/echo", 5) == 0) {
        ctx->req_echo++;
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_echo(req, req->client_ip, body, sizeof(body));
    } else if (strncmp(req->path, "/fail", 5) == 0) {
        ctx->req_fail++;
        int do_fail = 0;
        if (parse_query_param(req->path, "rate", param, sizeof(param))) {
            /* deterministic rate: map rate to every Nth request */
            double rate = atof(param);
            if (rate > 0.0 && rate <= 1.0) {
                unsigned long n = (unsigned long)(1.0 / rate);
                if (n == 0) n = 1;
                if (n > 0 && (ctx->requests % n == 0)) do_fail = 1;
            }
        }
        if (parse_query_param(req->path, "code", param, sizeof(param))) {
            int code = atoi(param);
            if (code >= 400 && code < 600) {
                if (do_fail || ctx->fail_every_n > 0) {
                    unsigned long n = ctx->fail_every_n;
                    if (do_fail || (n > 0 && (ctx->requests % n == 0))) {
                        ctx->failures++;
                        if (code == 500) { status = "500 Internal Server Error"; ctx->fail_500++; }
                        else if (code == 503) { status = "503 Service Unavailable"; ctx->fail_503++; }
                        else if (code == 404) { status = "404 Not Found"; ctx->fail_404++; }
                        else { status = "500 Internal Server Error"; ctx->fail_other++; }
                        ctype = "text/plain";
                        blen = snprintf(body, sizeof(body), "Simulated failure: %d\n", code);
                    } else {
                        status = "200 OK";
                        ctype = "text/plain";
                        blen = snprintf(body, sizeof(body), "OK\n");
                    }
                } else {
                    /* no rate provided; fail immediately with given code */
                    ctx->failures++;
                    if (code == 500) { status = "500 Internal Server Error"; ctx->fail_500++; }
                    else if (code == 503) { status = "503 Service Unavailable"; ctx->fail_503++; }
                    else if (code == 404) { status = "404 Not Found"; ctx->fail_404++; }
                    else { status = "500 Internal Server Error"; ctx->fail_other++; }
                    ctype = "text/plain";
                    blen = snprintf(body, sizeof(body), "Simulated failure: %d\n", code);
                }
            } else {
                status = "400 Bad Request";
                ctype = "text/plain";
                blen = snprintf(body, sizeof(body), "Invalid code\n");
            }
        } else {
            int code = 500;
            if (do_fail || (ctx->fail_every_n > 0 && (ctx->requests % (unsigned long)ctx->fail_every_n == 0))) {
                ctx->failures++;
                ctx->fail_500++;
                status = "500 Internal Server Error";
                ctype = "text/plain";
                blen = snprintf(body, sizeof(body), "Simulated failure\n");
            } else {
                status = "200 OK";
                ctype = "text/plain";
                blen = snprintf(body, sizeof(body), "OK\n");
            }
        }
    } else if (strncmp(req->path, "/sleep", 6) == 0) {
        ctx->req_sleep++;
        if (parse_query_param(req->path, "ms", param, sizeof(param))) {
            int ms = atoi(param);
            if (ms > 0 && ms <= 10000) {
                usleep(ms * 1000);
            }
        }
        status = "200 OK";
        ctype = "text/plain";
        blen = snprintf(body, sizeof(body), "OK\n");
    } else if (strcmp(req->path, "/metrics") == 0) {
        ctx->req_metrics++;
        status = "200 OK";
        ctype = "text/plain; version=0.0.4";
        blen = build_body_metrics(ctx, body, sizeof(body));
    } else if (strcmp(req->path, "/") == 0) {
        ctx->req_root++;
        status = "200 OK";
        ctype = "text/html; charset=utf-8";
        blen = build_body_index(ctx, body, sizeof(body));
    } else {
        ctx->req_other++;
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
