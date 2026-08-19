// crc_sender.c — simple version
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 6002
#define GEN  "10011"          // G(x) = x^4 + x + 1

// XOR-based binary division; returns remainder (CRC bits) in rem
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
    char msg[] = "HI";
    char bin[100] = "";

    // ASCII -> binary
    for (int i = 0; msg[i]; i++)
        for (int b = 7; b >= 0; b--)
            strcat(bin, ((msg[i] >> b) & 1) ? "1" : "0");

    int glen = strlen(GEN);
    char padded[100];
    sprintf(padded, "%s%0*d", bin, glen - 1, 0);   // append zeros

    char crc[10];
    crc_divide(padded, GEN, crc);

    char frame[100];
    sprintf(frame, "%s%s", bin, crc);

    printf("Data bits : %s\n", bin);
    printf("CRC bits  : %s\n", crc);
    printf("Frame     : %s\n", frame);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(PORT), {inet_addr("127.0.0.1")}};
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    send(sock, frame, strlen(frame), 0);
    printf("Frame sent.\n");
    close(sock);
    return 0;
}