/* Exercise the actual FETCH handlers with write-buffer allocation failures. */
#include "rrd_tool.h"
#include <stdlib.h>
#include <string.h>

static int fail_growth;
static int failures;
static void *test_realloc(void *ptr, size_t size)
{
    if (fail_growth) {
        failures++;
        return NULL;
    }
    return realloc(ptr, size);
}

#undef rrd_realloc
#define rrd_realloc test_realloc
int rrdcached_main(int argc, const char **argv);
#define main rrdcached_main
#include "../src/rrd_daemon.c"
#undef main
#undef rrd_realloc

int main(void)
{
    const char *args[] = {
        "DS:value:GAUGE:120:U:U", "RRA:AVERAGE:0.5:1:10"
    };
    char path[] = "/tmp/rrd-fetch-test.XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return 1;
    close(fd);
    unlink(path);
    if (rrd_create_r(path, 60, 900000000, 2, args) != 0) {
        fprintf(stderr, "create: %s\n", rrd_get_error());
        return 1;
    }
    cache_tree = g_tree_new_full(tree_compare_func, NULL, NULL, NULL);
    int injected[2] = {0, 0};
    for (int binary = 0; binary < 2; binary++) {
        for (size_t slack = 1; slack < 256; slack++) {
            listen_socket_t sock = {0};
            char command[512];
            int pair[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0)
                return 1;
            sock.fd = pair[0];
            sock.wbuf_data = malloc(4096);
            if (!sock.wbuf_data)
                return 1;
            sock.wbuf_capacity = 4096;
            sock.wbuf_size = 4096 - slack;
            memset(sock.wbuf_data, 'x', sock.wbuf_size);
            sock.wbuf_data[sock.wbuf_size] = 0;
            snprintf(command, sizeof(command), "%s AVERAGE 900000000 900000180", path);
            fail_growth = 1;
            failures = 0;
            int status = binary
                ? handle_request_fetchbin(NULL, &sock, 0, command, strlen(command) + 1)
                : handle_request_fetch(NULL, &sock, 0, command, strlen(command) + 1);
            fail_growth = 0;
            injected[binary] += failures;
            if ((failures && (status == 0 || failures != 1)) ||
                (!failures && status != 0)) {
                fprintf(stderr, "FETCH%s slack=%zu: status=%d, failures=%d\n",
                        binary ? "BIN" : "", slack, status, failures);
                return 1;
            }
            if (failures) {
                char byte;
                if (recv(pair[1], &byte, 1, MSG_DONTWAIT) != -1 ||
                    (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    fprintf(stderr, "partial response sent after allocation failure\n");
                    return 1;
                }
            }
            wbuf_free(&sock);
            close(pair[0]);
            close(pair[1]);
        }
    }
    g_tree_destroy(cache_tree);
    unlink(path);
    return injected[0] == 0 || injected[1] == 0;
}
