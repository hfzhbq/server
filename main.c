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

struct msg_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    char* payload;
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
        struct msg_t msg;
        memset(&msg, 0, sizeof(msg));

        nrecv = recv(connfd, &msg, sizeof(msg), 0);
        if (nrecv < 0) {
            return 1;
        }
        else if (nrecv == 0) {
            return 0;
        }
        if (msg.type == 11) {
            printf("recv open mesg: type = %d, id = %d, len = %d, msg_size = %d\n", msg.type, msg.id, msg.len, sizeof(msg));
        }
    }
}

void ioserver_stop(int signo)
{
    printf("server stop\n");
    close(connfd);
    close(listenfd);
    close(sock2fd);

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

    sock2fd = open(IOZONE_TEMP, O_CREAT | O_RDWR, 0644);
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

