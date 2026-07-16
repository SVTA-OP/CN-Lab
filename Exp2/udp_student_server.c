#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int find_student(char *roll, char *result) {
    FILE *fp = fopen("students.csv", "r");
    char line[256];

    fgets(line, sizeof(line), fp); // skip header line

    while (fgets(line, sizeof(line), fp)) {
        char line_copy[256];
        strcpy(line_copy, line);

        char *token = strtok(line_copy, ",");
        if (token != NULL && strcmp(token, roll) == 0) {
            strcpy(result, line);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int main() {
    int sock;
    struct sockaddr_in addr, client_addr;
    socklen_t len = sizeof(client_addr);
    char roll[64], result[256], reply[300];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9001);

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    printf("UDP student server listening on port 9001...\n");

    while (1) {
        int n = recvfrom(sock, roll, sizeof(roll) - 1, 0,
                          (struct sockaddr*)&client_addr, &len);
        roll[n] = '\0';
        printf("Client requested roll no: %s\n", roll);

        if (find_student(roll, result)) {
            strcpy(reply, result);
        } else {
            strcpy(reply, "Student not found\n");
        }

        sendto(sock, reply, strlen(reply), 0,
               (struct sockaddr*)&client_addr, len);
    }

    close(sock);
    return 0;
}
