/*
 * File:   injector.c
 * Author: Baiqiang Hong
 *
 */
// LD_PRELOAD=./libinjection-so.so ./code-injection or export LD_PRELOAD=./libinjection-so.so
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#include <stdarg.h>
#include <signal.h>
#include <stdbool.h>
#include <common/bitops.h>
#include "injector.h"

static int inj_sockfd = -1;

static uint32_t open_id = 0;
static uint32_t creat_id = 0;
static uint32_t write_id = 0;
static uint32_t read_id = 0;
static uint32_t lseek_id = 0;
static uint32_t unlink_id = 0;
static uint32_t close_id = 0;

static char* inj_payload = NULL;

pthread_t tid = -1;
volatile int read_ok = 0;
volatile int lseek_ok = 0;
volatile int write_ok = 0;
int inj_ret = 0;

static struct cmd_t* inj_msg = NULL;

static struct sockaddr_in inj_servaddr;

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

static ssize_t inj_read(int sock_fd, void *buf, size_t size)
{
    int nread = 0;
    int nleft = size;
    char *ptr = (char *)buf;

    while (nleft > 0) {

        nread = recv(sock_fd, ptr, nleft, MSG_DONTWAIT);

        if (nread <= 0) {
            if (errno == EAGAIN) {
                continue;
            }
            perror("inj recv error");
        }
        nleft -= nread;
        ptr += nread;
    }

    return size - nleft;
}

