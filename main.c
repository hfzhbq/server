/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   main.c
 * Author: root
 *
 * Created on July 3, 2019, 9:32 PM
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

struct cmd_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    uint32_t flag;  /* flag of command */
    __off64_t offset; /* seek offset */
    int whence;     /* seek whence */
    int32_t ret;   /* return value of ack command */
    uint8_t again; /* indicate the command is sent again */
    char payload[0];
}__attribute__((packed));

enum cmd_type {
    OPEN = 11,
    CREAT,
    CLOSE,
    LSEEK,
    WRITE,
    READ,
    UNLINK,
    STAT,
    READ_ACK = 1,
    WRITE_ACK,
    LSEEK_ACK,
    UNLINK_ACK
};

#define OPEN_LINE 20

#define RECV_LINE 10
#define SEND_LINE 10

#define	SA	struct sockaddr

static char recvbuff[RECV_LINE];
static char sendbuff[SEND_LINE];

int listenfd = -1;
int connfd = -1;
pid_t pid1 = -1;
pid_t pid2 = -1;
static char* io_payload = NULL;
static struct cmd_t* io_ack = NULL;

static uint32_t ack_read_id = 0;
static uint32_t ack_write_id = 0;
static uint32_t ack_lseek_id = 0;
static uint32_t ack_unlink_id = 0;

#define IOZONE_TEMP "./iozone.tmp"
int sock2fd = -1;

static struct cmd_t cmd;

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

int recv_handle(int sockfd)
{
    ssize_t nrecv = 0;
    while (1) {

        nrecv = recv(sockfd, recvbuff, sizeof(recvbuff), MSG_DONTWAIT);
        if (nrecv > 0) {
//            printf("nrecv : %d\n", nrecv);
        }
        else if (nrecv < 0) {
//            perror("recv error");

        }
    }
}

