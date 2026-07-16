#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

// Search students.csv for the given roll number.
// If found, fill 'result' with the matching line and return 1.
// Otherwise return 0.
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
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char roll[64], result[256], reply[300];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("TCP student server listening on port 9000...\n");

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);

        int n = read(client_fd, roll, sizeof(roll) - 1);
        roll[n] = '\0';
        printf("Client requested roll no: %s\n", roll);

        if (find_student(roll, result)) {
            strcpy(reply, result);
        } else {
            strcpy(reply, "Student not found\n");
        }

        write(client_fd, reply, strlen(reply));
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
