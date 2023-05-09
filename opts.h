#ifndef _OPTS_H_
#define _OPTS_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define DEFAULT_PSIZE 512
#define DEFAULT_PORT 28473
#define DEFAULT_BSIZE 65536
#define DEFAULT_NAME "Nienazwany Nadajnik"

struct sender_opts {
    /** address of targeted receiver (set with option -a, obligatory) */
    char *dest_addr;

    /** data port (set with option -P) defaults to @p DEFAULT_PORT */
    uint16_t port;

    /** audio_data size (set with -p) defaults to @p DEFAULT_PSIZE */
    uint32_t psize;

    /** sender name (set with -n) defaults to @p DEFAULT_NAME */
    char *sender_name;
};

typedef struct sender_opts sender_opts;

struct receiver_opts {
    /** address of whitelisted sender
     * set with option -a, obligatory
     */
    char *from_addr;

    /** data port
     * set with option -P, defaults to @p DEFAULT_PORT
     */
    char portstr[12];
    uint16_t port;

    /** buffer size (set with -b) defaults to @p DEFAULT_BSIZE */
    uint32_t bsize;
};

typedef struct receiver_opts receiver_opts;

inline static sender_opts *get_sender_opts(int argc, char **argv) {
    sender_opts *opts = malloc(sizeof(sender_opts));

    opts->psize = DEFAULT_PSIZE;
    opts->port = DEFAULT_PORT;
    opts->sender_name = DEFAULT_NAME;

    int aflag = 0;
    int errflag = 0;

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "a:n:p:P:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                opts->dest_addr = optarg;
                break;
            case 'n':
                opts->sender_name = optarg;
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
                if (optopt == 'a' || optopt == 'p' || optopt == 'P' || optopt
                                                                       == 'n')
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
    sprintf(opts->portstr, "%d", DEFAULT_PORT);

    int aflag = 0;

    int errflag = 0;

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "a:b:P:")) != -1) {
        switch (c) {
            case 'a':
                aflag = 1;
                opts->from_addr = optarg;
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
                    memcpy(opts->portstr, optarg, sizeof(opts->port));
                break;
            case '?':
                if (optopt == 'a' || optopt == 'b' || optopt == 'P')
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
