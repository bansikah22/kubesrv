#include "server.h"

int main(void) {
    server_ctx_t ctx;
    
    if (server_init(&ctx) < 0) {
        return 1;
    }
    
    return server_run(&ctx);
}
