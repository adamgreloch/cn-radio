#ifndef _OPTS_H_
#define _OPTS_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "receiver_config.h"

#define NUM 438473
#define DEFAULT_PSIZE 512
#define DISCOVER_ADDR "255.255.255.255"
#define UI_PORT   (10000 + (NUM % 10000))
#define DATA_PORT (20000 + (NUM % 10000))
#define CTRL_PORT (30000 + (NUM % 10000))
#define DEFAULT_BSIZE 65536
#define DEFAULT_FSIZE 131072
#define DEFAULT_RTIME 250
#define DEFAULT_NAME "Nienazwany Nadajnik"

struct sender_opts {
    /** address of targeted receiver (set with option -a, obligatory) */
    char mcast_addr_str[20];

    /** data port (set with option -P) defaults to @p DATA_PORT */
    uint16_t port;

    /** audio_data size (set with -p) defaults to @p DEFAULT_PSIZE */
    uint64_t psize;

    /** FIFO size (set with -f) defaults to @p DEFAULT_FSIZE */
    uint64_t fsize;

    /** port used for control protocol with receivers
     * set with option -C, defaults to @p DEFAULT_CTRL
     */
    uint16_t ctrl_port;

    /** time reserved for gathering missing packs reports from receivers
     * set with option -R, defaults to @p DEFAULT_RTIME
     */
    uint64_t rtime;

    /** sender name (set with -n) defaults to @p DEFAULT_NAME */
    char sender_name[MAX_NAME_LEN + 1];
};

typedef struct sender_opts sender_opts;

struct receiver_opts {
    /** address used for control protocol with senders
     * set with option -d, defaults to @p DISCOVER_ADDR
     */
    char discover_addr[20];

    /** port used for control protocol with senders
     * set with option -C, defaults to @p CTRL_PORT
     */
    char ctrl_portstr[12];
    uint16_t ctrl_port;

    /** port used for station switching UI over TCP
     * set with option -U, defaults to @p UI_PORT
     */
    uint16_t ui_port;

    /** time between sending missing packs reports to senders
     * set with option -R, defaults to @p DEFAULT_RTIME
     */
    uint64_t rtime;

    /** buffer size (set with -b) defaults to @p DEFAULT_BSIZE */
    uint64_t bsize;

    /** prioritized sender name (set with -n) defaults to '\0' (none) */
    char sender_name[MAX_NAME_LEN + 1];
};

typedef struct receiver_opts receiver_opts;

// TODO handle all incorrect args
// TODO add ANSI check for name, check for non-emptiness
// TODO add -n argument for receiver
// TODO refactor

