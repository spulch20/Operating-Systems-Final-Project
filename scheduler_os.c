/*Copied over from Assignment 3 code*/
#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define RECV_CHUNK 4096

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

static void buffer_init(Buffer *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int buffer_append(Buffer *b, const char *src, size_t n) {
    size_t needed = b->len + n + 1;
    if (needed > b->cap) {
        size_t new_cap = (b->cap == 0) ? 8192 : b->cap;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(b->data, new_cap);
        if (!new_data) {
            return -1;
        }
        b->data = new_data;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static void buffer_free(Buffer *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;
    int sockfd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host, port, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(rc));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);
    return sockfd;
}

static int send_all(int sockfd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int parse_http_status(const char *response) {
    int status = 0;
    if (sscanf(response, "HTTP/%*d.%*d %d", &status) != 1) {
        return -1;
    }
    return status;
}

static const char *get_http_body(const char *response) {
    const char *marker = strstr(response, "\r\n\r\n");
    if (!marker) {
        return NULL;
    }
    return marker + 4;
}

//Modified version of the http_request function from assignment 3. (removed sid)
static int http_request(const char *port, const char *method, const char *path, const char *body, Buffer *out_response) {
    int sockfd = -1;
    Buffer req;
    char header_line[512];
    size_t body_len = (body == NULL) ? 0 : strlen(body);
    char recv_buf[RECV_CHUNK];

    buffer_init(&req);
    buffer_init(out_response);

    sockfd = connect_to_server(HOST, port);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to %s:%s\n", HOST, port);
        buffer_free(&req);
        return -1;
    }

    /*
     * Build HTTP request line + required headers.
     *
     * Example start:
     *   GET /kv/initialize HTTP/1.1
     *   Host: 127.0.0.1:34923
     *   Connection: close
     */
    snprintf(header_line, sizeof(header_line),
             "%s %s HTTP/1.1\r\n"
             "Host: %s:%s\r\n"
             "Connection: close\r\n",
             method, path, HOST, port);
    if (buffer_append(&req, header_line, strlen(header_line)) != 0) {
        close(sockfd);
        buffer_free(&req);
        return -1;
    }

    

    /*
     * If there is a body, HTTP requires Content-Length.
     * API expects plain text, so Content-Type is text/plain.
     */
    if (body != NULL) {
        snprintf(header_line, sizeof(header_line),
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n",
                 body_len);
        if (buffer_append(&req, header_line, strlen(header_line)) != 0) {
            close(sockfd);
            buffer_free(&req);
            return -1;
        }
    }

    /*
     * Blank line ends the header section in HTTP.
     * After this line comes the optional request body.
     */
    if (buffer_append(&req, "\r\n", 2) != 0) {
        close(sockfd);
        buffer_free(&req);
        return -1;
    }

    if (body != NULL) {
        if (buffer_append(&req, body, body_len) != 0) {
            close(sockfd);
            buffer_free(&req);
            return -1;
        }
    }

    if (send_all(sockfd, req.data, req.len) != 0) {
        fprintf(stderr, "send() failed\n");
        close(sockfd);
        buffer_free(&req);
        return -1;
    }

    /*
     * Read until recv() returns 0 (peer closed connection).
     * We requested "Connection: close", so this reliably marks end of response.
     */
    for (;;) {
        ssize_t n = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "recv() failed\n");
            close(sockfd);
            buffer_free(&req);
            buffer_free(out_response);
            return -1;
        }
        if (n == 0) {
            break; /* server closed connection */
        }
        if (buffer_append(out_response, recv_buf, (size_t)n) != 0) {
            close(sockfd);
            buffer_free(&req);
            buffer_free(out_response);
            return -1;
        }
    }

    close(sockfd);
    buffer_free(&req);
    return 0;
}

/*end of section copied from Assignment 3*/