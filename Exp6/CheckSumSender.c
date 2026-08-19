// checksum_sender.c — simple version
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6001

unsigned char checksum(unsigned char *data, int n) {
    unsigned int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
        if (sum > 0xFF) sum = (sum & 0xFF) + 1;   // end-around carry
    }
    return ~sum & 0xFF;                            // 1's complement
}

int main() {
    char msg[] = "CheckSumTestMessage";
    int n = strlen(msg);
    unsigned char cs = checksum((unsigned char *)msg, n);

    printf("Data: %s\nChecksum: 0x%02X\n", msg, cs);

    unsigned char frame[100];
    memcpy(frame, msg, n);
    frame[n] = cs;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(PORT), {inet_addr("127.0.0.1")}};
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    send(sock, frame, n + 1, 0);
    printf("Frame sent.\n");
    close(sock);
    return 0;
}