inline static sender_opts *get_sender_opts(int argc, char **argv) {
    sender_opts *opts = malloc(sizeof(sender_opts));

    opts->psize = DEFAULT_PSIZE;
    opts->port = DATA_PORT;
    sprintf(opts->sender_name, "%s", DEFAULT_NAME);
    opts->ctrl_port = CTRL_PORT;
    opts->rtime = DEFAULT_RTIME;
    opts->fsize = DEFAULT_FSIZE;

    int aflag = 0;
    int errflag = 0;

    int c;
    int len;

    opterr = 0;

    while ((c = getopt(argc, argv, "a:n:p:P:C:R:f:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                memset(opts->mcast_addr_str, 0, sizeof(opts->mcast_addr_str));
                memcpy(opts->mcast_addr_str, optarg, strlen(optarg));
                break;
            case 'C':
                if ((opts->ctrl_port = strtoul(optarg, NULL, 10)) < 1024) {
                    fprintf(stderr,
                            "Invalid or illegal control port number: %s\n",
                            optarg);
                    errflag = 1;
                }
                break;
            case 'R':
                opts->rtime = strtoul(optarg, NULL, 10);
                if (opts->rtime == 0) {
                    fprintf(stderr, "Invalid rtime_u: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'n':
                len = strlen(optarg);
                if (len > MAX_NAME_LEN) {
                    fprintf(stderr, "Name too long: %s\n", optarg);
                    errflag = 1;
                } else {
                    memset(opts->sender_name, 0, sizeof(opts->sender_name));
                    memcpy(opts->sender_name, optarg, strlen(optarg));
                }
                break;
            case 'p':
                opts->psize = strtoull(optarg, NULL, 10);
                if (opts->psize == 0) {
                    fprintf(stderr, "Invalid audio_pack size: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'f':
                opts->fsize = strtoull(optarg, NULL, 10);
                if (opts->fsize == 0) {
                    fprintf(stderr, "Invalid FIFO size: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'P':
                opts->port = strtoul(optarg, NULL, 10);
                if (opts->port < 1024) {
                    fprintf(stderr, "Invalid or illegal port number: %s\n",
                            optarg);
                    errflag = 1;
                }
                break;
            case '?':
                if (optopt == 'a' || optopt == 'p' ||
                    optopt == 'P' || optopt == 'n' || optopt == 'C' ||
                    optopt == 'R')
                    fprintf(stderr, "Option -%c requires an argument.\n",
                            optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                free(opts);
                exit(1);
            default:
                exit(1);
        }
    }

    if (aflag == 0) {
        fprintf(stderr, "Usage: ./sikradio-sender "
                        "-a <mcast_addr>\n");
        errflag = 1;
    }

    if (errflag == 1) {
        free(opts);
        exit(1);
    }

    return opts;
}

inline static receiver_opts *get_receiver_opts(int argc, char **argv) {
    receiver_opts *opts = malloc(sizeof(receiver_opts));

    opts->bsize = DEFAULT_BSIZE;
    opts->ctrl_port = CTRL_PORT;
    sprintf(opts->ctrl_portstr, "%d", CTRL_PORT);
    opts->rtime = DEFAULT_RTIME;
    sprintf(opts->discover_addr, "%s", DISCOVER_ADDR);
    opts->ui_port = UI_PORT;
    opts->sender_name[0] = '\0';

    int errflag = 0;

    int c;
    int len;

    opterr = 0;

    size_t port;

    while ((c = getopt(argc, argv, "n:b:d:C:R:U:")) != -1) {
        switch (c) {
            case 'd':
                memset(opts->discover_addr, 0, sizeof(opts->discover_addr));
                memcpy(opts->discover_addr, optarg, strlen(optarg));
                break;
            case 'C':
                memset(opts->ctrl_portstr, 0, sizeof(opts->ctrl_portstr));
                memcpy(opts->ctrl_portstr, optarg, strlen(optarg));
                port = strtoull(optarg, NULL, 10);
                if (port < (1 << 10) || port > (1 << 16)) {
                    fprintf(stderr,
                            "Invalid or illegal control port number: %s\n",
                            optarg);
                    errflag = 1;
                } else opts->ctrl_port = port;
                break;
            case 'U':
                port = strtoull(optarg, NULL, 10);
                if (port < (1 << 10) || port > (1 << 16)) {
                    fprintf(stderr,
                            "Invalid or illegal UI port number: %s\n",
                            optarg);
                    errflag = 1;
                } else opts->ui_port = port;
                break;
            case 'R':
                opts->rtime = strtoull(optarg, NULL, 10);
                if (opts->rtime == 0) {
                    fprintf(stderr, "Invalid rtime_u: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'b':
                opts->bsize = strtoull(optarg, NULL, 10);
                if (opts->bsize == 0) {
                    fprintf(stderr, "Invalid buffer size: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'n':
                len = strlen(optarg);
                if (len > MAX_NAME_LEN) {
                    fprintf(stderr, "Name too long: %s\n", optarg);
                    errflag = 1;
                } else {
                    memset(opts->sender_name, 0, sizeof(opts->sender_name));
                    memcpy(opts->sender_name, optarg, strlen(optarg));
                }
                break;
            case '?':
                if (optopt == 'b' || optopt == 'd' || optopt == 'C' ||
                    optopt == 'R'
                    || optopt == 'U')
                    fprintf(stderr, "Option -%c requires an argument.\n",
                            optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                free(opts);
                exit(1);
            default:
                exit(1);
        }
    }

    if (errflag == 1) {
        free(opts);
        exit(1);
    }

    return opts;
}

#endif // _OPTS_H_
