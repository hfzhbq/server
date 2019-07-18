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

struct msg_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    char* payload;
}__attribute__((packed));

typedef enum {
    OPEN,
    CLOSE,
    LSEEK,
    WRITE,
    READ
} INJ_OP;

//#define SERVER_ADDR "192.168.2.69"
#define SERV_ADDR "127.0.0.1"

#define SERV_PORT 9090

static int inj_sockfd = -1;

static uint32_t open_id = 0;
static uint32_t write_id = 1;

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
}


int open64 (const char *file, int flag, ...)
{
 //   printf("open64 inj");
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    open_id += 1;
    struct msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = 11;
    msg.id = open_id;
    msg.len = 0;

    char* buf = "123456789abcdefg";

    msg.payload= buf;

//    msg.payload = NULL;
    inj_write(inj_sockfd, &msg, sizeof(msg));

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

    int nread = 0;
    int nleft = size;
    unsigned char *ptr = (char *)buf;

    while (nleft > 0) {

        nread = recv(inj_sockfd, ptr, nleft, MSG_DONTWAIT);

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


/*
ssize_t write(int fd, const void *buf, size_t size)
{
    if (inj_sockfd == -1) {
        inj_socket_init();
    }

    write_id += 1;
    struct msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = 12;
    msg.id = write_id;
    msg.len = size;

    int i;

    for (i = 0; i < size; i++) {
        msg.payload[i] = buf[i];
    }

    inj_write(inj_sockfd, msg, sizeof(msg));

    return size;
}
*/

