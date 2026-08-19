/*
 * dns_client.c
 * SSN College of Engineering – UDP DNS Simulation
 * Compile: gcc dns_client.c -o client
 * Run:     ./client <domain>
 *          ./client google.com
 *          ./client unknown.xyz
 *          ./client "bad domain"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>

#define SERVER_IP    "127.0.0.1"
#define PORT         8080
#define BUF_SIZE     256
#define TIMEOUT_SEC  3          /* seconds before timeout */
#define CACHE_SIZE   16

/* ── Client-side Cache ──────────────────────────────────────────────────── */
typedef struct {
    char hostname[BUF_SIZE];
    char ip[16];
} cache_entry;

cache_entry cache[CACHE_SIZE];
int cache_count = 0;

/* Look up hostname in local cache; returns IP string or NULL */
const char *cache_lookup(const char *hostname) {
    for (int i = 0; i < cache_count; i++) {
        if (strcasecmp(cache[i].hostname, hostname) == 0)
            return cache[i].ip;
    }
    return NULL;
}

/* Store a resolved hostname→IP pair in the cache */
void cache_store(const char *hostname, const char *ip) {
    if (cache_count < CACHE_SIZE) {
        strncpy(cache[cache_count].hostname, hostname, BUF_SIZE - 1);
        strncpy(cache[cache_count].ip,       ip,       15);
        cache_count++;
        printf("[CACHE]    Stored: %-25s → %s\n", hostname, ip);
    }
}

/* Print the entire cache for inspection */
void cache_print(void) {
    if (cache_count == 0) {
        printf("(cache is empty)\n");
        return;
    }
    printf("\n%-25s %s\n", "CACHED HOSTNAME", "IP ADDRESS");
    printf("%-25s %s\n",   "---------------", "----------");
    for (int i = 0; i < cache_count; i++)
        printf("%-25s %s\n", cache[i].hostname, cache[i].ip);
}

/* ── DNS Resolution ─────────────────────────────────────────────────────── */
void resolve(const char *domain) {
    printf("\n==============================\n");
    printf("Resolving: %s\n", domain);
    printf("==============================\n");

    /* 1. Check cache first */
    const char *cached_ip = cache_lookup(domain);
    if (cached_ip) {
        printf("[CACHE HIT] %s → %s  (served from cache, no network call)\n",
               domain, cached_ip);
        return;
    }
    printf("[CACHE MISS] Not in cache — querying DNS server...\n");

    /* 2. Create UDP socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return; }

    /* 3. Set receive timeout (simulates DNS timeout) */
    struct timeval tv = { .tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt"); close(sockfd); return;
    }

    /* 4. Build server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP\n"); close(sockfd); return;
    }

    /* 5. Send the query */
    if (sendto(sockfd, domain, strlen(domain), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto"); close(sockfd); return;
    }
    printf("[SENT]     Query sent for: %s\n", domain);

    /* 6. Wait for response */
    char response[BUF_SIZE];
    memset(response, 0, BUF_SIZE);
    socklen_t server_len = sizeof(server_addr);

    int n = recvfrom(sockfd, response, BUF_SIZE - 1, 0,
                     (struct sockaddr *)&server_addr, &server_len);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            printf("[TIMEOUT]  No response from server after %d seconds.\n",
                   TIMEOUT_SEC);
        else
            perror("recvfrom");
        close(sockfd);
        return;
    }

    response[n] = '\0';

    /* 7. Parse response */
    if (strncmp(response, "IP:", 3) == 0) {
        const char *ip = response + 3;
        printf("[RESOLVED] %s → %s\n", domain, ip);
        cache_store(domain, ip);                 /* store in cache */

    } else if (strcmp(response, "ERROR:NXDOMAIN") == 0) {
        printf("[ERROR]    NXDOMAIN — '%s' does not exist.\n", domain);

    } else if (strcmp(response, "ERROR:INVALID_FORMAT") == 0) {
        printf("[ERROR]    INVALID FORMAT — '%s' is not a valid domain name.\n",
               domain);
    } else {
        printf("[UNKNOWN]  Unexpected response: %s\n", response);
    }

    close(sockfd);
}

/* ── Main ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    printf("==============================================\n");
    printf("  DNS Client – UDP Socket Simulation\n");
    printf("  Server: %s:%d   Timeout: %ds\n",
           SERVER_IP, PORT, TIMEOUT_SEC);
    printf("==============================================\n");

    if (argc < 2) {
        /* Interactive mode */
        printf("Usage: %s <domain> [domain2] ...\n", argv[0]);
        printf("Running demo queries...\n");

        /* Demo: mix of hits, misses, cache hits, and errors */
        resolve("google.com");       /* should resolve */
        resolve("ssn.edu.in");       /* should resolve */
        resolve("google.com");       /* cache hit — no server call */
        resolve("unknown.xyz");      /* NXDOMAIN */
        resolve("bad domain!");      /* INVALID FORMAT */

    } else {
        /* Resolve each domain passed as argument */
        for (int i = 1; i < argc; i++)
            resolve(argv[i]);
    }

    /* Print final cache state */
    printf("\n--- Final Cache Contents ---\n");
    cache_print();

    return 0;
}