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


#define RECV_LINE 2048
#define SEND_LINE 2048
#define	SA	struct sockaddr
static char recvbuff[RECV_LINE];
static char sendbuff[SEND_LINE];

int io_handle(int sockfd)
{
    ssize_t nread = 0;
    ssize_t nwrite = 0;

    while (1) {

        nread = recv(sockfd, recvbuff, sizeof(recvbuff), MSG_DONTWAIT);
        if (nread > 0) {
            printf("nrecv : %d\n");
        }

        nwrite = send(sockfd, sendbuff, sizeof(sendbuff), MSG_DONTWAIT);
        if (nread > 0) {
            printf("nsend : %d\n");
        }
    }
}

int main(int argc, char** argv)
{
    int listenfd = -1;
    int connfd = -1;
    pid_t pid = -1;

    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket error");
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(9090);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    listen(listenfd, 5);

    bzero(recvbuff, RECV_LINE+1);

    memset(sendbuff, 'm', sizeof(sendbuff));

    while (1) {

        connfd = accept(listenfd, (SA*)NULL, NULL);

        if (connfd < 0) {
            printf("Error: %d\n", strerror(errno));
            return 1;
        }

        printf("Connection is established\n");

        pid = fork();
        if (pid == 0) {
            close(listenfd);

            //nwrite = write(connfd, sendbuff, sizeof(sendbuff));
            io_handle(connfd);
            exit(0);
        }
        else if (pid < 0) {
            perror("fork");
        }
        else {
            /* keep a long connection, close func will result in client receive a
             * SIGPIPE.
             */
            // close(connfd);
            //avoid zombie process
           //waitpid(pid, NULL, 0);
        }
    }

    return (EXIT_SUCCESS);
}

