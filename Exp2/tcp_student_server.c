// ---------- HEADER FILES (libraries we borrow code from) ----------
#include <stdio.h>      // printf, fopen, fgets, fclose (input/output functions)
#include <string.h>     // strcpy, strcmp, strtok, strlen (string functions)
#include <arpa/inet.h>  // socket(), bind(), listen(), accept(), htons(), sockaddr_in
                        // (everything needed for network programming)
#include <unistd.h>     // read(), write(), close() (low-level system calls on file descriptors)

// ---------- FUNCTION: find_student ----------
// Looks through students.csv for a row whose FIRST column equals 'roll'.
// 'roll'   = the roll number we are searching for (input)
// 'result' = a buffer where we copy the matching row (output)
// Returns 1 if found, 0 if not found.
int find_student(char *roll, char *result) {

    // Open students.csv in read mode ("r"). fp is a pointer to the open file.
    // NOTE: if the file doesn't exist, fp will be NULL (the code doesn't check this!)
    FILE *fp = fopen("students.csv", "r");

    // A buffer (array of 256 characters) to hold one line of the file at a time.
    char line[256];

    // Read the very first line (the header, like "roll,name,dept") and throw it away,
    // because we only care about actual student rows.
    fgets(line, sizeof(line), fp);

    // Keep reading one line at a time until the file ends.
    // fgets returns NULL at end of file, which stops the loop.
    while (fgets(line, sizeof(line), fp)) {

        // Make a copy of the line. We need this because strtok DESTROYS the
        // string it works on (it inserts '\0' where commas were).
        // We want the original 'line' to stay intact so we can send it back later.
        char line_copy[256];
        strcpy(line_copy, line);

        // strtok splits a string at a delimiter (here, a comma).
        // First call returns the first chunk, i.e. the first column (the roll number).
        // Example: "101,Arun,CSE" -> token points to "101"
        char *token = strtok(line_copy, ",");

        // Check that a token was actually found (not NULL),
        // AND that it matches the roll number the client asked for.
        // strcmp returns 0 when the two strings are exactly equal.
        if (token != NULL && strcmp(token, roll) == 0) {

            // Match! Copy the ORIGINAL full line (not the destroyed copy) into 'result'
            // so main() can send it to the client.
            strcpy(result, line);

            fclose(fp);   // close the file since we're done with it
            return 1;     // tell the caller: "found it"
        }
    }

    // We went through the whole file with no match.
    fclose(fp);   // always close the file before leaving
    return 0;     // tell the caller: "not found"
}

// ---------- MAIN: the TCP server ----------
int main() {

    int server_fd, client_fd;   // "file descriptors" = small integers that
                                // represent open sockets (like handles).
                                // server_fd = the listening socket
                                // client_fd = the connection to one client

    struct sockaddr_in addr;    // struct that holds an IPv4 address + port number
                                // (where our server will "live" on the network)

    char roll[64];     // buffer: the roll number received from the client
    char result[256];  // buffer: the matching CSV line from find_student
    char reply[300];   // buffer: the final message we send back to the client

    // Create a socket.
    //   AF_INET     = use IPv4
    //   SOCK_STREAM = use TCP (reliable, connection-based)
    //   0           = pick the default protocol for TCP
    // Returns a file descriptor for the new socket.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Fill in the address struct:
    addr.sin_family = AF_INET;           // IPv4 address family
    addr.sin_addr.s_addr = INADDR_ANY;   // accept connections on ANY of this machine's
                                         // network interfaces (wifi, ethernet, localhost...)
    addr.sin_port = htons(9000);         // port 9000. htons converts the number to
                                         // "network byte order" (big-endian), because
                                         // the network and your CPU may order bytes differently

    // bind = attach the socket to the IP/port we set above.
    // Without this, the OS wouldn't know which port our server wants.
    // (struct sockaddr*) is a cast: bind expects a generic address type.
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));

    // listen = switch the socket into "waiting for clients" mode.
    // The 1 is the backlog: how many pending connections can queue up
    // while we're busy with someone else.
    listen(server_fd, 1);

    printf("TCP student server listening on port 9000...\n");

    // Infinite loop: the server never stops, it handles one client after another.
    while (1) {

        // accept = block (wait) until a client connects.
        // It returns a NEW socket (client_fd) dedicated to talking to that client.
        // The two NULLs mean "I don't care about the client's address".
        client_fd = accept(server_fd, NULL, NULL);

        // read = receive bytes the client sent us, into the 'roll' buffer.
        // sizeof(roll) - 1 leaves one spare slot for the '\0' terminator.
        // n = number of bytes actually received.
        int n = read(client_fd, roll, sizeof(roll) - 1);

        // Network data is just raw bytes with no string ending, so we manually
        // add '\0' at the end to turn it into a proper C string.
        roll[n] = '\0';

        printf("Client requested roll no: %s\n", roll);

        // Search the CSV for this roll number.
        if (find_student(roll, result)) {
            // Found: put the student's line into the reply.
            strcpy(reply, result);
        } else {
            // Not found: reply with an error message.
            strcpy(reply, "Student not found\n");
        }

        // write = send the reply bytes back to the client over the socket.
        // strlen(reply) = send exactly as many bytes as the message has.
        write(client_fd, reply, strlen(reply));

        // Close this client's connection. The server goes back to the top of
        // the loop and waits for the next client.
        close(client_fd);
    }

    // These two lines are never reached (the while(1) loop never ends),
    // but they're good practice.
    close(server_fd);   // close the listening socket
    return 0;           // exit the program successfully
}