#include "client.h"
#include <arpa/inet.h>

void compare_results(pico_time __attribute__((unused)) now, void __attribute__((unused)) *arg)
{
#ifdef CONSISTENCY_CHECK /* TODO: Enable */
    int i;
    printf("Calculating result.... (%p)\n", buffer1);

    if (memcmp(buffer0, buffer1, TCPSIZ) == 0)
        exit(0);

    for (i = 0; i < TCPSIZ; i++) {
        if (buffer0[i] != buffer1[i]) {
            fprintf(stderr, "Error at byte %d - %c!=%c\n", i, buffer0[i], buffer1[i]);
            exit(115);
        }
    }
#endif
    exit(0);

}

static char *buffer1[1024];
static char *buffer0 = "abcdefghijklmnopqrstuvwxyz123456";
static int sent = 0;
//#define TCPSIZ (1024 * 1024 * 5)
#define TCPSIZ (16)
//#define INFINITE_TCPTEST

void cb_tcpclient(uint16_t ev, struct pico_socket *s)
{
    static int w_size = 0;
    static int r_size = 0;
    static int closed = 0;
    int r, w;
    static unsigned long count = 0;

    count++;
    printf("tcpclient> wakeup %lu, event %u\n", count, ev);

    if (ev & PICO_SOCK_EV_RD) {
        do {
            r = pico_socket_read(s, buffer1 + r_size, 16);
            if (r > 0) {
                printf("SOCKET READ - %d\n", r);
                printf("Packet payload received to offset %d\n", r_size);
                int i;
                for (i = 0; i < r; i++) {
                    printf("%c", ((uint8_t *)(buffer1 + r_size))[i]);
                }
                printf("\n");
                r_size += r;
            }

            if (r < 0) {
                printf("!!EXIT!! PICO_SOCK_EV_RD: %d\n", r);
                exit(5);
            }
        } while(r > 0);
    }

    if (ev & PICO_SOCK_EV_CONN) {
        printf("Connection established with server.\n");
    }

    if (ev & PICO_SOCK_EV_FIN) {
        printf("Socket closed. Exit normally. \n");
        if (!pico_timer_add(2000, compare_results, NULL)) {
            printf("Failed to start exit timer, exiting now\n");
            exit(1);
        }
    }

    if (ev & PICO_SOCK_EV_ERR) {
        printf("Socket error received: %s. Bailing out.\n", strerror(pico_err));
        exit(1);
    }

    if (ev & PICO_SOCK_EV_CLOSE) {
        printf("Socket received close from peer - Wrong case if not all client data sent!\n");
        pico_socket_close(s);
        return;
    }

    if (ev & PICO_SOCK_EV_WR) {
       if (sent < 2) {
            w = pico_socket_write(s, buffer0 + w_size, 16);
            if (w > 0) {
                printf("SOCKET WRITTEN - %d\n", w);
                printf("Packet payload written ");
                int i;
                for (i = 0; i< w; i++) {
                    printf("%c", (buffer0 + w_size)[i]);
                }
                printf("\n");

                w_size += w;
                if (w < 0)
                    exit(5);
            }
            sent++;
        }
        else {
#ifdef INFINITE_TCPTEST
            w_size = 0;
            return;
#endif
            if (!closed) {
                pico_socket_shutdown(s, PICO_SHUT_WR);
                printf("Called shutdown()\n");
                closed = 1;
            }
        }
    }
}

int connect_client(struct pico_socket *s, struct pico_ip4* remote, uint16_t remote_port, uint16_t *listen_port) {
    s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &cb_tcpclient);
    struct pico_ip4 inaddr = {0};

    uint16_t listen_port_temp = htons(*listen_port);
    int ret = pico_socket_bind(s, &inaddr, &listen_port_temp);
    if (ret < 0) {
        printf("%s: error binding socket to port %u: %s\n", __FUNCTION__, short_be(*listen_port), strerror(pico_err));
        return ret;
    }

    ret = pico_socket_connect(s, remote, htons(remote_port));
    if (ret != 0) {
        printf("%s: error connecting to port %u\n", __FUNCTION__, short_be(remote_port));
        return ret;
    }
    return ret;
}

#undef TCPSIZ
