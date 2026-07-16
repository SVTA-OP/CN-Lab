#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock;
    struct sockaddr_in addr, client_addr;
    socklen_t len = sizeof(client_addr);
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9001);

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    printf("UDP server listening on port 9001...\n");

    int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                      (struct sockaddr*)&client_addr, &len);
    buffer[n] = '\0';
    printf("Received: %s\n", buffer);

    char *reply = "Hello from UDP server";
    sendto(sock, reply, strlen(reply), 0,
           (struct sockaddr*)&client_addr, len);

    close(sock);
    return 0;
}
