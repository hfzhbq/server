/*
* To change this license header, choose License Headers in Project Properties. * To change this template file, choose Tools | Templates* and open the template in the editor.*/
// LD_PRELOAD=./libinjection-so.so ./code-injection or export LD_PRELOAD=./libinjection-so.so
#define _GNU_SOURCE
#define HEAD_SIZE
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

#define IOZONE_TEMP "./iozone.tmp"

struct cmd_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    int32_t flag;  /* flag of command */
    int32_t ret;   /* return value of commnd */
    char payload[0];
}__attribute__((packed));

typedef enum {
    OPEN,
    CLOSE,
    LSEEK,
    WRITE,
    READ,
    UNLINK,
    STAT
} INJ_OP;

//#define SERVER_ADDR "192.168.2.69"
#define SERV_ADDR "127.0.0.1"

#define SERV_PORT 9090

static int inj_sockfd = -1;

static uint32_t open_id = 0;
static uint32_t write_id = 0;
static uint32_t read_id = 0;
static uint32_t lseek_id = 0;
static uint32_t unlink_id = 0;

static struct cmd_t* inj_msg = NULL;

static struct sockaddr_in inj_servaddr;

static void inj_stop(int signo)
{
    printf("inj client stop\n");
    if (inj_msg != NULL)
        free(inj_msg);
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

static ssize_t inj_read(int sock_fd, void *buf, size_t size)
{
    int nread = 0;
    int nleft = size;
    unsigned char *ptr = (char *)buf;

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

    inj_msg = malloc(sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj malloc");
    }

    inj_msg->id = open_id;
    inj_msg->type = 11;
    inj_msg->len = len;
    inj_msg->flag = flag;
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


ssize_t read(int fd, void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    read_id += 1;
    size_t len = size;

    inj_msg = malloc(sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj malloc");
    }

 //   char* buffer = (char*) buf;
    inj_msg->id = read_id;
    inj_msg->type = 13;
    inj_msg->len = 0;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);

    inj_read(inj_sockfd, buf, size);

    if (inj_msg != NULL)
        free(inj_msg);
}

ssize_t write(int fd, const void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    write_id += 1;
    size_t len = size;

    inj_msg = malloc(sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj malloc");
    }

    char* buffer = (char*) buf;
    inj_msg->id = write_id;
    inj_msg->type = 12;
    inj_msg->len = len;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, len);
    memcpy(inj_msg->payload, buffer, len);
    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t) + len);
    printf("");
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
    char* buf = "lseek\n";
    int len = (int) (strlen(buf));

    inj_msg = malloc(sizeof(struct cmd_t) + len);
    if (inj_msg == NULL) {
        perror("inj malloc");
    }

    inj_msg->id = lseek_id;
    inj_msg->type = 14;
    inj_msg->len = len;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    memset(inj_msg->payload, 0, (size_t) len);
    memcpy(inj_msg->payload, buf, (size_t) len);

    return offset;
}

int unlink(const char *path)
{
    int ret;

    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    unlink_id += 1;

    inj_msg = malloc(sizeof(struct cmd_t));
    if (inj_msg == NULL) {
        perror("inj malloc");
    }

    inj_msg->id = unlink_id;
    inj_msg->type = 15;
    inj_msg->len = 0;
    inj_msg->flag = 0;
    inj_msg->ret = 0;
    inj_msg->payload[0] = 0;

    inj_write(inj_sockfd, inj_msg, sizeof(struct cmd_t));
    memset(inj_msg, 0, sizeof(struct cmd_t));
    inj_read(inj_sockfd, inj_msg, sizeof(struct cmd_t));
    if (inj_msg->type == 10) {
        ret = inj_msg->ret;
//        printf("inj ack %d", ret);
    }


    if (inj_msg != NULL)
        free(inj_msg);

    char command[512] = {0};
    snprintf(command, sizeof(command), "rm -rf %s", path);
    system(command);
    return ret;
}


