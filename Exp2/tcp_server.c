#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("TCP server listening on port 9000...\n");

    client_fd = accept(server_fd, NULL, NULL);

    int n = read(client_fd, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';
    printf("Received: %s\n", buffer);

    char *reply = "Hello from TCP server";
    write(client_fd, reply, strlen(reply));

    close(client_fd);
    close(server_fd);
    return 0;
}