int io_parse_cmd(int sockfd)
{
    ssize_t nrecv = 0;
    while (1) {

        memset(&cmd, 0, sizeof(cmd));

        nrecv = recv(connfd, &cmd, sizeof(cmd), 0);
        if (nrecv < 0) {
            return 1;
        }
        else if (nrecv == 0) {
            return 0;
        }

        if (cmd.type == OPEN) {
            printf("server recv OPEN cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n, open flag = %8x\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.flag);
            io_payload = calloc(1, cmd.len);
            if (io_payload == NULL) {
                perror("ioserver malloc");
            }

            nrecv = recv(connfd, io_payload, cmd.len, 0);
            if (nrecv < 0) {
                perror("ioserver recv");
            }

            if (io_payload != NULL) {
                free(io_payload);
            }
            sock2fd = open64(IOZONE_TEMP, cmd.flag, 0644);

            if (sock2fd > 0) {
                printf("open success\n");
            }
            else if(sock2fd < 0) {
                perror("ioserver open");
            }
        }
        else if (cmd.type == CLOSE) {
            printf("server recv CLOSE cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));

            close(sock2fd);
        }
        else if (cmd.type == WRITE) {
            printf("server recv WRITE cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
            ack_write_id += 1;
            io_payload = calloc(1, cmd.len);
            if (io_payload == NULL) {
                perror("ioserver malloc");
            }

/*
            nrecv = recv(connfd, io_payload, cmd.len, 0);

            if (nrecv != cmd.len) {
                printf("break\n");
                break;
            }
*/

            nrecv = inj_read(connfd, io_payload, cmd.len);

            if (nrecv < 0) {
                perror("ioserver recv");
            }
            else {
                printf("nrecv : %d\n", nrecv);
                io_ack = calloc(1, sizeof(cmd));
                if (io_ack == NULL) {
                    perror("ioserver malloc");
                }

                io_ack->type = WRITE_ACK;
                io_ack->id = ack_write_id;
                io_ack->ret = write(sock2fd, io_payload, nrecv);

                if (io_ack->ret < 0) {
                    perror("ioserver read");
                }

                int nsend = 0;
                nsend = send(connfd, io_ack, sizeof(cmd), 0);
                if (nsend < 0) {
                    perror("ioserver send");
                }

                if (io_ack != NULL) {
                    free(io_ack);
                }
            }
//            fputs(payload, stdout);
            if (io_payload != NULL) {
                free(io_payload);
            }
        }
        else if (cmd.type == READ) {
            printf("server recv READ cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
            ack_read_id += 1;
            if (cmd.len > 0) {
                io_ack = calloc(1, sizeof(cmd) + cmd.len);
                if (io_ack == NULL) {
                    perror("ioserver malloc");
                }

                io_ack->type = READ_ACK;
                io_ack->id = ack_read_id;
                io_ack->len = cmd.len;

                io_ack->ret = read(sock2fd, io_ack->payload, cmd.len);

                if (io_ack->ret < 0) {
                    perror("ioserver read");
                }
                int nsend = 0;
                nsend = send(connfd, io_ack, sizeof(cmd) + cmd.len, 0);
                if (nsend < 0) {
                    perror("ioserver send");
                }

                if (io_ack != NULL) {
                    free(io_ack);
                }
            }
        }
        else if (cmd.type == LSEEK) {
            printf("server recv LSEEK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, whence = %#x, offset = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.whence, cmd.offset, cmd.again);

            ack_lseek_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver malloc");
            }

            io_ack->type = LSEEK_ACK;
            io_ack->id = ack_lseek_id;
            io_ack->ret = lseek(sock2fd, cmd.offset, cmd.whence);

            if (io_ack->ret < 0) {
                perror("ioserver lseek");
            }
            int nsend = 0;
            nsend = send(connfd, io_ack, sizeof(cmd), 0);
            if (nsend < 0) {
                perror("ioserver send");
            }

            if (io_ack != NULL) {
                free(io_ack);
            }
        }
        else if (cmd.type == UNLINK) {
            printf("server recv UNLINK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));

            ack_unlink_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver malloc");
            }

            io_ack->type = UNLINK_ACK;

            io_ack->ret = unlink(IOZONE_TEMP);
            if (io_ack->ret < 0) {
                perror("ioserver unlink");
            }

            int nsend = 0;
            nsend = send(connfd, io_ack, sizeof(cmd), 0);
            if (nsend < 0) {
                perror("ioserver send");
            }

            if (io_ack != NULL) {
                free(io_ack);
            }
        }
        else if (cmd.type == CREAT) {
            printf("server recv CREAT cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));

            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver malloc");
            }

            io_ack->type = CREAT;

            sock2fd = creat(IOZONE_TEMP, cmd.flag);
            if (sock2fd > 0) {
                printf("creat success\n");
            }
            else if(sock2fd < 0) {
                perror("ioserver creat");
            }
        }
    }
}

void ioserver_stop(int signo)
{
    printf("server stop\n");
    close(connfd);
    close(listenfd);
    close(sock2fd);

/*
    if (io_payload != NULL)
        free(io_payload);
*/

    if (pid1 > 0) {
        waitpid(pid1, NULL, 0);
    }
    else if (pid2 > 0) {
        waitpid(pid2, NULL, 0);
    }
    exit(0);
}

int main(int argc, char** argv)
{

    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket error");
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(9090);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    listen(listenfd, SOMAXCONN);

    bzero(recvbuff, RECV_LINE);

    memset(sendbuff, 'm', sizeof(sendbuff));

    signal(SIGINT, ioserver_stop);

    while (1) {

        connfd = accept(listenfd, (SA*)NULL, NULL);

        if (connfd < 0) {
            printf("Error: %d\n", strerror(errno));
            return 1;
        }

        printf("Connection is established\n");

        pid1 = fork();
        if (pid1 == 0) {
            close(listenfd);
            io_parse_cmd(connfd);

            exit(0);
        }
        else if (pid1 < 0) {
            perror("fork");
        }
        else {

        }
    }

    return (EXIT_SUCCESS);
}

