#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>

extern int optind;

static char buf[BUFSIZ];

static int urlparse(char *url, char **host, char **path, char **port)
{
    int i, j;

    if (strlen(url) < 7) {
        i = 0;
    } else if (!strncmp(url, "http://", 7)) {
        i = 7;
    } else {
        i = 0;
    }

    if ((i == 0) && strstr(url, "://")) {
        return -1;
    }

    *host = url + i;

    for (; (url[i] != '/') && (url[i] != ':') && (url[i] != '\0'); i++);

    switch (url[i]) {
    case '/':
        url[i] = '\0';
        *port = NULL;
        *path = url + i + 1;
        break;
    case ':':
        url[i] = '\0';
        i++;
        *port = url + i;
        for (; (url[i] != '/') && (url[i] != '\0'); i++);
        if (url[i] == '/') {
            url[i] = '\0';
            *path = url + i + 1;
        } else {
            *path = url + i;
        }

        break;
    case '\0':
        *port = NULL;
        *path = url + i;
        break;
    }

    return 0;
}

static void version(void)
{
    puts("web version 0.3: void where prohibited");
    puts("Copyright 2010, Chris Barts");
    puts("Licensed under the GNU General Public License, version 3.0 or, at your option, any later version.");
    puts("This software has NO WARRANTY, even for USABILITY or FITNESS FOR A PARTICULAR PURPOSE.");
}

static void help(char name[])
{
    printf("%s [options...] url\n", name);
    puts("Options:");
    puts("--out file | -o file    Output data to file");
    puts("--dump file | -d file   Dump header to file");
    puts("--help | -h             Print this help");
    puts("--version | -v          Print version information");
}

int main(int argc, char *argv[])
{
    char *host, *path, *proto, *port, url[BUFSIZ], *hend, *pnam, *of =
        NULL, *df = NULL;
    struct addrinfo hints, *result, *rp;
    int sfd, r, s, c, lind, oh = 1, dh = 2, seen_hend = 0;
    ssize_t reqlen, len, hendi;
    struct option longopts[] = {
        {"out", 1, 0, 0},
        {"dump", 1, 0, 0},
        {"help", 0, 0, 0},
        {"version", 0, 0, 0},
        {0, 0, 0, 0}
    };

    pnam = argv[0];

    if (argc == 1) {
        printf("%s: [options...] url\n", pnam);
        exit(EXIT_SUCCESS);
    }

    while ((c = getopt_long(argc, argv, "o:d:hv", longopts, &lind)) != -1) {
        switch (c) {
        case 0:
            switch (lind) {
            case 0:
                of = optarg;
                break;
            case 1:
                df = optarg;
                break;
            case 2:
                help(pnam);
                exit(EXIT_SUCCESS);
            case 3:
                version();
                exit(EXIT_SUCCESS);
            default:
                help(pnam);
                exit(EXIT_FAILURE);
            }

            break;
        case 'o':
            of = optarg;
            break;
        case 'd':
            df = optarg;
            break;
        case 'h':
            help(pnam);
            exit(EXIT_SUCCESS);
        case 'v':
            version();
            exit(EXIT_SUCCESS);
        default:
            help(pnam);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "%s: URL required\n", pnam);
        goto abend_nonet;
    }

    if (of) {
        if ((oh =
             open(of, O_WRONLY | O_APPEND | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
            perror("open() output file");
            goto abend_nonet;
        }
    }

    if (df) {
        if ((dh =
             open(df, O_WRONLY | O_APPEND | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
            perror("open() dump file");
            goto abend_nonet;
        }
    }

    strncpy(url, argv[optind], sizeof(url));
    if ((r = urlparse(url, &host, &path, &port)) < 0) {
        switch (r) {
        case -1:
            fprintf(stderr, "web: invalid protocol\n");
            goto abend_nonet;
        case -2:
            fprintf(stderr, "web: no host\n");
            goto abend_nonet;
        case -3:
            fprintf(stderr, "web: invalid port %s\n", port);
            goto abend_nonet;
        default:
            fprintf(stderr, "web: can't happen\n");
            goto abend_nonet;
        }
    }

    if (!port)
        port = "80";

    snprintf(buf, sizeof(buf), "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n",
             path, host);
    reqlen = strlen(buf);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    if ((s = getaddrinfo(host, port, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        goto abend_nonet;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sfd =
             socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;              /* Success */
        }

        close(sfd);
    }

    freeaddrinfo(result);

    if (!rp) {
        fprintf(stderr, "Could not connect\n");
        goto abend_nonet;
    }

    if (write(sfd, buf, reqlen) != reqlen) {
        perror("write() to remote host");
        goto abend_net;
    }

    while ((len = read(sfd, buf, sizeof(buf))) > 0) {
        if (seen_hend) {
            if (write(oh, buf, len) != len) {
                perror("write() to output file");
                goto abend_net;
            }

        } else if (!seen_hend && (hend = strstr(buf, "\r\n\r\n"))) {
            seen_hend++;
            hendi = hend - buf + 4;
            if (write(dh, buf, hendi) != hendi) {
                perror("write() to dump file");
                goto abend_net;
            }

            if (write(oh, buf + hendi, len - hendi) != len - hendi) {
                perror("write() to output file");
                goto abend_net;
            }

        } else {
            if (write(dh, buf, len) != len) {
                perror("write() to dump file");
                goto abend_net;
            }
        }
    }

    if (len < 0) {
        perror("read");
        goto abend_net;
    }

    shutdown(sfd, SHUT_RDWR);
    close(sfd);
    if (oh != 1)
        close(oh);
    if (dh != 2)
        close(dh);
    exit(EXIT_SUCCESS);

  abend_net:
    shutdown(sfd, SHUT_RDWR);
    close(sfd);
  abend_nonet:
    if (oh != 1)
        close(oh);
    if (dh != 2)
        close(dh);
    exit(EXIT_FAILURE);
}
