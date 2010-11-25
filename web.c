#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
        if (url[i] = '/') {
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

int main(int argc, char *argv[])
{
    char *host, *path, *proto, *port, url[BUFSIZ];
    struct addrinfo hints, *result, *rp;
    int sfd, r, s;
    ssize_t reqlen, len, i;

    if (argc != 2) {
        fprintf(stderr, "usage: web url\n");
        return 0;
    }

    strncpy(url, argv[1], sizeof(url));
    if ((r = urlparse(url, &host, &path, &port)) < 0) {
        switch (r) {
        case -1:
            fprintf(stderr, "web: invalid protocol\n");
            exit(EXIT_FAILURE);
        case -2:
            fprintf(stderr, "web: no host\n");
            exit(EXIT_FAILURE);
        case -3:
            fprintf(stderr, "web: invalid port %s\n", port);
            exit(EXIT_FAILURE);
        default:
            fprintf(stderr, "web: can't happen\n");
            exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
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

    if (!rp) {
        freeaddrinfo(result);
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);

    if (write(sfd, buf, reqlen) != reqlen) {
        perror("write");
        shutdown(sfd, SHUT_RDWR);
        close(sfd);
        exit(EXIT_FAILURE);
    }

    while ((len = read(sfd, buf, sizeof(buf))) > 0) {
        write(1, buf, len);
    }

    if (len < 0) {
        perror("read");
        shutdown(sfd, SHUT_RDWR);
        close(sfd);
        exit(EXIT_FAILURE);
    }

    shutdown(sfd, SHUT_RDWR);
    close(sfd);
    return 0;
}
