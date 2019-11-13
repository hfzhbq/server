/*
 * File :   libinj-jni.c
 * Author : baiqiang.hong
 * Usage : LD_PRELOAD="/root/jni_inj/dist/Debug/GNU-Linux/libjni_inj.so" java -cp /root/ Test
 * Debug : gdb --args env /root/jni_inj/dist/Debug/GNU-Linux/libjni_inj.so java -cp /root/ Test
 *
 * Created on November 11, 2019, 10:10 AM
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "injector.h"
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "bitops.h"

static struct cmd_t* inj_msg = NULL;
static struct sockaddr_in inj_servaddr;
static char* inj_payload = NULL;

static uint32_t write_id = 0;
static int write_ok = 0;
static int inj_sockfd = -1;
pthread_t tid = -1;
int inj_ret = 0;

static int (*dl_write)(int fd, const void *buf, size_t size) = NULL;


static void *cmd_parse_thread(void *arg)
{
    ssize_t nrecv = 0;
    while (1) {

        struct cmd_t cmd;

        memset(&cmd, 0, sizeof(cmd));

        nrecv = recv(inj_sockfd, &cmd, sizeof(cmd), 0);
        if (nrecv < 0) {
            perror("inj recv error");
            _exit(0);
        }

        if (nrecv > 0) {
            if (cmd.type == READ_ACK) {
/*
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
                if (len > 0) {
#ifdef INJ_DEBUG
                    printf("inj recv READ ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, read_ret = %d\n", cmd.type, id, len, sizeof(cmd), inj_ret);
#endif
                    inj_payload = calloc(1, len);
                    if (inj_payload == NULL) {
                        perror("inj malloc");
                    }

                    nrecv = inj_read(inj_sockfd, inj_payload, len);


                    if (nrecv == len) {
                        read_ok = 1;
                    }
                    else {
                        printf("nrecv = %d\n", nrecv);
                    }
                }
*/
            }
/*
            else if (cmd.type == LSEEK_ACK) {
                lseek64_ret = byteswap64(cmd.offset);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv LSEEK_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, ret = %x\n", cmd.type, id, len, sizeof(cmd), lseek64_ret);
#endif
                lseek_ok = 1;
            }
*/
            else if (cmd.type == WRITE_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv WRITE_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                write_ok = 1;
            }
/*
            else if (cmd.type == OPEN_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv OPEN_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                open_ok = 1;
            }
            else if (cmd.type == CREAT_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv CREAT_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                creat_ok = 1;
            }
            else if (cmd.type == UNLINK_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv UNLINK_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                unlink_ok = 1;
            }
            else if (cmd.type == FSYNC_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv FSYNC_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                fsync_ok = 1;
            }
            else if (cmd.type == CLOSE_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv CLOSE_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                close_ok = 1;
            }
            else if (cmd.type == OPEN_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv OPEN_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                open_ok = 1;
            }
            else if (cmd.type == FOPEN_ACK) {
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv FOPEN_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                fopen_ok = 1;
            }
*/
        }
    }
}


static int inj_socket_init()
{
    inj_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (inj_sockfd < 0) {
        printf("socket error");
        return -1;
    }

    bzero(&inj_servaddr, sizeof(inj_servaddr));
    inj_servaddr.sin_family = AF_INET;
    inj_servaddr.sin_port = htons(SERV_PORT);

    if (inet_pton(AF_INET, SERV_ADDR, &inj_servaddr.sin_addr) <= 0) {
        printf("inet_pton error");
        return -1;
    }

    if (connect(inj_sockfd, (struct sockaddr *)&inj_servaddr, sizeof(inj_servaddr)) < 0) {
        perror("connect error");
        //close(inj_sockfd);
        return -1;
    }
/*
    printf("\nPRELOAD ok : inj_sockfd = %d\n", inj_sockfd);
*/

    int err = 0;
    err = pthread_create(&tid, NULL, cmd_parse_thread, NULL);
    if (err != 0) {
        perror("phread create error");
        return -1;
    }
}

static ssize_t inj_write(int sockfd, const void *buf, size_t size)
{
    int nwrite = 0;
    int nleft = 0;
    const char *ptr;

    ptr = buf;
    nleft = size;
    while (nleft > 0) {

        nwrite = send(sockfd, ptr, nleft, MSG_DONTWAIT);

        if (nwrite <= 0) {
            if (errno == EAGAIN) {
                continue;
            }
            perror("inj send error");
        }
        nleft -= nwrite;
        ptr += nwrite;
    }

    return size;
}


ssize_t write(int fd, const void *buf, size_t size)
{

    printf("write op hook successfully\n");
    if (dl_write == NULL) {
        dl_write = dlsym(RTLD_NEXT, "write");
    }

    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    int cnt = 0;
    write_id += 1;
    size_t len = size;

    inj_msg = calloc(1, sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    char* buffer = (char*) buf;
    inj_msg->id = byteswap32(write_id);
    inj_msg->type = WRITE;
    inj_msg->len = byteswap32(len);
    inj_msg->fd = byteswap32(fd);
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, len);
    memcpy(inj_msg->payload, buffer, len);
    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    while (1) {
        usleep(200);

        if (write_ok == 1) {
            break;
        }
    }

    write_ok = 0;
    cnt = 0;

    if (inj_msg != NULL)
        free(inj_msg);

    return dl_write(fd, buf, size);
}


