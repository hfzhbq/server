/*
 * File:   injector.h
 * Author: Baiqiang Hong
 *
 */
#ifndef __INJ_H__
#define __INJ_H__

#define IOZONE_TEMP "./iozone.tmp"

//#define SOLARIS
//#define DEBUG

#define SERV_PORT   9090

#ifdef SOLARIS
#define FLIP_ENDIAN
#define SERV_ADDR   "192.168.2.89"
#else
#define SERV_ADDR   "127.0.0.1"
//#define SERV_ADDR   "132.145.158.190"
#endif

#ifdef DEBUG
#define INJ_DEBUG
#endif

struct cmd_t {
    uint8_t  type;      /* packet type */
    uint32_t id;        /* id is from 1 to 0xFFFFFFFF, id is +1 for next request packet */
    uint32_t len;       /* the length of payload */
    uint32_t flag;      /* flag of command */
    char mode[2];       /* mode of fopen */
#ifdef SOLARIS
    off64_t offset;
#else
    __off64_t offset;   /* seek offset */
#endif
    int whence;         /* seek whence */
    int32_t ret;        /* return value of ack command */
    uint8_t again;      /* indicate that the command is sent again */
    char payload[0];
}__attribute__((packed));

enum cmd_type {
    OPEN = 1,
    CREAT,
    CLOSE,
    LSEEK,
    WRITE,
    READ,
    UNLINK,
    FSYNC,
    FOPEN,
    FWRITE,
    FREAD,
    FCLOSE,
    FFLUSH,
    FILENO,
    SETVBUF,
    READ_ACK = 20,
    WRITE_ACK,
    OPEN_ACK,
    CREAT_ACK,
    CLOSE_ACK,
    LSEEK_ACK,
    UNLINK_ACK,
    FSYNC_ACK,
    FOPEN_ACK,
};

#endif