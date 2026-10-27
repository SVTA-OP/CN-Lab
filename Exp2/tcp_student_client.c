// ---------- HEADER FILES ----------
#include <stdio.h>      // printf, fgets (input/output functions)
#include <string.h>     // strlen, strcspn (string functions)
#include <arpa/inet.h>  // socket(), connect(), htons(), inet_pton(), sockaddr_in
                        // (everything needed for network programming)
#include <unistd.h>     // read(), write(), close() (low-level calls on file descriptors)

int main() {

    int sock;                   // file descriptor for our socket (a small integer
                                // the OS uses as a handle for this connection)

    struct sockaddr_in addr;    // struct holding the SERVER's IPv4 address + port
                                // (where we want to connect to)

    char roll[64];      // buffer: the roll number the user types in
    char buffer[300];   // buffer: the response we receive from the server

    // Ask the user for input.
    printf("Enter roll no: ");

    // Read one line from the keyboard (stdin) into 'roll'.
    // sizeof(roll) stops it from writing past the end of the array.
    // fgets keeps the '\n' (Enter key) at the end of what you typed.
    fgets(roll, sizeof(roll), stdin);

    // Remove that trailing newline.
    // strcspn(roll, "\n") returns the position of the first '\n' in the string
    // (or the string length if there is none). We overwrite that position
    // with '\0', which ends the string there.
    // This matters because the server compares "101" with strcmp, and
    // "101\n" would never match.
    roll[strcspn(roll, "\n")] = '\0';

    // Create a socket.
    //   AF_INET     = IPv4
    //   SOCK_STREAM = TCP (reliable, connection-based)
    //   0           = default protocol for TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // Fill in the server's address:
    addr.sin_family = AF_INET;      // IPv4 address family
    addr.sin_port = htons(9000);    // server's port is 9000. htons converts the number
                                    // to "network byte order" (big-endian), because the
                                    // network and your CPU may order bytes differently.

    // inet_pton converts the text IP "127.0.0.1" into the binary form the
    // struct needs, and stores it in addr.sin_addr.
    // 127.0.0.1 = "localhost", i.e. the server is on THIS same machine.
    // To talk to a server on another computer, put its IP here instead.
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // connect = reach out to the server at that address and port.
    // This performs the TCP handshake. It's where the server's accept() wakes up.
    // (struct sockaddr*) is a cast: connect expects a generic address type.
    // NOTE: the return value isn't checked, so if the server isn't running,
    // the code carries on anyway and misbehaves below.
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    // Send the roll number to the server.
    // strlen(roll) = send exactly as many bytes as the text has (no '\0').
    write(sock, roll, strlen(roll));

    // Wait for the server's reply and store it in 'buffer'.
    // sizeof(buffer) - 1 leaves one spare slot for the '\0' terminator.
    // n = number of bytes actually received.
    // read() blocks (waits) until the server sends something.
    int n = read(sock, buffer, sizeof(buffer) - 1);

    // Network data is raw bytes with no string ending, so we add '\0'
    // ourselves to turn it into a proper C string we can print.
    buffer[n] = '\0';

    // Show the server's answer (the student's CSV line, or "Student not found").
    printf("Server response: %s\n", buffer);

    // Close the connection. The server's read/write on this client is now finished.
    close(sock);

    return 0;    // exit the program successfully
}