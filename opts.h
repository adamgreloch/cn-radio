#ifndef _OPTS_H_
#define _OPTS_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "err.h"
#include "common.h"
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

inline static int parse_string_from_opt(char *dest, size_t dest_size) {
    if (strlen(optarg) > dest_size) {
        fprintf(stderr,
                "Invalid argument: -%c %s\n", optopt,
                optarg);
        return 1;
    }
    memset(dest, 0, dest_size);
    memcpy(dest, optarg, strlen(optarg));
    return 0;
}

inline static int check_opt_for_numeric() {
    if (!is_number(optarg)) {
        fprintf(stderr, "Argument must be numeric/positive: %s\n", optarg);
        return 1;
    }
    return 0;
}

inline static int parse_name_from_opt(char *dest, size_t dest_size) {
    if (parse_string_from_opt(dest, dest_size) == 1) return 1;

    if (strlen(optarg) == 0) {
        fprintf(stderr, "Name cannot be empty.\n");
        return 1;
    }

    if (isblank(dest[0]) || isblank(dest[strlen(optarg) - 1])) {
        fprintf(stderr,
                "Name cannot start/end with spaces: %s\n", optarg);
        return 1;
    }

    for (size_t i = 0; i < strlen(optarg); i++)
        if (dest[i] < 32) {
            fprintf(stderr,
                    "Illegal character '%c' in name: %s\n", dest[i], optarg);
            return 1;
        }

    return 0;
}

inline static int parse_num_from_opt(uint64_t *dest, bool nonzero) {
    if (check_opt_for_numeric() == 1) return 1;
    errno = 0;
    uint64_t n = strtoull(optarg, NULL, 10);
    if ((nonzero && n == 0) || errno != 0) {
        fprintf(stderr,
                "Invalid argument: %s\n", optarg);
        return 1;
    }
    *dest = n;
    return 0;
}

inline static int parse_port_from_opt(uint16_t *dest) {
    if (check_opt_for_numeric() == 1) return 1;
    errno = 0;
    uint64_t n = strtoull(optarg, NULL, 10);
    if (n == 0 || n > 65535 || errno != 0) {
        fprintf(stderr,
                "Invalid or illegal port number: %s\n", optarg);
        return 1;
    }
    *dest = n;
    return 0;
}

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

    opterr = 0;

    while ((c = getopt(argc, argv, "a:n:p:P:C:R:f:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                errflag |= parse_string_from_opt(opts->mcast_addr_str, sizeof
                        (opts->mcast_addr_str));
                break;
            case 'C':
                errflag |= parse_port_from_opt(&opts->ctrl_port);
                break;
            case 'R':
                errflag |= parse_num_from_opt(&opts->rtime, true);
                break;
            case 'n':
                errflag |= parse_name_from_opt(opts->sender_name,
                                               MAX_NAME_LEN);
                break;
            case 'p':
                errflag |= parse_num_from_opt(&opts->psize, true);
                if (opts->psize + sizeof(struct audio_pack) >
                    UDP_IPV4_DATASIZE) {
                    fprintf(stderr,
                            "Pack size larger than possible to send: %lu\n",
                            opts->psize);
                    errflag = 1;
                }
                break;
            case 'f':
                errflag |= parse_num_from_opt(&opts->fsize, false);
                break;
            case 'P':
                errflag |= parse_port_from_opt(&opts->port);
                break;
            case '?':
                if (optopt == 'a' || optopt == 'p' ||
                    optopt == 'P' || optopt == 'n' || optopt == 'C' ||
                    optopt == 'R' || optopt == 'f')
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

    opterr = 0;

    while ((c = getopt(argc, argv, "n:b:d:C:R:U:")) != -1) {
        switch (c) {
            case 'd':
                errflag |= parse_string_from_opt(opts->discover_addr, sizeof
                        (opts->discover_addr));
                break;
            case 'C':
                errflag |= parse_string_from_opt(opts->ctrl_portstr, sizeof
                        (opts->ctrl_portstr));
                errflag |= parse_port_from_opt(&opts->ctrl_port);
                break;
            case 'U':
                errflag |= parse_port_from_opt(&opts->ui_port);
                break;
            case 'R':
                errflag |= parse_num_from_opt(&opts->rtime, true);
                break;
            case 'b':
                errflag |= parse_num_from_opt(&opts->bsize, true);
                break;
            case 'n':
                errflag |= parse_name_from_opt(opts->sender_name,
                                               MAX_NAME_LEN);
                break;
            case '?':
                if (optopt == 'b' || optopt == 'd' || optopt == 'C' ||
                    optopt == 'R' || optopt == 'U' || optopt == 'n')
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
