#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock;
    struct sockaddr_in addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    char *msg = "Hello from TCP client";
    write(sock, msg, strlen(msg));

    int n = read(sock, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    printf("Server replied: %s\n", buffer);

    close(sock);
    return 0;
}
