// checksum_receiver.c — simple version
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6001
#ifndef INJECT_ERROR
#define INJECT_ERROR 0
#endif

unsigned char checksum(unsigned char *data, int n) {
    unsigned int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
        if (sum > 0xFF) sum = (sum & 0xFF) + 1;
    }
    return sum & 0xFF;
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
    unsigned char frame[100];
    int len = recv(conn, frame, sizeof(frame), 0);

    printf("Received: ");
    for (int i = 0; i < len; i++) printf("%02X ", frame[i]);
    printf("\n");

#if INJECT_ERROR
    frame[0] ^= 0x01;   // flip a bit to simulate transmission error
    printf("Injected error -> first byte now 0x%02X\n", frame[0]);
#endif

    unsigned char result = checksum(frame, len);   // includes checksum byte
    printf("Verification sum: 0x%02X\n", result);

    if (result == 0xFF)
        printf("RESULT: No error.\n");
    else
        printf("RESULT: Error detected!\n");

    close(conn);
    close(server);
    return 0;
}