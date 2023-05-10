#ifndef _OPTS_H_
#define _OPTS_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define NUM 438473
#define DEFAULT_PSIZE 512
#define DISCOVER_ADDR "255.255.255.255"
#define DATA_PORT (20000 + (NUM % 10000))
#define CTRL_PORT (30000 + (NUM % 10000))
#define DEFAULT_BSIZE 65536
#define DEFAULT_RTIME 250
#define DEFAULT_NAME "Nienazwany Nadajnik"
#define MAX_NAME_LEN 64

struct sender_opts {
    /** address of targeted receiver (set with option -a, obligatory) */
    char dest_addr[20];

    /** data port (set with option -P) defaults to @p DATA_PORT */
    uint16_t port;

    /** audio_data size (set with -p) defaults to @p DEFAULT_PSIZE */
    uint32_t psize;

    /** port used for control protocol with receivers
     * set with option -C, defaults to @p DEFAULT_CTRL
     */
    uint16_t ctrl_port;

    /** time reserved for gathering missing packs reports from receivers
     * set with option -R, defaults to @p DEFAULT_RTIME
     */
    uint64_t rtime;

    /** sender name (set with -n) defaults to @p DEFAULT_NAME */
    char sender_name[64];
};

typedef struct sender_opts sender_opts;

struct receiver_opts {
    /** address of whitelisted sender
     * set with option -a, obligatory
     */
    char from_addr[20];

    /** data port
     * set with option -P, defaults to @p DATA_PORT
     */
    char portstr[12];
    uint16_t port;

    /** address used for control protocol with senders
     * set with option -d, defaults to @p DISCOVER_ADDR
     */
    char discover_addr[20];

    /** port used for control protocol with senders
     * set with option -C, defaults to @p CTRL_PORT
     */
    uint16_t ctrl_port;

    /** time between sending missing packs reports to senders
     * set with option -R, defaults to @p DEFAULT_RTIME
     */
    uint64_t rtime;

    /** buffer size (set with -b) defaults to @p DEFAULT_BSIZE */
    uint32_t bsize;
};

typedef struct receiver_opts receiver_opts;

inline static sender_opts *get_sender_opts(int argc, char **argv) {
    sender_opts *opts = malloc(sizeof(sender_opts));

    opts->psize = DEFAULT_PSIZE;
    opts->port = DATA_PORT;
    sprintf(opts->sender_name, "%s", DEFAULT_NAME);
    opts->ctrl_port = CTRL_PORT;
    opts->rtime = DEFAULT_RTIME;

    int aflag = 0;
    int errflag = 0;

    int c;
    int len;

    opterr = 0;

    while ((c = getopt(argc, argv, "a:n:p:P:C:R:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                memcpy(opts->dest_addr, optarg, strlen(optarg));
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
                    fprintf(stderr, "Invalid rtime: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'n':
                len = strlen(optarg);
                if (len > MAX_NAME_LEN) {
                    fprintf(stderr, "Name too long: %s\n", optarg);
                    errflag = 1;
                } else
                    memcpy(opts->sender_name, optarg, strlen(optarg));
                break;
            case 'p':
                opts->psize = strtoul(optarg, NULL, 10);
                if (opts->psize == 0) {
                    fprintf(stderr, "Invalid audio_pack size: %s\n", optarg);
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
                abort();
        }
    }

    if (aflag == 0) {
        fprintf(stderr, "Usage: ./sikradio-sender "
                        "-a <dest_address>\n");
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
    sprintf(opts->portstr, "%d", DATA_PORT);
    opts->ctrl_port = CTRL_PORT;
    opts->rtime = DEFAULT_RTIME;
    sprintf(opts->discover_addr, "%s", DISCOVER_ADDR);

    int aflag = 0;

    int errflag = 0;

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "a:b:P:d:C:R:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                memcpy(opts->from_addr, optarg, strlen(optarg));
                break;
            case 'd':
                memcpy(opts->discover_addr, optarg, strlen(optarg));
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
                    fprintf(stderr, "Invalid rtime: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'b':
                opts->bsize = strtoul(optarg, NULL, 10);
                if (opts->bsize == 0) {
                    fprintf(stderr, "Invalid buffer size: %s\n", optarg);
                    errflag = 1;
                }
                break;
            case 'P':
                if ((opts->port = strtoul(optarg, NULL, 10)) < 1024) {
                    fprintf(stderr, "Invalid or illegal port number: %s\n",
                            optarg);
                    errflag = 1;
                } else
                    memcpy(opts->portstr, optarg, strlen(optarg));
                break;
            case '?':
                if (optopt == 'a' || optopt == 'b' || optopt == 'P'
                    || optopt == 'd' || optopt == 'C' || optopt == 'R')
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
                abort();
        }
    }

    if (aflag == 0) {
        fprintf(stderr, "Usage: ./sikradio-receiver "
                        "-a <sender_address>\n");
        errflag = 1;
    }

    if (errflag == 1) {
        free(opts);
        exit(1);
    }

    return opts;
}

#endif // _OPTS_H_
