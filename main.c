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


#define MAXLINE 1024

/*
 *
 */
int main(int argc, char** argv)
{
    int listenfd;
    int connfd;
    int ret;

    char buff[MAXLINE+1];
    struct sockaddr_in servaddr;
    time_t ticks;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("socket error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(9090);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    listen(listenfd, 3);

    while (1) {
        connfd = accept(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
        //ticks = time(NULL);
        snprintf(buff ,sizeof(buff), "%d\r\n", "123456");
        write(connfd, buff, strlen(buff));
        close(connfd);
    }


    return (EXIT_SUCCESS);
}

