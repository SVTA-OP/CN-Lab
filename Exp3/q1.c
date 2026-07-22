/*
 * Assignment 3 - HTTP Client using TCP Sockets
 * Computer Networks Lab
 *
 * Compile: gcc http_client.c -o http_client
 * Run:     ./http_client
 *
 * Accepts a URL (e.g. http://localhost:8000/sample.pdf or
 * http://example.com/index.html), connects to the server over a raw
 * TCP socket, sends a GET request, receives the response, saves the
 * body to a local file, and prints performance statistics:
 *   - Response time   (time to first byte of response)
 *   - Download time    (time to receive the full body)
 *   - Data size         (bytes received)
 *   - Throughput        (KB/s)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 4096
#define MAX_URL_LEN 2048

/* ---- utility: elapsed time in milliseconds between two timevals ---- */
double elapsed_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_usec - start.tv_usec) / 1000.0;
}

/*
 * Parse a URL of the form:
 *   http://host[:port]/path
 * into host, port, and path components.
 */
void parse_url(const char *url, char *host, int *port, char *path) {
    const char *p = url;

    /* Skip scheme */
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        fprintf(stderr, "Note: this client speaks plain HTTP. "
                        "For HTTPS, use the OpenSSL-based client.\n");
        p += 8;
    }

    /* Extract host (and optional port) up to first '/' */
    const char *slash = strchr(p, '/');
    char hostport[512];
    size_t hostport_len = slash ? (size_t)(slash - p) : strlen(p);
    strncpy(hostport, p, hostport_len);
    hostport[hostport_len] = '\0';

    char *colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        strcpy(host, hostport);
        *port = atoi(colon + 1);
    } else {
        strcpy(host, hostport);
        *port = 80;
    }

    /* Extract path */
    if (slash) {
        strcpy(path, slash);
    } else {
        strcpy(path, "/");
    }
}

/* Extract the filename portion of a path, for saving locally */
void get_filename(const char *path, char *filename) {
    const char *last_slash = strrchr(path, '/');
    const char *name = last_slash ? last_slash + 1 : path;

    if (strlen(name) == 0) {
        strcpy(filename, "downloaded_index.html");
    } else {
        strcpy(filename, "downloaded_");
        strcat(filename, name);
    }
}

int main() {
    char url[MAX_URL_LEN];
    char host[512];
    char path[1024];
    int port;

    printf("Enter URL: ");
    if (fgets(url, sizeof(url), stdin) == NULL) {
        fprintf(stderr, "Failed to read URL\n");
        return 1;
    }
    url[strcspn(url, "\r\n")] = '\0'; /* strip trailing newline */

    parse_url(url, host, &port, path);

    printf("\nParsed URL ->  Host: %s | Port: %d | Path: %s\n\n",
           host, port, path);

    /* ---- Resolve hostname ---- */
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error: could not resolve host '%s'\n", host);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    /* ---- Create TCP socket ---- */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct timeval t_connect_start, t_connect_end;
    gettimeofday(&t_connect_start, NULL);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return 1;
    }
    gettimeofday(&t_connect_end, NULL);
    double connect_time = elapsed_ms(t_connect_start, t_connect_end);

    /* ---- Build and send GET request ---- */
    char request[2048];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    struct timeval t_send_start, t_first_byte, t_end;
    gettimeofday(&t_send_start, NULL);

    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("Send failed");
        close(sockfd);
        return 1;
    }

    /* ---- Receive response ---- */
    char buffer[BUFFER_SIZE];
    long total_bytes = 0;
    int header_parsed = 0;
    int first_byte_received = 0;

    /* Save to a temp file first; body-only bytes get written after headers */
    char filename[1024];
    get_filename(path, filename);

    FILE *out_file = fopen(filename, "wb");
    if (!out_file) {
        perror("Could not create output file");
        close(sockfd);
        return 1;
    }

    char header_buf[8192];
    size_t header_len = 0;
    int header_end_found = 0;
    char status_line[256] = "";

    ssize_t n;
    while ((n = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (!first_byte_received) {
            gettimeofday(&t_first_byte, NULL);
            first_byte_received = 1;
        }
        total_bytes += n;

        if (!header_parsed) {
            /* Accumulate into header_buf until we find \r\n\r\n */
            long copy_len = n;
            if (header_len + copy_len > sizeof(header_buf) - 1)
                copy_len = sizeof(header_buf) - 1 - header_len;
            memcpy(header_buf + header_len, buffer, copy_len);
            header_len += copy_len;
            header_buf[header_len] = '\0';

            char *end = strstr(header_buf, "\r\n\r\n");
            if (end) {
                header_end_found = 1;
                header_parsed = 1;

                /* Grab the status line for display */
                char *line_end = strstr(header_buf, "\r\n");
                if (line_end) {
                    size_t slen = line_end - header_buf;
                    if (slen >= sizeof(status_line)) slen = sizeof(status_line) - 1;
                    strncpy(status_line, header_buf, slen);
                    status_line[slen] = '\0';
                }

                /* Write any body bytes that arrived in this same chunk */
                size_t header_bytes = (end - header_buf) + 4;
                size_t body_in_buffer = header_len - header_bytes;
                if (body_in_buffer > 0) {
                    fwrite(header_buf + header_bytes, 1, body_in_buffer, out_file);
                }
            }
        } else {
            fwrite(buffer, 1, n, out_file);
        }
    }
    gettimeofday(&t_end, NULL);
    fclose(out_file);
    close(sockfd);

    if (!header_end_found) {
        fprintf(stderr, "Warning: response headers not fully parsed; "
                        "raw data (if any) was still saved.\n");
    }

    /* Body size = total received minus header bytes */
    long header_size = 0;
    char *hdr_end = strstr(header_buf, "\r\n\r\n");
    if (hdr_end) header_size = (hdr_end - header_buf) + 4;
    long body_size = total_bytes - header_size;
    if (body_size < 0) body_size = 0;

    /* ---- Performance metrics ---- */
    double response_time = elapsed_ms(t_send_start, t_first_byte); /* time to first byte */
    double download_time = elapsed_ms(t_first_byte, t_end);         /* full transfer time */
    double total_time = elapsed_ms(t_connect_start, t_end);
    double throughput_kbps = (download_time > 0)
                              ? (body_size / 1024.0) / (download_time / 1000.0)
                              : 0.0;

    printf("---- Server Response ----\n");
    printf("Status Line     : %s\n", status_line);
    printf("Saved To        : %s\n\n", filename);

    printf("---- Performance Analysis ----\n");
    printf("TCP Connect Time : %.2f ms\n", connect_time);
    printf("Response Time    : %.2f ms  (request sent -> first byte received)\n", response_time);
    printf("Download Time    : %.2f ms  (first byte -> transfer complete)\n", download_time);
    printf("Total Time       : %.2f ms  (connect -> transfer complete)\n", total_time);
    printf("Data Size        : %ld bytes (%.2f KB)\n", body_size, body_size / 1024.0);
    printf("Throughput       : %.2f KB/s\n", throughput_kbps);

    return 0;
}