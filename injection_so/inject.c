/*
* To change this license header, choose License Headers in Project Properties. * To change this template file, choose Tools | Templates* and open the template in the editor.*/
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

#define IOZONE_TEMP "./iozone.tmp"

struct cmd_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    uint32_t flag;  /* flag of command */
    __off64_t offset; /* seek offset */
    int whence;     /* seek whence */
    int32_t ret;   /* return value of ack command */
    char payload[0];
}__attribute__((packed));

enum cmd_type {
    OPEN = 11,
    CLOSE,
    LSEEK,
    WRITE,
    READ,
    UNLINK,
    STAT,
    ACK = 1
};

//#define SERV_ADDR "192.168.2.70"
#define SERV_ADDR "127.0.0.1"

#define SERV_PORT 9090

static int inj_sockfd = -1;

static uint32_t open_id = 0;
static uint32_t write_id = 0;
static uint32_t read_id = 0;
static uint32_t lseek_id = 0;
static uint32_t unlink_id = 0;
static uint32_t close_id = 0;

static char* inj_payload = NULL;

pthread_t tid = -1;
volatile bool read_ok = false;
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

static void *cmd_recv_thread(void *arg)
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
/*
        if (cmd.id == 17) {
            printf("17 recv not in loop, ACK = %d\n", cmd.type);
        }
*/
/*
        else if (nrecv == 0) {
            return 0;
        }
*/
        if (nrecv > 0) {
            if (cmd.type == ACK) {
                //printf("inj ack %d", cmd.ret);
//                printf("ACK detect");
                inj_ret = cmd.ret;
                if (cmd.len > 0) {
//                    printf("inj recv ACK READ cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));
                    inj_payload = malloc(cmd.len);
                    if (inj_payload == NULL) {
                        perror("inj malloc");
                    }

                    memset(inj_payload, 0, cmd.len);

                    nrecv = inj_read(inj_sockfd, inj_payload, cmd.len);
/*
                    if (cmd.id == 16) {
                        if (inj_payload != NULL) {
                            printf("nrecv = %d, cmd.len = %d, payload = %x\n", nrecv, cmd.len, inj_payload[0]);
                            int i;
                            for (i = 0; i < 100; i++) {
                                printf("%02x", inj_payload[i]);
                            }
                        }
                    }

                    if (cmd.id == 17) {
                        if (inj_payload != NULL) {
                            printf("nrecv = %d, cmd.len = %d, payload = %x\n", nrecv, cmd.len, inj_payload[0]);
                            int i;
                            for (i = 0; i < 100; i++) {
                                printf("%02x", inj_payload[i]);
                            }
                        }
                    }
*/
/*
                    if (inj_payload != NULL) {
                        free(inj_payload);
                    }
*/
                    if (nrecv == cmd.len) {
                        read_ok = true;
                    }
                }
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
        close(inj_sockfd);
        return -1;
    }
    printf("\nPRELOAD ok : inj_sockfd = %d\n", inj_sockfd);

    int err = 0;
    err = pthread_create(&tid, NULL, cmd_recv_thread, NULL);
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

    inj_msg->id = open_id;
    inj_msg->type = OPEN;
    inj_msg->len = len;
    inj_msg->flag = flag;// | O_LARGEFILE;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, (size_t) len);
    memcpy(inj_msg->payload, buf, (size_t) len);

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    if (inj_msg != NULL)
        free(inj_msg);

    int fd = -1;
    va_list args;
    va_start(args, flag);
    fd = open(file, flag | O_LARGEFILE, args);
    va_end(args);

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


ssize_t read(int fd, void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    read_id += 1;
    size_t len = size;

    inj_msg = calloc(1, sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

 //   char* buffer = (char*) buf;
    inj_msg->id = read_id;
    inj_msg->type = READ;
    inj_msg->len = len;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);
    while (!read_ok) {
        usleep(400);
    }
    memcpy(buf, inj_payload, len);
    //buf = inj_payload;
    read_ok = false;

    if (inj_payload != NULL)
        free(inj_payload);

    if (inj_msg != NULL)
        free(inj_msg);

    return size;
}

ssize_t write(int fd, const void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    write_id += 1;
    size_t len = size;

    inj_msg = calloc(1, sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    char* buffer = (char*) buf;
    inj_msg->id = write_id;
    inj_msg->type = WRITE;
    inj_msg->len = len;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, len);
    memcpy(inj_msg->payload, buffer, len);
    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    if (inj_msg != NULL)
        free(inj_msg);

    return size;
}


__off64_t lseek64 (int fd, __off64_t offset, int whence)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    lseek_id += 1;

    inj_msg = calloc(1, sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj calloc");
    }

    inj_msg->id = lseek_id;
    inj_msg->type = LSEEK;
    inj_msg->whence = whence;
    inj_msg->offset = offset;
    inj_msg->len = 0;
    inj_msg->flag = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));

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

    inj_msg->id = unlink_id;
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


