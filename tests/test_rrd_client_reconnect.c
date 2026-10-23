/*
 * When a send to rrdcached fails, the client connects again to the
 * address it has stored. If that name resolves to several addresses and
 * an early one refuses the connection, the client freed the stored
 * address on the way, so rrd_client_address() returned NULL and every
 * later call that sends a file name failed.
 *
 * This program makes "rrdtest-multi" resolve to 127.0.0.2 (nothing
 * listens there) followed by 127.0.0.1 (the daemon), by defining its own
 * getaddrinfo()/freeaddrinfo(), which take the place of the C library's
 * for librrd on ELF systems. Where they do not, it exits with 77 (skip).
 */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "rrd.h"
#include "rrd_client.h"

static int failures = 0;
static int gai_calls = 0;

static void check(
    int ok,
    const char *what)
{
    if (ok) {
        printf("OK: %s\n", what);
    } else {
        printf("FAILED: %s%s%s\n", what,
               rrd_test_error()? ": " : "",
               rrd_test_error()? rrd_get_error() : "");
        failures++;
    }
    rrd_clear_error();
}

/* a list of IPv4 stream addresses, one node per address */
static struct addrinfo *make_list(
    const char **ips,
    int n,
    int port)
{
    struct addrinfo *head = NULL, **tail = &head;
    int       i;

    for (i = 0; i < n; i++) {
        struct addrinfo *ai = calloc(1, sizeof(*ai));
        struct sockaddr_in *sin = calloc(1, sizeof(*sin));

        if (ai == NULL || sin == NULL) {
            free(ai);
            free(sin);
            break;
        }
        sin->sin_family = AF_INET;
        sin->sin_port = htons((unsigned short) port);
        inet_pton(AF_INET, ips[i], &sin->sin_addr);
        ai->ai_family = AF_INET;
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = IPPROTO_TCP;
        ai->ai_addrlen = sizeof(*sin);
        ai->ai_addr = (struct sockaddr *) sin;
        *tail = ai;
        tail = &ai->ai_next;
    }
    return head;
}

int getaddrinfo(
    const char *node,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res)
{
    static const char *multi[] = { "127.0.0.2", "127.0.0.1" };
    struct in_addr a;

    (void) hints;
    gai_calls++;
    if (node == NULL || service == NULL)
        return EAI_NONAME;
    if (strcmp(node, "rrdtest-multi") == 0)
        *res = make_list(multi, 2, atoi(service));
    else if (inet_pton(AF_INET, node, &a) == 1)
        *res = make_list(&node, 1, atoi(service));
    else
        return EAI_NONAME;
    return *res == NULL ? EAI_MEMORY : 0;
}

void freeaddrinfo(
    struct addrinfo *res)
{
    while (res != NULL) {
        struct addrinfo *next = res->ai_next;

        free(res->ai_addr);
        free(res);
        res = next;
    }
}

/* the socket of the client: the one whose peer is 127.0.0.1:port */
static int find_client_socket(
    int port)
{
    int       fd;

    for (fd = 3; fd < 1024; fd++) {
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);

        if (getpeername(fd, (struct sockaddr *) &sin, &len) == 0
            && sin.sin_family == AF_INET
            && ntohs(sin.sin_port) == port
            && sin.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
            return fd;
    }
    return -1;
}

int main(
    int argc,
    char **argv)
{
    char      addr[64];
    rrd_client_t *client;
    int       port, fd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
        return 2;
    }
    port = atoi(argv[1]);
    snprintf(addr, sizeof(addr), "rrdtest-multi:%d", port);
    signal(SIGPIPE, SIG_IGN);

    client = rrd_client_new(NULL);
    if (client == NULL) {
        printf("FAILED: rrd_client_new\n");
        return 1;
    }
    check(rrd_client_connect(client, addr) == 0,
          "connect, skipping the address nobody listens on");
    if (gai_calls == 0) {
        printf("getaddrinfo() of this program was not used by librrd\n");
        return 77;
    }
    check(rrd_client_ping(client), "ping");

    /* make the next send fail, so the client connects again */
    fd = find_client_socket(port);
    check(fd >= 0, "find the client socket");
    if (fd >= 0)
        shutdown(fd, SHUT_WR);

    check(rrd_client_ping(client), "ping after connecting again");
    check(rrd_client_is_connected(client), "connected after connecting again");
    check(rrd_client_address(client) != NULL
          && strcmp(rrd_client_address(client), addr) == 0,
          "address kept after connecting again");

    rrd_client_destroy(client);
    return failures ? 1 : 0;
}
