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
    uint8_t  type;  /* cmd type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    int32_t flag;  /* flag of command */
    int32_t ret;   /* return value of commnd */
    char payload[0];
}__attribute__((packed));

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
//static char* io_payload2 = NULL;

#define IOZONE_TEMP "./iozone.tmp"
int sock2fd = -1;

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

int recv_msg(int sockfd)
{
    ssize_t nrecv = 0;
    while (1) {

        struct cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));

        nrecv = recv(connfd, &cmd, sizeof(cmd), 0);
        if (nrecv < 0) {
            return 1;
        }
        else if (nrecv == 0) {
            return 0;
        }

        if (cmd.type == 11) {
            printf("server recv open mesg: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));
            io_payload = malloc(cmd.len);
            if (io_payload == NULL) {
                perror("ioserver malloc");
            }
            memset(io_payload, 0, (size_t) cmd.len);
            nrecv = recv(connfd, io_payload, cmd.len, 0);
            if (nrecv < 0) {
                perror("ioserver recv");
            }
            fputs(io_payload, stdout);

            if (io_payload != NULL) {
                free(io_payload);
            }

            sock2fd = open(IOZONE_TEMP, O_CREAT | O_RDWR, 0644);
        }
        else if (cmd.type == 12) {
            printf("server recv write mesg: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));
            io_payload = malloc(cmd.len);
            if (io_payload == NULL) {
                perror("ioserver malloc");
            }
            memset(io_payload, 0, (size_t) cmd.len);
            nrecv = recv(connfd, io_payload, cmd.len, 0);
            if (nrecv < 0) {
                perror("ioserver recv");
            }
            else {
                printf("nrecv : %d\n", nrecv);
                write(sock2fd, io_payload, nrecv);
            }
//            fputs(payload, stdout);
            if (io_payload != NULL) {
                free(io_payload);
            }
        }
        else if (cmd.type == 13) {

        }
        else if (cmd.type == 14) {

        }
        else if (cmd.type == 15) {
            printf("server recv unlink mesg: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd));
            struct cmd_t ack;
            memset(&ack, 0, sizeof(ack));
            ack.type = 10;
            ack.id = 0;
            ack.len = 0;
            ack.flag = 0;
            ack.ret = unlink(IOZONE_TEMP);
            ack.payload[0] = NULL;

            int nsend = 0;
            nsend = send(connfd, &ack, sizeof(ack), 0);
            if (nsend < 0) {
                perror("ioserver send");
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
    if (io_payload != NULL)
        free(io_payload);

    if (pid1 > 0) {
        waitpid(pid1, NULL, 0);
    }
    else if (pid2 > 0) {
        waitpid(pid2, NULL, 0);
    }
    _exit(0);
}

int send_handle(int sockfd)
{

    ssize_t nsend = 0;

    while (1) {

        nsend = send(sockfd, sendbuff, sizeof(sendbuff), MSG_DONTWAIT);
        if (nsend > 0) {
            printf("nsend : %d\n", nsend);
        }
        else if (nsend < 0) {
//            perror("nsend");
        }
    }
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
//            send_handle(connfd);
            exit(0);
        }
        else if (pid1 < 0) {
            perror("fork");
        }
        else {
            pid2 = fork();
            if (pid2 == 0) {
                close(listenfd);
                recv_msg(connfd);
                //recv_handle(connfd);
                exit(0);
            }
            else if (pid2 < 0){
                perror("fork");
            }
        }
    }

    return (EXIT_SUCCESS);
}