static void *cmd_parse_thread(void *arg)
{
    ssize_t nrecv = 0;
    while (1) {

        struct cmd_t cmd;

        memset(&cmd, 0, sizeof(cmd));

        //nrecv = inj_read(inj_sockfd, &cmd, sizeof(cmd));
        nrecv = recv(inj_sockfd, &cmd, sizeof(cmd), 0);
        if (nrecv < 0) {
            perror("inj recv error");
            return -1;
        }

        if (nrecv > 0) {
            if (cmd.type == READ_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
                if (len > 0) {
#ifdef INJ_DEBUG
                    printf("inj recv ACK READ cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
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
            }
            else if (cmd.type == LSEEK_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv LSEEK_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                lseek_ok = 1;
            }
            else if (cmd.type == WRITE_ACK) {
                inj_ret = byteswap32(cmd.ret);
                int len = byteswap32(cmd.len);
                int id = byteswap32(cmd.id);
#ifdef INJ_DEBUG
                printf("inj recv WRITE_ACK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, id, len, sizeof(cmd));
#endif
                write_ok = 1;
            }
        }
    }
}

static void inj_stop(int signo)
{
    printf("inj client stop\n");
    if (inj_msg != NULL)
        free(inj_msg);
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
    printf("\nPRELOAD ok : inj_sockfd = %d\n", inj_sockfd);

    int err = 0;
    err = pthread_create(&tid, NULL, cmd_parse_thread, NULL);
    if (err != 0) {
        perror("phread create error");
        return -1;
    }
//    signal(SIGINT, inj_stop);
}


int open64(const char *file, int flag, ...)
{
 //   printf("open64 inj");
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    open_id += 1;
    char* buf = "123456789abcdefg\n";
    int len = (int) (strlen(buf) + 1);

    inj_msg = calloc(1, sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = byteswap32(open_id);
    inj_msg->type = OPEN;
    inj_msg->len = byteswap32(len);
    inj_msg->flag = byteswap32(flag);// | O_LARGEFILE;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, (size_t) len);
    memcpy(inj_msg->payload, buf, (size_t) len);

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    if (inj_msg != NULL)
        free(inj_msg);

    /* Using a fake file to let iozone run happily */
    int fd = -1;
    va_list args;
    va_start(args, flag);
    fd = open(file, flag | O_LARGEFILE, args);
    va_end(args);

    return fd;
}

int creat64 (const char *file, mode_t mode)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    creat_id += 1;

    inj_msg = calloc(1, sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = byteswap32(creat_id);
    inj_msg->type = CREAT;
    inj_msg->len = 0;
    inj_msg->flag = byteswap32(mode);// | O_LARGEFILE;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));

    if (inj_msg != NULL)
        free(inj_msg);

    /* Using a fake file to let iozone run happily */
    int fd = -1;
    //creat() is equivalent to open() with flags equal to O_CREAT|O_WRONLY|O_TRUNC.
    fd = creat(file, mode | O_LARGEFILE);

    return fd;
}

/*
int close(int fd)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    close_id += 1;

    inj_msg = calloc(1, sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = close_id;
    inj_msg->type = CLOSE;
    inj_msg->len = 0;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));

    if (inj_msg != NULL)
        free(inj_msg);

    return 0;
}
*/

/**
 * upstream : fd -> buf -> connection -> buf
 */

ssize_t read(int fd, void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    int cnt = 0;
    read_id += 1;
    size_t len = size;

    inj_msg = calloc(1, sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = byteswap32(read_id);
    inj_msg->type = READ;
    inj_msg->len = byteswap32(len);
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    /**
     * Sometimes, we send cmd to server, but server won't recv or recv
     * a error cmd mesg for some reasons, in this case, we have to add send
     *  cmd again. In order to figure out when this case occurs, add
     * flag to indicate this case happened and the cmd has been send again
     */

    while (1) {
        usleep(200);

        if (read_ok == 1) {
            break;
        }

        if (cnt > 100) {
            inj_msg->again += 1;
            inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);
            cnt = 0;
        }
        cnt += 1;
    }
    memcpy(buf, inj_payload, len);
    read_ok = 0;
    cnt = 0;

    if (inj_payload != NULL)
        free(inj_payload);

    if (inj_msg != NULL)
        free(inj_msg);

    return size;
}
/**
 *  downstream : buf -> connection -> buf -> fd
 */
ssize_t write(int fd, const void *buf, size_t size)
{
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
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, len);
    memcpy(inj_msg->payload, buffer, len);
    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    /**
     * Sometimes, we send cmd to server, but server won't recv or recv
     * a error cmd mesg for some reasons, in this case, we have to add send
     * cmd again. In order to figure out when this case occurs, add
     * flag to indicate this case happened and the cmd has been send again
     */
    while (1) {
        usleep(200);

        if (write_ok == 1) {
            break;
        }

        if (cnt > 100) {
            inj_msg->again += 1;
            inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);
            cnt = 0;
        }
        cnt += 1;
    }

    write_ok = 0;
    cnt = 0;

    if (inj_msg != NULL)
        free(inj_msg);

    return size;
}


__off64_t lseek64 (int fd, __off64_t offset, int whence)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }
    int cnt = 0;
    lseek_id += 1;

    inj_msg = calloc(1, sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = byteswap32(lseek_id);
    inj_msg->type = LSEEK;
    inj_msg->whence = byteswap32(whence);
    inj_msg->offset = byteswap32(offset);

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));

    /**
     * Sometimes, we send cmd to server, but server won't recv or recv
     * a error cmd mesg for some reasons, in this case, we have to add send
     * cmd again. In order to figure out when this case occurs, add
     * flag to indicate this case happened and the READ cmd has been send again
     */

    while (1) {
        usleep(200);

        if (lseek_ok == 1) {
            break;
        }
/**
 * At some point, we have to add deley to avoid the lseek cmd is excuted twice,
 * it will issue data verification error. In different system, the speed of system
 * is also different, so the problem severity is different.
 */
#ifdef SOLARIS
        if (cnt > 800) {
#else
        if (cnt > 300) {
#endif
            inj_msg->again += 1;
            inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));
            cnt = 0;
        }
        cnt += 1;
    }

    lseek_ok = 0;
    cnt = 0;
    if (inj_msg != NULL)
        free(inj_msg);

    return offset;
}

int unlink(const char *path)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    unlink_id += 1;

    inj_msg = calloc(1, sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = byteswap32(unlink_id);
    inj_msg->type = UNLINK;
    inj_msg->len = 0;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));

    if (inj_msg != NULL)
        free(inj_msg);

    char command[512] = {0};
    snprintf(command, sizeof(command), "rm -rf %s", path);
    system(command);
    return inj_ret;
}


