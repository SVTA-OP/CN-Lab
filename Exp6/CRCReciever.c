// crc_receiver.c — simple version
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6002
#define GEN  "10011"
#ifndef INJECT_ERROR
#define INJECT_ERROR 0
#endif

void crc_divide(char *data, char *gen, char *rem) {
    int dlen = strlen(data), glen = strlen(gen);
    char temp[100];
    strcpy(temp, data);

    for (int i = 0; i <= dlen - glen; i++) {
        if (temp[i] == '1')
            for (int j = 0; j < glen; j++)
                temp[i + j] = (temp[i + j] == gen[j]) ? '0' : '1';
    }
    strcpy(rem, temp + dlen - (glen - 1));
}

int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {AF_INET, htons(PORT), {INADDR_ANY}};
    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 1);
    printf("Waiting for sender...\n");

    int conn = accept(server, NULL, NULL);
    char frame[100] = {0};
    int len = recv(conn, frame, sizeof(frame) - 1, 0);
    frame[len] = '\0';
    printf("Received: %s\n", frame);

#if INJECT_ERROR
    frame[3] = (frame[3] == '0') ? '1' : '0';   // flip bit 3
    printf("Injected error -> %s\n", frame);
#endif

    char rem[10];
    crc_divide(frame, GEN, rem);
    printf("Remainder: %s\n", rem);

    if (strspn(rem, "0") == strlen(rem))
        printf("RESULT: No error.\n");
    else
        printf("RESULT: Error detected!\n");

    close(conn);
    close(server);
    return 0;
}