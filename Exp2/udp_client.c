#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    char *msg = "Hello from UDP client";
    sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, len);

    int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
    buffer[n] = '\0';
    printf("Server replied: %s\n", buffer);

    close(sock);
    return 0;
}
