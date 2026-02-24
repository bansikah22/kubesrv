#include "http.h"
#include "config.h"
#include "generated/static_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

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
    
    const char *query = strchr(p, '?');
    if (query != NULL && query < end) {
        len = query - p;
        if (len >= sizeof(req->path)) len = sizeof(req->path) - 1;
        memcpy(req->path, p, len);
        req->path[len] = '\0';

        len = end - (query + 1);
        if (len >= sizeof(req->query)) len = sizeof(req->query) - 1;
        memcpy(req->query, query + 1, len);
        req->query[len] = '\0';
    } else {
        len = end - p;
        if (len >= sizeof(req->path)) len = sizeof(req->path) - 1;
        memcpy(req->path, p, len);
        req->path[len] = '\0';
    }
}

static int get_query_param(const char *query, const char *key, char *value, size_t value_size) {
    const char *p = query;
    size_t key_len = strlen(key);

    while (p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *val_start = p + key_len + 1;
            const char *val_end = strchr(val_start, '&');
            size_t val_len;
            if (val_end) {
                val_len = val_end - val_start;
            } else {
                val_len = strlen(val_start);
            }

            if (val_len >= value_size) {
                val_len = value_size - 1;
            }
            memcpy(value, val_start, val_len);
            value[val_len] = '\0';
            return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return 0;
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

static int build_body_dns(const http_request_t *req, char *buf, size_t size) {
    char host[256];
    if (!get_query_param(req->query, "host", host, sizeof(host))) {
        return snprintf(buf, size, "{\"error\":\"host parameter is required\"}\n");
    }

    struct addrinfo hints, *res;
    char ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, NULL, &hints, &res);
    if (status != 0) {
        return snprintf(buf, size, "{\"host\":\"%s\",\"success\":false,\"error\":\"%s\"}\n", host, gai_strerror(status));
    }

    void *addr;
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        addr = &(ipv4->sin_addr);
    } else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        addr = &(ipv6->sin6_addr);
    }

    inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);
    freeaddrinfo(res);

    return snprintf(buf, size, "{\"host\":\"%s\",\"resolved_ip\":\"%s\",\"success\":true}\n", host, ipstr);
}

static int build_body_tcp(const http_request_t *req, char *buf, size_t size) {
    char host[256];
    char port_str[8];

    if (!get_query_param(req->query, "host", host, sizeof(host)) || !get_query_param(req->query, "port", port_str, sizeof(port_str))) {
        return snprintf(buf, size, "{\"error\":\"host and port parameters are required\"}\n");
    }

    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        return snprintf(buf, size, "{\"error\":\"invalid port\"}\n");
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        return snprintf(buf, size, "{\"host\":\"%s\",\"port\":%d,\"connected\":false,\"error\":\"dns resolution failed\"}\n", host, port);
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int result = connect(sockfd, res->ai_addr, res->ai_addrlen);

    gettimeofday(&end, NULL);
    long latency = ((end.tv_sec - start.tv_sec) * 1000) + ((end.tv_usec - start.tv_usec) / 1000);

    close(sockfd);
    freeaddrinfo(res);

    if (result == -1) {
        return snprintf(buf, size, "{\"host\":\"%s\",\"port\":%d,\"connected\":false,\"error\":\"connection failed\"}\n", host, port);
    }

    return snprintf(buf, size, "{\"host\":\"%s\",\"port\":%d,\"connected\":true,\"latency_ms\":%ld}\n", host, port, latency);
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
    } else if (strcmp(req->path, "/dns") == 0) {
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_dns(req, body, sizeof(body));
    } else if (strcmp(req->path, "/tcp") == 0) {
        status = "200 OK";
        ctype = "application/json";
        blen = build_body_tcp(req, body, sizeof(body));
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
