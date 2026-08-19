/*
 * dns_server.c
 * SSN College of Engineering – UDP DNS Simulation
 * Compile: gcc dns_server.c -o server
 * Run:     ./server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#define PORT        8080
#define BUF_SIZE    256
#define TABLE_SIZE  10

/* ── DNS Table (hostname → IP) ─────────────────────────────────────────── */
typedef struct {
    char hostname[BUF_SIZE];
    char ip[16];
} dns_entry;

dns_entry dns_table[TABLE_SIZE] = {
    {"google.com",      "142.250.182.46"},
    {"youtube.com",     "142.250.72.238"},
    {"facebook.com",    "157.240.241.35"},
    {"instagram.com",   "157.240.241.174"},
    {"twitter.com",     "104.244.42.193"},
    {"github.com",      "140.82.121.4"},
    {"ssn.edu.in",      "203.197.132.10"},
    {"amazon.com",      "205.251.242.103"},
    {"wikipedia.org",   "208.80.154.224"},
    {"openai.com",      "172.64.155.209"},
};

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Returns 1 if the query string is a valid-looking domain name */
int is_valid_domain(const char *query) {
    int len = strlen(query);
    if (len == 0 || len > 253) return 0;

    /* Must contain at least one dot */
    int has_dot = 0;
    for (int i = 0; i < len; i++) {
        char c = query[i];
        if (c == '.') { has_dot = 1; continue; }
        if (!isalnum(c) && c != '-' && c != '_') return 0;
    }
    return has_dot;
}

/* Look up a hostname in the DNS table; returns IP string or NULL */
const char *lookup(const char *hostname) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (strcasecmp(dns_table[i].hostname, hostname) == 0)
            return dns_table[i].ip;
    }
    return NULL;
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(void) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];

    /* 1. Create UDP socket */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* 2. Bind to port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("DNS Server listening on port %d...\n\n", PORT);
    printf("%-25s %s\n", "HOSTNAME", "IP ADDRESS");
    printf("%-25s %s\n", "--------", "----------");
    for (int i = 0; i < TABLE_SIZE; i++)
        printf("%-25s %s\n", dns_table[i].hostname, dns_table[i].ip);
    printf("\nWaiting for queries...\n\n");

    /* 3. Query-response loop */
    while (1) {
        memset(buffer, 0, BUF_SIZE);

        /* Receive query from client */
        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) { perror("recvfrom"); continue; }

        buffer[n] = '\0';
        /* Strip trailing newline if any */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        printf("[QUERY]    %-25s from %s\n",
               buffer, inet_ntoa(client_addr.sin_addr));

        char response[BUF_SIZE];

        /* 4. Validate format */
        if (!is_valid_domain(buffer)) {
            snprintf(response, BUF_SIZE, "ERROR:INVALID_FORMAT");
            printf("[RESPONSE] %s\n\n", response);
        } else {
            /* 5. Look up in DNS table */
            const char *ip = lookup(buffer);
            if (ip) {
                snprintf(response, BUF_SIZE, "IP:%s", ip);
                printf("[RESPONSE] %s → %s\n\n", buffer, ip);
            } else {
                snprintf(response, BUF_SIZE, "ERROR:NXDOMAIN");
                printf("[RESPONSE] %s → NXDOMAIN (domain not found)\n\n", buffer);
            }
        }

        /* 6. Send response back to client */
        sendto(sockfd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sockfd);
    return 0;
}