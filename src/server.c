#include "server.h"
#include "http.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig) {
    if (sig == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
    } else {
        g_running = 0;
    }
}

static int setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) < 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) < 0) return -1;
    
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) return -1;
    
    return 0;
}

int server_init(server_ctx_t *ctx) {
    const char *env;
    FILE *fp;
    const char *config_file;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->start_time = time(NULL);
    ctx->requests = 0;
    ctx->port = KUBESRV_DEFAULT_PORT;
    
    if (gethostname(ctx->hostname, sizeof(ctx->hostname)) < 0) {
        strncpy(ctx->hostname, "unknown", sizeof(ctx->hostname) - 1);
    }
    
    env = getenv("PORT");
    if (env != NULL) {
        int p = atoi(env);
        if (p > 0 && p <= 65535) {
            ctx->port = p;
        }
    }
    
    /* Check for MESSAGE_FILE env var (can be full path or just filename) */
    config_file = getenv("MESSAGE_FILE");
    if (config_file != NULL) {
        /* Try to read from specified path */
        fp = fopen(config_file, "r");
        if (fp != NULL) {
            if (fgets(ctx->message_buf, sizeof(ctx->message_buf), fp) != NULL) {
                /* Remove trailing newline */
                size_t len = strlen(ctx->message_buf);
                if (len > 0 && ctx->message_buf[len - 1] == '\n') {
                    ctx->message_buf[len - 1] = '\0';
                }
                ctx->message = ctx->message_buf;
                fprintf(stdout, "[kubesrv] Loaded message from %s\n", config_file);
            }
            fclose(fp);
        }
    }
    
    /* Fall back to MESSAGE environment variable */
    if (ctx->message == NULL || ctx->message[0] == '\0') {
        env = getenv("MESSAGE");
        ctx->message = (env != NULL) ? env : "Hello, Kubernetes!";
    }
    
    return 0;
}

static void handle_client(int fd, const struct sockaddr_in *addr, server_ctx_t *ctx) {
    char buffer[KUBESRV_BUFFER_SIZE];
    char response[KUBESRV_BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];
    char timestamp[32];
    http_request_t req;
    ssize_t n;
    time_t now;
    struct tm *tm;
    int len;
    
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    
    n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(fd);
        _exit(0);
    }
    buffer[n] = '\0';
    
    http_parse_request(buffer, &req);
    ctx->requests++;
    
    len = http_build_response(&req, ctx, response, sizeof(response));
    write(fd, response, len);
    
    now = time(NULL);
    tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);
    
    fprintf(stdout, "[%s] %s %s %s\n", timestamp, ip, req.method, req.path);
    fflush(stdout);
    
    close(fd);
    _exit(0);
}

int server_run(server_ctx_t *ctx) {
    int sfd, cfd;
    int opt = 1;
    struct sockaddr_in saddr, caddr;
    socklen_t clen = sizeof(caddr);
    pid_t pid;
    
    if (setup_signals() < 0) {
        perror("sigaction");
        return 1;
    }
    
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return 1;
    }
    
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sfd);
        return 1;
    }
    
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(ctx->port);
    
    if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        close(sfd);
        return 1;
    }
    
    if (listen(sfd, KUBESRV_BACKLOG) < 0) {
        perror("listen");
        close(sfd);
        return 1;
    }
    
    fprintf(stdout, "[kubesrv] %s v%s on port %d\n", 
            ctx->hostname, KUBESRV_VERSION, ctx->port);
    fprintf(stdout, "[kubesrv] Endpoints: / /healthz /info /metrics\n");
    fflush(stdout);
    
    while (g_running) {
        cfd = accept(sfd, (struct sockaddr *)&caddr, &clen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        
        pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
        } else if (pid == 0) {
            close(sfd);
            handle_client(cfd, &caddr, ctx);
        } else {
            close(cfd);
        }
    }
    
    fprintf(stdout, "[kubesrv] Shutting down\n");
    close(sfd);
    return 0;
}
