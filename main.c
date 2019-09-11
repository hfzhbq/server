/*
 * File:   injector.c
 * Author: Baiqiang Hong
 * Usage: ./ioserver
 */
#define _GNU_SOURCE
//#define SOLARIS
#define DEBUG

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
/**
 * keep the copy of injector.h in this file, because I can only upload one file
 * to solais and compile only one file.
 */

#ifdef DEBUG
#define IOSERV_DEBUG
#endif

struct cmd_t {
    uint8_t  type;  /* packet type */
    uint32_t id;    /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;   /* the length of payload */
    uint32_t flag;  /* flag of command */
#ifdef SOLARIS
    off64_t offset;
#else
    __off64_t offset; /* seek offset */
#endif
    int whence;     /* seek whence */
    int32_t ret;   /* return value of ack command */
    uint8_t again; /* indicate that the command is sent again */
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
    FSYNC,
    STAT,
    READ_ACK = 1,
    WRITE_ACK,
    OPEN_ACK,
    CREAT_ACK,
    CLOSE_ACK,
    LSEEK_ACK,
    UNLINK_ACK,
    FSYNC_ACK
};

#define	SA	struct sockaddr

int listenfd = -1;
int connfd = -1;
pid_t pid = -1;

static char* io_payload = NULL;
static struct cmd_t* io_ack = NULL;

static uint32_t ack_read_id = 0;
static uint32_t ack_write_id = 0;
static uint32_t ack_open_id = 0;
static uint32_t ack_creat_id = 0;
static uint32_t ack_lseek_id = 0;
static uint32_t ack_unlink_id = 0;
static uint32_t ack_fsync_id = 0;
static uint32_t ack_close_id = 0;
/**
 * even though lseek is called unsuccessfully, but it also will respond. the iozone also call lseek unsuccessfully on reverse read phase.
 */
static uint32_t lseek_rec_id = 0;
static uint32_t fsync_rec_id = 0;
static uint32_t read_rec_id = 0;

#define IOZONE_TEMP "./iozone.tmp"
int sock2fd = -1;

static struct cmd_t cmd;

#ifdef IOSERV_DEBUG
static struct timespec start, end;
static double elapsed;
#endif

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
#ifdef IOSERV_DEBUG
            printf("server recv OPEN cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d\n, open flag = %8x, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.flag, cmd.again);
#endif
            ack_open_id += 1;

            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = OPEN_ACK;
            io_ack->id = ack_open_id;
            sock2fd = open64(IOZONE_TEMP, cmd.flag, 0644);

            if (sock2fd > 0) {
#ifdef IOSERV_DEBUG
                printf("open64 success\n");
#endif
            }
            else if(sock2fd < 0) {
                perror("ioserver open");
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
        else if (cmd.type == CLOSE) {
#ifdef IOSERV_DEBUG
            printf("server recv CLOSE cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_close_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = CLOSE_ACK;
            io_ack->id = ack_close_id;

            io_ack->ret = close(sock2fd);
            if (io_ack->ret == 0) {
#ifdef IOSERV_DEBUG
                printf("close success\n");
#endif
            }
            else {
//                printf("server recv CLOSE cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
/*
                printf("sock2fd : %d\n", sock2fd);
                perror("ioserver close");
*/
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

        else if (cmd.type == WRITE) {
#ifdef IOSERV_DEBUG
            printf("server recv WRITE cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_write_id += 1;
            io_payload = calloc(1, cmd.len);
            if (io_payload == NULL) {
                perror("ioserver calloc");
            }

            nrecv = inj_read(connfd, io_payload, cmd.len);

            if (nrecv < 0) {
                perror("ioserver recv");
            }
            else {

                io_ack = calloc(1, sizeof(cmd));
                if (io_ack == NULL) {
                    perror("ioserver calloc");
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

            if (io_payload != NULL) {
                free(io_payload);
            }
        }
        else if (cmd.type == READ) {
#ifdef IOSERV_DEBUG
            printf("server recv READ cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_read_id += 1;
            if (cmd.len > 0) {
                io_ack = calloc(1, sizeof(cmd) + cmd.len);
                if (io_ack == NULL) {
                    perror("ioserver calloc");
                }

                io_ack->type = READ_ACK;
                io_ack->id = ack_read_id;
                io_ack->len = cmd.len;

                if (read_rec_id != cmd.id) {
#ifdef IOSERV_DEBUG
                    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
                    io_ack->ret = read(sock2fd, io_ack->payload, cmd.len);
#ifdef IOSERV_DEBUG
                    clock_gettime(CLOCK_MONOTONIC, &end);
                    elapsed = (end.tv_sec - start.tv_sec);
                    elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;
                    printf("the read time is %f seconds\n\r", elapsed);
#endif
                }

                if (io_ack->ret < 0) {
                    perror("ioserver read");
                }
                else {
                    read_rec_id = cmd.id;
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
#ifdef IOSERV_DEBUG
            printf("server recv LSEEK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, whence = %#x, offset = %llx, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.whence, cmd.offset, cmd.again);
#endif
            ack_lseek_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = LSEEK_ACK;
            io_ack->id = ack_lseek_id;
            io_ack->offset = lseek(sock2fd, cmd.offset, cmd.whence);
            if (io_ack->offset > 0) {
#ifdef IOSERV_DEBUG
                printf("lseek success\n");
#endif
            }
            else if(io_ack->offset < 0) {
//                printf("server recv LSEEK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, whence = %#x, offset = %x, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.whence, cmd.offset, cmd.again);
//                printf("sock2fd : %d\n", sock2fd);
//                perror("ioserver lseek");
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
#ifdef IOSERV_DEBUG
            printf("server recv UNLINK cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_unlink_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = UNLINK_ACK;
            io_ack->id = ack_unlink_id;
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
#ifdef IOSERV_DEBUG
            printf("server recv CREAT cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_creat_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = CREAT_ACK;
            io_ack->id = ack_creat_id;

            sock2fd = creat(IOZONE_TEMP, cmd.flag);
            if (sock2fd > 0) {
#ifdef IOSERV_DEBUG
                printf("creat success\n");
#endif
            }
            else if(sock2fd < 0) {
                perror("ioserver creat");
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
        else if (cmd.type == FSYNC) {
#ifdef IOSERV_DEBUG
            printf("server recv FSYNC cmd: type = %d, id = %d, payload_len = %d, msg_header_size = %d, again = %d\n", cmd.type, cmd.id, cmd.len, sizeof(cmd), cmd.again);
#endif
            ack_fsync_id += 1;
            io_ack = calloc(1, sizeof(cmd));
            if (io_ack == NULL) {
                perror("ioserver calloc");
            }

            io_ack->type = FSYNC_ACK;
            io_ack->id = ack_fsync_id;

 //           if (fsync_rec_id != cmd.id) {
                io_ack->ret = fsync(sock2fd);
//            }

            if (io_ack->ret == 0) {
#ifdef IOSERV_DEBUG
                printf("fsync success\n");
#endif
//                fsync_rec_id = cmd.id;
            }
            else {
                perror("ioserver fsync");
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
    }
}

void ioserver_stop(int signo)
{
    printf("server stop\n");
    close(connfd);
    close(listenfd);
    close(sock2fd);

    if (pid > 0) {
        unlink(IOZONE_TEMP);
        waitpid(pid, NULL, 0);
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

    signal(SIGINT, ioserver_stop);

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
            io_parse_cmd(connfd);

            exit(0);
        }
        else if (pid < 0) {
            perror("fork");
        }
        else {

        }
    }

    return (EXIT_SUCCESS);
}

