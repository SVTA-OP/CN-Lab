# Computer Networks Lab: Practical Exam Survival Guide

Assignments 1–6 use **C sockets**. Assignments 7–11 use **NS-3 (C++)**.
Every C program here was **compiled and run-tested**. The NS-3 programs follow the standard NS-3 API but could not be run in my environment, so compile them once before the exam and fix any version differences (listed in the NS-3 section).

> **How to use this:** For each assignment: read "In one minute", type the code yourself (don't just paste it), run it, then answer the "Viva questions" out loud.

## Contents
0. Setup and compile cheat sheet
1. Socket programming: the mental model
2. Assignment 1: Diagnostic tools + socket calls
3. Assignment 2: TCP and UDP student server
4. Assignment 3: HTTP client (and HTTPS)
5. Assignment 4: Wireshark HTTP
6. Assignment 5: UDP DNS simulation
7. Assignment 6: Checksum and CRC
8. NS-3 fundamentals
9. Assignment 7: UDP, latency, ports
10. Assignment 8: TCP + UDP dumbbell
11. Assignment 9: Reno vs Cubic vs BBR
12. Assignment 10: RIP / OSPF / AODV
13. Assignment 11: Wi-Fi / Bluetooth / Zigbee mobility
14. Last-night checklist and common errors

---

# 0. Setup and compile cheat sheet

```bash
# C programs
gcc file.c -o file                   # normal
gcc file.c -o file -pthread          # when using threads (Assignment 2 TCP server)
gcc file.c -o file -lssl -lcrypto    # OpenSSL (HTTPS client)
./file

# Two terminals: ALWAYS start the server first, then the client.
# Port already in use?   ->  wait 30 s, or use SO_REUSEADDR (already in our code)
# Kill a stuck server:   ->  Ctrl+C, or:  fuser -k 8080/tcp

# NS-3 (from the ns-3 folder). Put your .cc file in scratch/
./ns3 run scratch/myfile             # newer ns-3 (3.36+)
./ns3 run "scratch/myfile --arg=value"
./waf --run scratch/myfile           # older ns-3 (before 3.36)
```

Create the sample data once (used by Assignment 2):
```bash
cat > students.csv <<'EOF'
RollNo,Name,Department,Semester,CGPA
101,Asha,CSE,5,8.7
102,Ravi,ECE,3,7.9
103,Meena,CSE,7,9.1
104,Karthik,MECH,1,8.2
EOF
```

---

# 1. Socket programming: the mental model

A **socket** is a file-like handle to a network connection. You read and write it like a file.
Think of **TCP like a phone call** (dial, they pick up, talk reliably, hang up) and **UDP like postcards** (just send; might get lost or arrive out of order).

## The two call sequences (memorize these)

```
TCP SERVER                    TCP CLIENT
socket()                      socket()
bind()    <- attach to port
listen()  <- start waiting
accept()  <---- connect() ----  connect()
recv()/read()  <-- data -->   send()/write()
close()                       close()

UDP SERVER                    UDP CLIENT
socket(SOCK_DGRAM)            socket(SOCK_DGRAM)
bind()                        (no bind needed)
recvfrom()  <-- datagram --   sendto()
sendto()    -- reply -->      recvfrom()
close()                       close()
```

Key differences: UDP has **no listen / accept / connect**. Every UDP packet carries the destination address (`sendto`) and tells you the sender (`recvfrom`).

## The address struct (you write this every time)

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof addr);          // or bzero(&addr, sizeof addr)
addr.sin_family      = AF_INET;         // IPv4
addr.sin_port        = htons(8080);     // port in NETWORK byte order
addr.sin_addr.s_addr = htonl(INADDR_ANY);        // server: accept on any interface
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // client: server's IP
```

**Why `htons`?** Networks use big-endian byte order; your PC (x86) is little-endian. `h`ost→`n`etwork `s`hort (16-bit, ports), `l`ong (32-bit, IPs). `ntohs`/`ntohl` go the other way.

## Return values rule
Almost every call returns **-1 on error** (then check `perror("name")`). `recv` returns **0 when the other side closed**, `>0` = bytes read.

## Common pitfalls
| Problem | Fix |
|---|---|
| `bind: Address already in use` | `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, ...)` before bind |
| Client says "Connection refused" | Server not running, or wrong port |
| Forgot `htons` on port | Connects to a random wrong port |
| TCP data arrives in chunks | TCP is a stream, so loop on `recv` until 0 |
| `fgets` includes `\n` | Strip it: `buf[strcspn(buf,"\r\n")] = 0;` |
| UDP client hangs forever | Reply was lost, so set `SO_RCVTIMEO` timeout |

---

# 2. Assignment 1: Diagnostic tools and socket calls

## 1a. Networking tools: what to run and what to write down

Run each, take a screenshot, and write 1–2 lines of observation. Install if missing:
`sudo apt install net-tools iw nmap tcpdump dnsutils traceroute mtr-tiny whois curl wget`

| Tool | Command to try | What it tells you | Note down |
|---|---|---|---|
| `netstat` | `netstat -tulnp` (listening ports), `netstat -rn` (routes), `netstat -s` (stats) | Open ports/connections | Which ports listen, which process |
| `ifconfig` | `ifconfig` or `ip a` | Interface config: IP, MAC, MTU, RX/TX | Your IP, netmask, MAC |
| `iwconfig` | `iwconfig` | Wireless info: ESSID, signal, bit rate | Signal level, link quality |
| `ping` | `ping -c 4 google.com` | Connectivity and RTT | min/avg/max RTT, % loss |
| `route` | `route -n` or `ip route` | Routing table, default gateway | Gateway IP, interface |
| `nmap` | `nmap -sP 192.168.1.0/24` (hosts), `nmap -p 1-1000 localhost` (ports) | Host and port discovery | Open ports and services |
| `tcpdump` | `sudo tcpdump -i any -c 10`, `sudo tcpdump port 53` | Captures packets in text | Protocols, src/dst |
| `dig` | `dig google.com`, `dig +short google.com`, `dig MX gmail.com` | DNS query details | Answer IP, TTL, query time, server |
| `traceroute` | `traceroute google.com` | Every router hop to the target | Hop count, latency per hop |
| `mtr` | `mtr -rc 5 google.com` | traceroute + ping combined (loss per hop) | Which hop is slow |
| `curl` | `curl -I http://example.com`, `curl -v URL` | Fetch URL, show HTTP headers | Status code, headers |
| `wget` | `wget http://example.com/index.html` | Download file | Speed, size |
| `tracepath` | `tracepath google.com` | Like traceroute and shows path MTU, no root needed | MTU |
| `nslookup` | `nslookup google.com` | DNS lookup (older style) | Server used, IP |
| `host` | `host google.com` | Simplest DNS lookup | IPs |
| `whois` | `whois example.com` | Domain registration info | Registrar, dates |

**Handy ways to explain results:** `ping` uses **ICMP echo**. `traceroute` increments **TTL** so each router sends back "time exceeded". `dig` shows **ANSWER SECTION**. TTL there is how long the answer may be cached.

## 1b. Socket system calls: reference table

| Call | Purpose | Syntax | Returns |
|---|---|---|---|
| `socket()` | Create a socket | `int socket(int domain, int type, int protocol)` e.g. `socket(AF_INET, SOCK_STREAM, 0)` (TCP) / `SOCK_DGRAM` (UDP) | fd ≥ 0, or -1 |
| `bind()` | Attach socket to local IP:port | `int bind(int fd, const struct sockaddr *addr, socklen_t len)` | 0 / -1 |
| `listen()` | Mark TCP socket as passive; `backlog` = queue of pending connections | `int listen(int fd, int backlog)` | 0 / -1 |
| `connect()` | TCP: do 3-way handshake with server. UDP: just sets default peer | `int connect(int fd, const struct sockaddr *srv, socklen_t len)` | 0 / -1 |
| `accept()` | Take one connection from queue, returns a **new** socket for that client | `int accept(int fd, struct sockaddr *cli, socklen_t *len)` | new fd / -1 |
| `close()` | Close fd (TCP: sends FIN) | `int close(int fd)` | 0 / -1 |
| `bzero()` | Zero out memory (old BSD; same as `memset(p,0,n)`) | `void bzero(void *p, size_t n)` | nothing |
| `htons()` | Host→network, 16-bit (ports) | `uint16_t htons(uint16_t)` | converted value |
| `htonl()` | Host→network, 32-bit (IPs) | `uint32_t htonl(uint32_t)` | converted value |
| `ntohs()` / `ntohl()` | Network→host (16 / 32-bit) | same shape | converted value |
| `read()` / `write()` | Generic file I/O; works on sockets | `ssize_t read(int fd, void *buf, size_t n)` | bytes / 0 = EOF / -1 |
| `send()` / `recv()` | Socket-specific read/write with `flags` (e.g. `MSG_PEEK`, `MSG_DONTWAIT`) | `ssize_t send(int fd, const void *buf, size_t n, int flags)` | bytes / -1 (`recv`: 0 = closed) |
| `sendto()` | UDP send, with destination address | `sendto(fd, buf, n, flags, const struct sockaddr *dest, socklen_t len)` | bytes / -1 |
| `recvfrom()` | UDP receive, also fills in sender address | `recvfrom(fd, buf, n, flags, struct sockaddr *src, socklen_t *len)` | bytes / -1 |
| `select()` | Wait on **many** fds until one is ready (or timeout) | `int select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *timeout)` | # ready / 0 timeout / -1 |
| `setsockopt()` | Change socket options: `SO_REUSEADDR`, `SO_RCVTIMEO`, `SO_BROADCAST` | `setsockopt(fd, level, optname, const void *val, socklen_t len)` | 0 / -1 |
| `fcntl()` | Change fd properties, e.g. non-blocking mode | `fcntl(fd, F_SETFL, fcntl(fd,F_GETFL,0) \| O_NONBLOCK)` | depends / -1 |
| `getpeername()` | Who is at the other end of a connected socket? | `getpeername(fd, struct sockaddr *addr, socklen_t *len)` | 0 / -1 |
| `gethostname()` | Name of this machine | `int gethostname(char *name, size_t len)` | 0 / -1 |
| `gethostbyname()` | Hostname→IP (obsolete; use `getaddrinfo`) | `struct hostent *gethostbyname(const char *name)` | pointer / NULL |
| `gethostbyaddr()` | IP→hostname (obsolete) | `gethostbyaddr(const void *addr, socklen_t len, int type)` | pointer / NULL |

**Applications to mention:** `select()` = a chat server handling many clients in one thread. `setsockopt(SO_REUSEADDR)` = restart server instantly. `fcntl(O_NONBLOCK)` = never block on `recv`. `getpeername()` = log the client IP. `connect()` on UDP = lets you use `send`/`recv` instead of `sendto`/`recvfrom`.

### Mini demo to record observations (try it)
```c
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
int main(void) {
    char name[256];
    gethostname(name, sizeof name);
    printf("My hostname: %s\n", name);

    struct hostent *h = gethostbyname("google.com");
    if (h) printf("google.com -> %s\n", inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

    unsigned short p = 8080;
    printf("htons(8080) = 0x%04x (host 0x%04x)\n", htons(p), p);   // shows byte swap
    return 0;
}
```

### Viva questions
- *Difference between `read` and `recv`?* `recv` has flags, `read` is generic.
- *Why does `accept` return a new fd?* The original keeps listening, and the new one talks to that client.
- *What does `listen` backlog mean?* Max queued, not-yet-accepted connections.
- *`select` vs threads?* One thread monitoring many sockets vs one thread per client.

---

# 3. Assignment 2: Multi-client student lookup (TCP and UDP)

## In one minute
- **TCP version:** server accepts connections; for **each client it creates a thread** (`pthread_create`) so many clients are served at the same time. Client sends a roll number, server replies with the full record or `Student Record Not Found`.
- **UDP version:** same lookup, but over datagrams and the reply is only **Roll + Name**.
- Shared logic (read CSV, search) is in one header so both servers reuse it.

## `student.h` (shared by both servers)
**`student.h`**
```c
#ifndef STUDENT_H
#define STUDENT_H
#include <stdio.h>
#include <string.h>

/* full=1 -> all fields, full=0 -> only Roll + Name.
   Returns 1 if found, 0 if not. Result text is written into out. */
static int lookup_student(const char *roll, int full, char *out, size_t n) {
    FILE *f = fopen("students.csv", "r");
    if (!f) { snprintf(out, n, "Server error: cannot open students.csv\n"); return 0; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *save = NULL;
        char *r  = strtok_r(line, ",", &save);   /* thread-safe strtok */
        char *nm = strtok_r(NULL, ",", &save);
        char *dp = strtok_r(NULL, ",", &save);
        char *sm = strtok_r(NULL, ",", &save);
        char *cg = strtok_r(NULL, ",", &save);
        if (r && nm && dp && sm && cg && strcmp(r, roll) == 0) {
            if (full)
                snprintf(out, n, "Roll: %s\nName: %s\nDept: %s\nSemester: %s\nCGPA: %s\n",
                         r, nm, dp, sm, cg);
            else
                snprintf(out, n, "Roll: %s\nName: %s\n", r, nm);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    snprintf(out, n, "Student Record Not Found\n");
    return 0;
}
#endif
```

## TCP server (multi-threaded)
**`tcp_server.c`**
```c
/* gcc tcp_server.c -o tcp_server -pthread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "student.h"

#define PORT 8080
#define BUF  1024

void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char buf[BUF] = {0}, out[BUF];
    int n = recv(fd, buf, BUF - 1, 0);
    if (n > 0) {
        buf[strcspn(buf, "\r\n")] = 0;              /* strip newline */
        printf("[thread] request for roll '%s'\n", buf);
        lookup_student(buf, 1, out, sizeof out);    /* 1 = full record */
        send(fd, out, strlen(out), 0);
    }
    close(fd);
    return NULL;
}

int main(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    listen(srv, 5);
    printf("TCP server listening on port %d\n", PORT);

    while (1) {
        int *cfd = malloc(sizeof(int));              /* each thread gets its own copy */
        *cfd = accept(srv, NULL, NULL);
        if (*cfd < 0) { perror("accept"); free(cfd); continue; }
        pthread_t t;
        pthread_create(&t, NULL, handle_client, cfd);
        pthread_detach(t);                           /* no need to join */
    }
}
```

## TCP client
**`tcp_client.c`**
```c
/* gcc tcp_client.c -o tcp_client ; ./tcp_client [server_ip] */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define PORT 8080

int main(int argc, char **argv) {
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in s;
    memset(&s, 0, sizeof s);
    s.sin_family = AF_INET;
    s.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &s.sin_addr);

    if (connect(fd, (struct sockaddr *)&s, sizeof s) < 0) { perror("connect"); return 1; }

    char roll[64];
    printf("Enter Roll Number: ");
    fgets(roll, sizeof roll, stdin);
    send(fd, roll, strlen(roll), 0);

    char buf[1024];
    int n;
    while ((n = recv(fd, buf, sizeof buf - 1, 0)) > 0) {   /* until server closes */
        buf[n] = 0;
        printf("%s", buf);
    }
    close(fd);
    return 0;
}
```

## UDP server
**`udp_server.c`**
```c
/* gcc udp_server.c -o udp_server */
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "student.h"
#define PORT 9090

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);           /* SOCK_DGRAM = UDP */
    struct sockaddr_in me, cli;
    memset(&me, 0, sizeof me);
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port = htons(PORT);
    bind(fd, (struct sockaddr *)&me, sizeof me);       /* no listen/accept in UDP! */
    printf("UDP server on port %d\n", PORT);

    char buf[256], out[512];
    while (1) {
        socklen_t len = sizeof cli;
        int n = recvfrom(fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&cli, &len);
        if (n <= 0) continue;
        buf[n] = 0;
        buf[strcspn(buf, "\r\n")] = 0;
        lookup_student(buf, 0, out, sizeof out);       /* 0 = roll + name only */
        sendto(fd, out, strlen(out), 0, (struct sockaddr *)&cli, len);
    }
}
```

## UDP client
**`udp_client.c`**
```c
/* gcc udp_client.c -o udp_client ; ./udp_client [server_ip] */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#define PORT 9090

int main(int argc, char **argv) {
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct timeval tv = {3, 0};                        /* UDP can lose packets -> use a timeout */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    struct sockaddr_in s;
    memset(&s, 0, sizeof s);
    s.sin_family = AF_INET;
    s.sin_port = htons(PORT);
    inet_pton(AF_INET, ip, &s.sin_addr);

    char roll[64];
    printf("Enter Roll Number: ");
    fgets(roll, sizeof roll, stdin);
    sendto(fd, roll, strlen(roll), 0, (struct sockaddr *)&s, sizeof s);

    char buf[512];
    int n = recvfrom(fd, buf, sizeof buf - 1, 0, NULL, NULL);
    if (n < 0) printf("Timeout: no reply (datagram lost?)\n");
    else { buf[n] = 0; printf("%s", buf); }
    close(fd);
    return 0;
}
```

## Run it
```bash
gcc tcp_server.c -o tcp_server -pthread && gcc tcp_client.c -o tcp_client
./tcp_server              # terminal 1
./tcp_client              # terminal 2, 3, 4 ... (try several at once)
gcc udp_server.c -o udp_server && gcc udp_client.c -o udp_client
./udp_server ; ./udp_client
```
Expected: roll `103` → `Roll: 103 / Name: Meena / ...`; roll `999` → `Student Record Not Found`.

## 1c. Comparison table (write this in your record)

| Feature | TCP | UDP |
|---|---|---|
| **Connection** | Connection-oriented (3-way handshake, `connect`/`accept`) | Connectionless (just `sendto`) |
| **Reliability** | Reliable: ACKs, retransmission | Unreliable: lost packets stay lost |
| **Ordering** | Guaranteed in-order delivery (sequence numbers) | No ordering guarantee |
| **Error recovery** | Automatic (retransmit, flow and congestion control) | None; the application must handle timeouts and retry |
| **Communication** | Byte **stream**, no message boundaries | **Datagrams**, boundaries preserved (1 `sendto` = 1 `recvfrom`) |
| **Overhead / speed** | Higher (20-byte header, handshake) | Lower (8-byte header), faster |
| **Server concurrency here** | One thread per accepted connection | One socket serves everyone, replies per datagram |

### Viva questions
- *Why `pthread_detach`?* So thread resources are freed automatically without `join`.
- *Why `malloc` the fd for each thread?* If you passed `&fd` of a shared variable, the next `accept` could overwrite it before the thread reads it (race condition).
- *Why `strtok_r` not `strtok`?* `strtok` uses a hidden global, so it's not thread-safe.
- *How would you handle the UDP request being lost?* Client timeout + retry.

---

# 4. Assignment 3: HTTP client over TCP sockets

## In one minute
HTTP is just **text over TCP**. A browser sends:
```
GET /sample.pdf HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n                         <- blank line = end of headers
```
and the server replies with headers, a blank line, then the **body** (the file). Your client must: **parse URL → connect → send GET → read until the server closes → split headers/body → save body → print timing.**

Metrics: **Response time** = request sent → first byte. **Download time** = first byte → last byte. **Throughput** = bytes×8 / download time.

## Step 1: web server
```bash
mkdir CN_Web_Server && cd CN_Web_Server
echo "<html><body><h1>Welcome to CN Lab</h1></body></html>" > index.html
echo "hello networks" > notes.txt
# put any sample.pdf and image.jpg in here too
python3 -m http.server 8000
```

## Step 2: the client
**`http_client.c`**
```c
/* gcc http_client.c -o http_client
   ./http_client http://localhost:8000/sample.pdf                       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* "http://host[:port]/path"  ->  host, port, path */
static int parse_url(const char *url, char *host, char *port, char *path) {
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = strchr(p, '/');
    size_t hl = slash ? (size_t)(slash - p) : strlen(p);
    char hp[256];
    if (hl >= sizeof hp) return -1;
    memcpy(hp, p, hl); hp[hl] = 0;
    strcpy(path, slash ? slash : "/");
    char *colon = strchr(hp, ':');
    if (colon) { *colon = 0; strcpy(port, colon + 1); } else strcpy(port, "80");
    strcpy(host, hp);
    return 0;
}

int main(int argc, char **argv) {
    char url[512], host[256], port[16], path[512];
    if (argc > 1) strncpy(url, argv[1], sizeof url - 1);
    else { printf("Enter URL: "); scanf("%511s", url); }
    url[sizeof url - 1] = 0;
    if (parse_url(url, host, port, path) < 0) { fprintf(stderr, "bad url\n"); return 1; }
    printf("Host: %s  Port: %s  Path: %s\n", host, port, path);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) { fprintf(stderr, "DNS failed\n"); return 1; }

    int fd = socket(res->ai_family, res->ai_socktype, 0);
    double t0 = now();
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) { perror("connect"); return 1; }
    double t_conn = now();

    char req[1024];
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(fd, req, strlen(req), 0);
    double t_sent = now();

    size_t cap = 65536, len = 0;
    char *data = malloc(cap);
    double t_first = 0;
    ssize_t n;
    while ((n = recv(fd, data + len, cap - len, 0)) > 0) {
        if (t_first == 0) t_first = now();           /* first byte arrived */
        len += n;
        if (len == cap) { cap *= 2; data = realloc(data, cap); }
    }
    double t_end = now();
    close(fd);
    if (len == 0) { fprintf(stderr, "empty response\n"); return 1; }

    /* split header / body at the blank line */
    char *hdr_end = NULL;
    for (size_t i = 0; i + 3 < len; i++)
        if (memcmp(data + i, "\r\n\r\n", 4) == 0) { hdr_end = data + i + 4; break; }
    if (!hdr_end) { fprintf(stderr, "no header end\n"); return 1; }
    size_t hdr_len = hdr_end - data, body_len = len - hdr_len;

    char status[128];
    sscanf(data, "%127[^\r\n]", status);
    printf("Status: %s\n", status);

    const char *name = strrchr(path, '/');
    name = (name && name[1]) ? name + 1 : "index.html";
    char fname[600];
    snprintf(fname, sizeof fname, "downloaded_%s", name);
    FILE *f = fopen(fname, "wb");                     /* "wb": binary-safe (PDF, JPG) */
    fwrite(hdr_end, 1, body_len, f);
    fclose(f);

    double resp = t_first - t_sent, dl = t_end - t_first, total = t_end - t0;
    printf("Saved as        : %s\n", fname);
    printf("TCP connect time: %.4f s\n", t_conn - t0);
    printf("Response time   : %.4f s (request sent -> first byte)\n", resp);
    printf("Download time   : %.4f s (first byte -> last byte)\n", dl);
    printf("Total time      : %.4f s\n", total);
    printf("Data size       : %zu bytes (body) / %zu bytes (with headers)\n", body_len, len);
    printf("Throughput      : %.3f Mbps\n", dl > 0 ? body_len * 8.0 / dl / 1e6 : 0.0);
    free(data);
    return 0;
}
```

## Run
```bash
gcc http_client.c -o http_client
./http_client http://localhost:8000/sample.pdf
./http_client http://localhost:8000/index.html
./http_client http://example.com/          # part (a): any real web server (HTTP, port 80)
```
Verify binary files are intact: `cmp sample.pdf downloaded_sample.pdf` (no output = identical).

**Important details:** use `"wb"` for files (PDF/JPG are binary). The header/body split looks for `\r\n\r\n`. `Connection: close` means the server closes the socket when done, so `recv` returning 0 = file complete. Real sites that use `https://` or chunked encoding won't work with this simple client; use `http://` and the Python server.

## Advanced (optional): HTTPS client with OpenSSL
Flow: TCP connect (port 443) → `SSL_connect` (TLS handshake) → **verify certificate** → `SSL_write` GET → `SSL_read` → measure times.
Install headers: `sudo apt install libssl-dev`.
**`https_client.c`**
```c
/* gcc https_client.c -o https_client -lssl -lcrypto ; ./https_client example.com */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "example.com";
    struct addrinfo h, *res;
    memset(&h, 0, sizeof h); h.ai_family = AF_INET; h.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &h, &res)) { fprintf(stderr, "DNS failed\n"); return 1; }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    double t0 = now();
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) { perror("connect"); return 1; }
    double t_tcp = now() - t0;                              /* ~ 1 RTT (SYN, SYN-ACK) */

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_default_verify_paths(ctx);                  /* trust the system CA store */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);         /* REQUIRE a valid certificate */
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host);                    /* SNI */
    SSL_set1_host(ssl, host);                               /* check cert matches hostname */

    t0 = now();
    if (SSL_connect(ssl) != 1) { ERR_print_errors_fp(stderr); return 1; }
    double t_hs = now() - t0;                               /* TLS handshake time */

    printf("TLS version : %s\nCipher      : %s\n", SSL_get_version(ssl), SSL_get_cipher_name(ssl));
    printf("Cert verify : %s\n", SSL_get_verify_result(ssl) == X509_V_OK ? "OK" : "FAILED");
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    X509 *cert = SSL_get1_peer_certificate(ssl);
#else
    X509 *cert = SSL_get_peer_certificate(ssl);
#endif
    if (cert) {
        char *subj = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
        char *iss  = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
        printf("Subject     : %s\nIssuer      : %s\n", subj, iss);
        OPENSSL_free(subj); OPENSSL_free(iss); X509_free(cert);
    }

    char req[512], buf[4096];
    snprintf(req, sizeof req, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    double t1 = now();
    SSL_write(ssl, req, strlen(req));
    size_t total = 0; int n;
    while ((n = SSL_read(ssl, buf, sizeof buf)) > 0) total += n;
    double t_dl = now() - t1;

    printf("TCP connect : %.4f s\nSSL handshake: %.4f s\nBytes       : %zu\n", t_tcp, t_hs, total);
    printf("Transfer    : %.4f s  (%.3f Mbps)\n", t_dl, t_dl > 0 ? total * 8.0 / t_dl / 1e6 : 0);
    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); close(fd);
    return 0;
}
```
```bash
gcc https_client.c -o https_client -lssl -lcrypto && ./https_client example.com
```
(Compile-check this one on your machine; OpenSSL headers weren't available where I tested.)
**Observations to record:** TCP connect time ≈ 1 RTT; TLS handshake takes extra round trips (≈ 1–2 RTT) plus crypto; TLS version (1.3) and cipher; certificate verify result.

### Viva questions
- *HTTP/1.0 vs 1.1?* 1.1 has persistent connections and a mandatory `Host:` header.
- *Why the `Host:` header?* Many sites share one IP (virtual hosting).
- *What does `Content-Length` do?* Tells how many body bytes follow.
- *Why TLS handshake is slower?* Key exchange + certificate validation before any data.

---

# 5. Assignment 4: Wireshark HTTP analysis

## Setup (summary)
```bash
sudo apt update && sudo apt install wireshark -y     # answer YES to non-root capture
sudo usermod -aG wireshark $USER && newgrp wireshark
wireshark
```
Pick the interface (e.g. `wlan0`/`eth0`), type the display filter **`http`**, start capture, then browse.

**Modern-browser problem:** `gaia.cs.umass.edu` lab pages are **plain HTTP**, but browsers may auto-upgrade to HTTPS or hide it. Fixes: type `http://` explicitly, use a private window, or use `curl -v http://gaia.cs.umass.edu/wireshark-labs/HTTP-wireshark-file1.html` (Wireshark still captures it). Clear the cache before parts 2–4.

## Filters
| Purpose | Filter |
|---|---|
| HTTP | `http` |
| DNS | `dns` |
| TCP / UDP | `tcp` / `udp` |
| Specific host | `ip.addr == 1.2.3.4` |
| Retransmissions | `tcp.analysis.retransmission` |
| Only requests | `http.request` |

## Where to find each answer (click a packet → expand the layers in the middle pane)
| Q | Where to look |
|---|---|
| 1. HTTP versions | GET line `GET /... HTTP/1.1` (browser) and response `HTTP/1.1 200 OK` (server) |
| 2. Accepted languages | `Accept-Language:` header in the GET request |
| 3. IP addresses | `Internet Protocol` layer: **Source** (your PC) and **Destination** (server) in the GET; reversed in the response |
| 4. Status code | First line of response (`200 OK`) |
| 5. Last modified | `Last-Modified:` header in the response |
| 6. Bytes returned | `Content-Length:` header (file1 is a tiny one-line file) |
| 7. Headers not shown | Look at the "Packet Bytes" pane (bottom): hidden extras such as `Accept-Encoding`, `Connection`, `Server` |
| 8. If-Modified-Since in 1st GET? | **No** (cache empty) |
| 9. Did server return content? | **Yes**: `200 OK`, plus a `Line-based text data` section at the bottom |
| 10. 2nd GET If-Modified-Since? | **Yes**: followed by the `Last-Modified` date of the cached copy |
| 11. Status of 2nd response | **`304 Not Modified`**: no body returned, so the browser uses its cached copy (saves bandwidth) |
| 12–14. Long document | One GET; the response `200 OK`; note packet numbers from the packet list |
| 15. TCP segments | In the response packet details, "`[N Reassembled TCP Segments]`" tells you N (typically 4 for the Bill of Rights, check yours) |
| 16. GETs and addresses (file4) | Typically **3 GETs**: HTML + 2 images; addresses of the two different servers |
| 17. Serial or parallel? | If the 2nd image GET is sent **before** the 1st image's response finishes → parallel. Check the timestamps. |

Your IPs, dates and packet numbers will differ. Always answer with **your capture's** values.

**Concept recap for viva:** Conditional GET = browser asks "send only if changed since X"; server says `304`. HTTP is **stateless**. Wireshark reassembles TCP segments into one HTTP message.

---

# 6. Assignment 5: DNS simulation over UDP

## In one minute
- **Server** holds a table (`hostname → IP`) and answers UDP queries. Replies: `OK <ip>`, `NXDOMAIN`, or `ERROR INVALID_FORMAT`.
- **Client** keeps a **cache with TTL** (30 s). Asks the cache first, then the server. **Timeout** is handled with `SO_RCVTIMEO` + 3 retries.
- Special name `timeout.test` makes the server stay silent, which demonstrates the timeout.

## Server
**`dns_server.c`**
```c
/* gcc dns_server.c -o dns_server */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define PORT 5353

typedef struct { const char *name, *ip; } Rec;
Rec db[] = {                         /* the "DNS table" */
    {"google.com", "142.250.183.14"}, {"example.com", "93.184.216.34"},
    {"github.com", "140.82.112.3"},   {"ssn.edu.in", "10.0.0.5"},
    {"openai.com", "104.18.33.45"}
};
#define DB_N (sizeof db / sizeof db[0])

static int valid_name(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > 253 || !strchr(s, '.')) return 0;
    if (s[0] == '.' || s[n - 1] == '.' || strstr(s, "..")) return 0;
    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '.' && s[i] != '-') return 0;
    return 1;
}

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me, cli;
    memset(&me, 0, sizeof me);
    me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY); me.sin_port = htons(PORT);
    bind(fd, (struct sockaddr *)&me, sizeof me);
    printf("DNS server on UDP %d\n", PORT);

    char buf[512], reply[128];
    while (1) {
        socklen_t len = sizeof cli;
        int n = recvfrom(fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&cli, &len);
        if (n <= 0) continue;
        buf[n] = 0; buf[strcspn(buf, "\r\n")] = 0;
        printf("query: '%s'\n", buf);

        if (strcmp(buf, "timeout.test") == 0) {        /* simulate a lost reply */
            printf("  -> ignoring (timeout simulation)\n");
            continue;
        }
        if (!valid_name(buf)) { strcpy(reply, "ERROR INVALID_FORMAT"); }
        else {
            strcpy(reply, "NXDOMAIN");
            for (size_t i = 0; i < DB_N; i++)
                if (strcasecmp(buf, db[i].name) == 0) { snprintf(reply, sizeof reply, "OK %s", db[i].ip); break; }
        }
        sendto(fd, reply, strlen(reply), 0, (struct sockaddr *)&cli, len);
    }
}
```

## Client
**`dns_client.c`**
```c
/* gcc dns_client.c -o dns_client ; ./dns_client */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#define PORT 5353
#define CACHE_N 8
#define TTL 30            /* seconds a cache entry stays valid */
#define RETRIES 3

typedef struct { char name[256], ip[32]; time_t expires; int used; } Entry;
static Entry cache[CACHE_N];
static int next_slot = 0;

static const char *cache_get(const char *name) {
    for (int i = 0; i < CACHE_N; i++)
        if (cache[i].used && strcasecmp(cache[i].name, name) == 0 && time(NULL) < cache[i].expires)
            return cache[i].ip;
    return NULL;
}
static void cache_put(const char *name, const char *ip) {
    Entry *e = &cache[next_slot];
    next_slot = (next_slot + 1) % CACHE_N;                 /* simple round-robin replacement */
    strncpy(e->name, name, 255); strncpy(e->ip, ip, 31);
    e->expires = time(NULL) + TTL; e->used = 1;
}

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);   /* 2 s timeout */
    struct sockaddr_in srv;
    memset(&srv, 0, sizeof srv);
    srv.sin_family = AF_INET; srv.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &srv.sin_addr);

    char name[256], reply[128];
    while (1) {
        printf("\nDomain (or 'exit'): ");
        if (!fgets(name, sizeof name, stdin)) break;
        name[strcspn(name, "\r\n")] = 0;
        if (strcmp(name, "exit") == 0) break;

        const char *hit = cache_get(name);
        if (hit) { printf("[CACHE HIT] %s -> %s\n", name, hit); continue; }

        int ok = 0;
        for (int attempt = 1; attempt <= RETRIES && !ok; attempt++) {
            struct timeval a, b;
            gettimeofday(&a, NULL);
            sendto(fd, name, strlen(name), 0, (struct sockaddr *)&srv, sizeof srv);
            int n = recvfrom(fd, reply, sizeof reply - 1, 0, NULL, NULL);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) printf("Timeout (attempt %d/%d)\n", attempt, RETRIES);
                else perror("recvfrom");
                continue;
            }
            gettimeofday(&b, NULL);
            reply[n] = 0; ok = 1;
            double ms = (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_usec - a.tv_usec) / 1000.0;
            if (strncmp(reply, "OK ", 3) == 0) {
                printf("[SERVER] %s -> %s  (RTT %.3f ms)\n", name, reply + 3, ms);
                cache_put(name, reply + 3);
            } else if (strcmp(reply, "NXDOMAIN") == 0) printf("NXDOMAIN: '%s' does not exist\n", name);
            else printf("Server says: %s\n", reply);
        }
        if (!ok) printf("Giving up on '%s' after %d attempts\n", name, RETRIES);
    }
    return 0;
}
```

## Run and test
```bash
gcc dns_server.c -o dns_server && gcc dns_client.c -o dns_client
./dns_server        # terminal 1
./dns_client        # terminal 2
# try: google.com (server), google.com again (cache hit), foo.bar (NXDOMAIN),
#      bad..name (invalid format), timeout.test (3 timeouts)
```

## Why UDP for DNS? (the analysis part)
| Criterion | Explanation |
|---|---|
| **Overhead** | A query is tiny (~50 B). UDP has an 8-byte header and no handshake, so one request + one reply. TCP would add 3-way handshake + teardown (≥ 3 extra packets per lookup). |
| **Latency** | UDP answer arrives in **1 RTT**; TCP needs ~2 RTT (handshake + query). DNS happens before every web page load, so the saving adds up. |
| **Reliability** | UDP is unreliable, but DNS handles that itself with **timeouts and retries** at the application layer. Simple and cheap. |
| **Scalability** | Servers keep **no per-client connection state**, so millions of queries are possible. |
| **When TCP is used instead** | Responses > 512 B (or EDNS limit), zone transfers (AXFR), and DNS-over-TLS/HTTPS for security. |

### Viva questions
- *Which port does real DNS use?* 53.
- *What is NXDOMAIN?* Domain does not exist.
- *Why cache?* Fewer queries, lower latency. TTL stops stale data.
- *What if a reply is lost?* Client times out and retransmits.

---

# 7. Assignment 6: Checksum and CRC

## Concepts

### Checksum (8-bit, 1's complement)
1. Split data into 8-bit words (we use each character's byte).
2. Add them. If the sum goes beyond 8 bits, the **carry wraps around** and is added back (**end-around carry**).
3. **Checksum = bitwise complement (NOT) of the final sum.**
4. Receiver adds all words **plus the checksum**. If no error, the result is `1111 1111` (0xFF) and its complement is 0.

Worked example: data `Hi` = 72 (0x48), 105 (0x69). 72 + 105 = 177 (0xB1). Checksum = ~0xB1 = 0x4E = 78. Receiver: 177 + 78 = 255 → OK.

### CRC (cyclic redundancy check)
Generator `10011` = x⁴ + x + 1 (degree 4 → append 4 zeros).
1. Data bits, append **4 zeros**.
2. Do **XOR long division** by the generator (no borrows!). Whenever the leading bit is 1, XOR with the generator; otherwise shift.
3. The **remainder** (4 bits) = CRC. Replace the 4 appended zeros with it → **codeword**.
4. Receiver divides the received codeword by the same generator. **Remainder 0 → no error detected.** Non-zero → error.

Hand-example: data `1101011011`, G = `10011` → append `0000` → remainder `1110` → send `11010110111110`.

## Code (shared library `ecc.h`)
**`ecc.h`**
```c
#ifndef ECC_H
#define ECC_H
#include <stdio.h>
#include <string.h>

/* ---------- 8-bit one's complement addition with end-around carry ---------- */
static unsigned add8(unsigned sum, unsigned w, int verbose) {
    unsigned raw = sum + w;            /* may be 9 bits */
    unsigned carry = raw >> 8;         /* the bit that fell off the left */
    unsigned res = (raw & 0xFF) + carry;   /* wrap it around to the right */
    if (verbose)
        printf("  %3u + %3u = %3u  carry=%u  -> wrapped sum = %3u (0x%02X)\n", sum, w, raw, carry, res, res);
    return res;
}
static unsigned sum_words(const unsigned char *d, int n, int verbose) {
    unsigned s = 0;
    for (int i = 0; i < n; i++) s = add8(s, d[i], verbose);
    return s;
}

/* ---------- CRC: mod-2 (XOR) long division on '0'/'1' strings ----------
   data  : bit string
   extra : number of zeros to append first (r = len(gen)-1 at the sender, 0 at the receiver)
   rem   : output remainder, len(gen)-1 chars                                   */
static void crc_div(const char *data, int extra, const char *gen, char *rem) {
    char t[2048];
    int dl = strlen(data), g = strlen(gen), n = dl + extra;
    strcpy(t, data);
    for (int i = 0; i < extra; i++) t[dl + i] = '0';
    t[n] = 0;
    for (int i = 0; i <= n - g; i++)
        if (t[i] == '1')
            for (int j = 0; j < g; j++) t[i + j] = (t[i + j] == gen[j]) ? '0' : '1';
    strcpy(rem, t + n - (g - 1));
}
#endif
```

## Sender (TCP client)
**`sender.c`**
```c
/* gcc sender.c -o sender ; run receiver first */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ecc.h"
#define PORT 7070
#define GEN "10011"            /* x^4 + x + 1 */

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in s;
    memset(&s, 0, sizeof s);
    s.sin_family = AF_INET; s.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &s.sin_addr);
    if (connect(fd, (struct sockaddr *)&s, sizeof s) < 0) { perror("connect"); return 1; }

    int mode; char text[256], line[2400], inj;
    printf("1 = Checksum, 2 = CRC : "); scanf("%d", &mode);
    printf("Enter data (one word, no spaces): "); scanf("%255s", text);
    printf("Inject error? (y/n): "); scanf(" %c", &inj);

    if (mode == 1) {
        int n = strlen(text);
        printf("\nSender: summing %d 8-bit words\n", n);
        unsigned sum = sum_words((unsigned char *)text, n, 1);
        unsigned chk = (~sum) & 0xFF;
        printf("Sum = %u (0x%02X)  ->  Checksum (1's complement) = %u (0x%02X)\n", sum, sum, chk, chk);
        if (inj == 'y') { text[0] ^= 1; printf(">> flipped 1 bit in first character -> '%s'\n", text); }
        snprintf(line, sizeof line, "1|%s|%u\n", text, chk);
    } else {
        char bits[2048] = "", rem[64], code[2100];
        for (size_t i = 0; i < strlen(text); i++)
            for (int b = 7; b >= 0; b--) strcat(bits, ((text[i] >> b) & 1) ? "1" : "0");
        int r = strlen(GEN) - 1;
        crc_div(bits, r, GEN, rem);
        printf("\nData bits : %s\nGenerator : %s\nCRC       : %s\n", bits, GEN, rem);
        snprintf(code, sizeof code, "%s%s", bits, rem);
        printf("Codeword  : %s\n", code);
        if (inj == 'y') {
            int pos; printf("Bit position to flip (0..%zu): ", strlen(code) - 1); scanf("%d", &pos);
            code[pos] = code[pos] == '0' ? '1' : '0';
            printf("Corrupted : %s\n", code);
        }
        snprintf(line, sizeof line, "2|%s|%s\n", code, GEN);
    }
    send(fd, line, strlen(line), 0);
    char reply[200];
    int n = recv(fd, reply, sizeof reply - 1, 0);
    if (n > 0) { reply[n] = 0; printf("\nReceiver replied: %s", reply); }
    close(fd);
    return 0;
}
```

## Receiver (TCP server)
**`receiver.c`**
```c
/* gcc receiver.c -o receiver ; ./receiver */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ecc.h"
#define PORT 7070

int main(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0), yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(PORT);
    bind(srv, (struct sockaddr *)&a, sizeof a);
    listen(srv, 1);
    printf("Receiver waiting on port %d...\n", PORT);

    int c = accept(srv, NULL, NULL);
    char buf[2400];
    int n = recv(c, buf, sizeof buf - 1, 0);
    if (n <= 0) return 1;
    buf[n] = 0; buf[strcspn(buf, "\r\n")] = 0;
    printf("Received: %s\n", buf);

    char *p1 = strchr(buf, '|'), *p2 = strrchr(buf, '|');
    *p1 = 0; *p2 = 0;
    char *payload = p1 + 1, *extra = p2 + 1;
    char verdict[200];

    if (buf[0] == '1') {                                     /* ----- checksum ----- */
        unsigned chk = atoi(extra);
        int len = strlen(payload);
        printf("Receiver: summing data words + checksum\n");
        unsigned sum = sum_words((unsigned char *)payload, len, 1);
        sum = add8(sum, chk, 1);
        printf("Final sum = %u (0x%02X) -> complement = %u\n", sum, sum, (~sum) & 0xFF);
        snprintf(verdict, sizeof verdict, (((~sum) & 0xFF) == 0) ? "NO ERROR\n" : "ERROR DETECTED\n");
    } else {                                                 /* ----- CRC ----- */
        char rem[64];
        crc_div(payload, 0, extra, rem);
        printf("Generator: %s  Remainder: %s\n", extra, rem);
        int zero = 1;
        for (size_t i = 0; i < strlen(rem); i++) if (rem[i] != '0') zero = 0;
        snprintf(verdict, sizeof verdict, zero ? "NO ERROR (remainder 0)\n" : "ERROR DETECTED (remainder != 0)\n");
    }
    printf("Result: %s", verdict);
    send(c, verdict, strlen(verdict), 0);
    close(c); close(srv);
    return 0;
}
```

## Run
```bash
gcc receiver.c -o receiver && gcc sender.c -o sender
./receiver          # terminal 1 (start first)
./sender            # terminal 2: choose 1 (checksum) or 2 (CRC), type data, choose y to inject an error
```
Test matrix to show in the exam: (checksum, no error) → NO ERROR; (checksum, inject) → ERROR; (CRC, no error) → remainder 0; (CRC, flip bit 3) → remainder ≠ 0.

### Viva questions
- *Which errors can checksum miss?* Two bit flips that cancel out (e.g. one 0→1 and one 1→0 in the same column), or swapped words.
- *Why is CRC stronger?* Detects all single-bit, all double-bit (for suitable G), all odd-count, and burst errors up to the generator length.
- *Why is the checksum complemented?* So the receiver gets an all-1s check value for a good packet, which is easy to test.
- *Where used?* Checksum: IP/TCP/UDP headers. CRC: Ethernet frames, Wi-Fi, storage.

---

# 8. NS-3 fundamentals

## The universal recipe (every NS-3 program is this)
```
1. Create nodes              NodeContainer nodes; nodes.Create(2);
2. Create links (devices)    PointToPointHelper p2p; p2p.Install(a, b)
3. Install internet stack    InternetStackHelper stack; stack.Install(nodes);
4. Assign IP addresses       Ipv4AddressHelper a; a.SetBase("10.1.1.0","255.255.255.0"); a.Assign(devices)
5. (Routing)                 Ipv4GlobalRoutingHelper::PopulateRoutingTables();
6. Install applications      server (sink) + client (source), Start/Stop times
7. Enable tracing / monitors pcap, ascii, FlowMonitor
8. Run                       Simulator::Stop(Seconds(T)); Simulator::Run(); Simulator::Destroy();
```
**Rule:** one **subnet per link**. **Sink/server must be installed before traffic starts.**

## Minimal skeleton to type from memory
```cpp
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
using namespace ns3;

int main(int argc, char *argv[]) {
  NodeContainer nodes; nodes.Create(2);
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer dev = p2p.Install(nodes);
  InternetStackHelper stack; stack.Install(nodes);
  Ipv4AddressHelper addr; addr.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifc = addr.Assign(dev);

  UdpEchoServerHelper server(9);
  server.Install(nodes.Get(1)).Start(Seconds(1));
  UdpEchoClientHelper client(ifc.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(5));
  client.SetAttribute("Interval", TimeValue(Seconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(512));
  client.Install(nodes.Get(0)).Start(Seconds(2));

  Simulator::Run(); Simulator::Destroy();
}
```

## Cheat table: which class to use
| Need | Use |
|---|---|
| Wired link | `PointToPointHelper` (DataRate, Delay, `SetQueue`) |
| Constant UDP packets | `UdpClient/UdpServer` (Interval, PacketSize) or `OnOffHelper` (DataRate) |
| Constant-rate with data rate setting | `OnOffHelper` with OffTime 0 + `PacketSinkHelper` |
| TCP source as fast as possible | `BulkSendHelper` + `PacketSinkHelper` |
| Auto per-flow stats | `FlowMonitor` (tx/rx packets, lost, delay, bytes) |
| Packet trace for Wireshark | `p2p.EnablePcapAll("name")` |
| Text trace (+ enqueue, - dequeue, d drop, r receive) | `p2p.EnableAsciiAll(ascii.CreateFileStream("x.tr"))` |
| Mobility | `MobilityHelper` + `RandomWalk2dMobilityModel` / `RandomWaypointMobilityModel` |
| Command-line args | `CommandLine cmd; cmd.AddValue("name","help",var); cmd.Parse(argc,argv);` |

## FlowMonitor recipe (copy for every performance question)
```cpp
FlowMonitorHelper fmh;
Ptr<FlowMonitor> mon = fmh.InstallAll();     // before Run
Simulator::Run();
mon->CheckForLostPackets();
auto cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
for (auto &kv : mon->GetFlowStats()) {
  auto t = cls->FindFlow(kv.first);          // src/dst IP, ports, protocol (6=TCP,17=UDP)
  const auto &s = kv.second;
  double loss  = 100.0 * s.lostPackets / s.txPackets;
  double delay = s.delaySum.GetSeconds() / s.rxPackets;                    // avg e2e delay
  double thr   = s.rxBytes * 8.0 / (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds() / 1e6;  // Mbps
}
```

## Formulas to remember
- Throughput = received bytes × 8 / time
- Packet loss % = (tx − rx) / tx × 100
- Avg delay = Σ delays / rx packets
- PDR = rx / tx
- Interval for a rate: `interval = packetSize × 8 / rate`
- Jain fairness = (Σxᵢ)² / (n·Σxᵢ²) (1 = perfectly fair)

## Version differences (the likely compile errors)
| Error | Fix |
|---|---|
| `./ns3: not found` | Old version, use `./waf --run scratch/file` |
| `WIFI_STANDARD_80211g` not declared | Use `WIFI_PHY_STANDARD_80211g` (ns-3 before ~3.36) |
| `DropTailQueue<Packet>` not found | Use `"ns3::DropTailQueue"` and `"MaxPackets"`, `UintegerValue(50)` (very old) |
| `module not found` | Add the module (`internet-apps-module.h`, `aodv-module.h`, `lr-wpan-module.h`, `sixlowpan-module.h`) and make sure it's enabled in your build |
| Nothing received | Sink not started, wrong port, or missing routing |

---

# 9. Assignment 7: UDP, link latency, port-based delivery

## In one minute
Two nodes, one P2P link. Install a `UdpServer` (listening on port 5000) on Node2 and a `UdpClient` on Node1 sending at a fixed rate. Throughput = packets received × size × 8 / time. Then add a second pair on **port 6000**. Both use the same IP, and the **destination port** decides which server app gets the packet.

## Important result to explain
Because UDP has **no ACKs and no window**, throughput **does not depend on latency**. It equals the offered rate as long as the link bandwidth isn't exceeded. Changing latency from 2 → 20 ms will give (almost) the same Mbps. (Tiny differences: packets still in flight when the sender stops.) That's the correct observation, and contrast it with TCP, where higher RTT lowers throughput (window / RTT).

To see a drop: push the rate above the link capacity (`--rate=6` with two flows on 10 Mbps) and both flows will lose packets.

## Code
**`udp_latency.cc`**
```cpp
// Assignment 7: UDP between 2 nodes, throughput vs link latency, then 2 flows on ports 5000/6000
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <vector>
#include <iostream>
using namespace ns3;

struct Result { double f1; double f2; };

Result RunOnce(const std::string &delay, bool twoFlows, bool pcap, double rateMbps)
{
  const uint32_t pkt = 1024;                 // payload bytes
  const double tStart = 1.0, tStop = 10.0;

  NodeContainer nodes; nodes.Create(2);      // Node1 = nodes.Get(0), Node2 = nodes.Get(1)

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue(delay));      // <-- the thing we vary
  NetDeviceContainer dev = p2p.Install(nodes);

  InternetStackHelper stack; stack.Install(nodes);
  Ipv4AddressHelper addr; addr.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifc = addr.Assign(dev);

  // UdpClient has no "DataRate" attribute -> rate = pktSize*8 / Interval
  double interval = pkt * 8.0 / (rateMbps * 1e6);            // seconds between packets

  uint16_t ports[2] = {5000, 6000};
  std::vector<Ptr<UdpServer>> servers;
  for (int i = 0; i < (twoFlows ? 2 : 1); ++i)
  {
    UdpServerHelper srv(ports[i]);                           // server listens on this port
    ApplicationContainer sa = srv.Install(nodes.Get(1));
    sa.Start(Seconds(0.5)); sa.Stop(Seconds(tStop + 1));
    servers.push_back(DynamicCast<UdpServer>(sa.Get(0)));

    UdpClientHelper cli(ifc.GetAddress(1), ports[i]);        // destination IP + PORT
    cli.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    cli.SetAttribute("Interval", TimeValue(Seconds(interval)));
    cli.SetAttribute("PacketSize", UintegerValue(pkt));
    ApplicationContainer ca = cli.Install(nodes.Get(0));
    ca.Start(Seconds(tStart)); ca.Stop(Seconds(tStop));
  }

  if (pcap) p2p.EnablePcapAll("udp-ports");                  // -> udp-ports-0-0.pcap, udp-ports-1-0.pcap

  Simulator::Stop(Seconds(tStop + 1));
  Simulator::Run();

  Result r{0, 0};
  r.f1 = servers[0]->GetReceived() * pkt * 8.0 / ((tStop - tStart) * 1e6);   // Mbps
  if (twoFlows) r.f2 = servers[1]->GetReceived() * pkt * 8.0 / ((tStop - tStart) * 1e6);
  Simulator::Destroy();
  return r;
}

int main(int argc, char *argv[])
{
  double rate = 4.0;                                         // Mbps per flow
  CommandLine cmd; cmd.AddValue("rate", "Mbps per flow", rate); cmd.Parse(argc, argv);

  std::string delays[] = {"2ms", "5ms", "10ms", "20ms"};
  std::cout << "Latency | Flow1 (1 flow) || Flow1 | Flow2 (2 flows)\n";
  for (auto &d : delays)
  {
    Result one = RunOnce(d, false, false, rate);
    Result two = RunOnce(d, true, d == "2ms", rate);         // pcap only for the first 2-flow run
    std::cout << d << " | " << one.f1 << " Mbps || " << two.f1 << " | " << two.f2 << " Mbps\n";
  }
  return 0;
}
```

## Run
```bash
cp udp_latency.cc ~/ns-3-dev/scratch/      # your ns-3 folder
./ns3 run "scratch/udp_latency --rate=4"
./ns3 run "scratch/udp_latency --rate=6"   # saturates the link with 2 flows
```
Fill the table with the printed output:

| Link latency | Flow 1 throughput | Flow 2 throughput |
|---|---|---|
| 2 ms | | |
| 5 ms | | |
| 10 ms | | |
| 20 ms | | |

## Wireshark verification
Open `udp-ports-1-0.pcap` (Node2's capture). Filter `udp.dstport == 5000` and then `udp.dstport == 6000`. Both streams have the same destination IP (10.1.1.2) but different ports, which is how the OS demultiplexes them to Server 1 and Server 2. Also check `Statistics → Conversations → UDP`.

### Viva questions
- *Why does UDP throughput not change with latency?* No feedback loop; sender never waits.
- *What is port-based delivery (demultiplexing)?* IP gets the packet to the host, the port number gets it to the right application.
- *How does throughput of TCP depend on latency?* Roughly window/RTT.

---

# 10. Assignment 8: TCP + UDP on a dumbbell topology

## In one minute
```
n0 --10Mbps,2ms--\                      /--10Mbps,2ms-- n2   (UDP sink)
                  r1 ==2Mbps,10ms== r2
n1 --10Mbps,2ms--/                      \--10Mbps,2ms-- n3   (TCP sink)
TCP: n0 → n3 (BulkSend, 1024 B)    UDP: n1 → n2 (OnOff, 512 B, 1 Mbps)
```
The **2 Mbps bottleneck** is shared. TCP will back off and adapt; UDP keeps blasting at its rate (not congestion-controlled). Total demand > 2 Mbps, so the DropTail queue (50 pkts) overflows → drops → TCP retransmits.

## Code
**`dumbbell.cc`**
```cpp
// Assignment 8: n0,n1 -> r1 =bottleneck= r2 -> n2,n3. TCP n0->n3, UDP n1->n2
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>
using namespace ns3;

int main(int argc, char *argv[])
{
  std::string bw = "2Mbps", bnDelay = "10ms", udpRate = "1Mbps";
  uint32_t queueSize = 50;
  double simTime = 20.0;
  CommandLine cmd;
  cmd.AddValue("bw", "bottleneck bandwidth", bw);
  cmd.AddValue("delay", "bottleneck delay", bnDelay);
  cmd.AddValue("udpRate", "UDP data rate", udpRate);
  cmd.AddValue("queue", "DropTail queue size (packets)", queueSize);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));   // 1024-byte TCP segments

  NodeContainer n; n.Create(6);       // 0:n0 1:n1 2:n2 3:n3 4:r1 5:r2
  Ptr<Node> n0 = n.Get(0), n1 = n.Get(1), n2 = n.Get(2), n3 = n.Get(3), r1 = n.Get(4), r2 = n.Get(5);

  std::string q = std::to_string(queueSize) + "p";
  PointToPointHelper access, bott;
  access.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  access.SetChannelAttribute("Delay", StringValue("2ms"));
  bott.SetDeviceAttribute("DataRate", StringValue(bw));
  bott.SetChannelAttribute("Delay", StringValue(bnDelay));
  access.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue(q));
  bott.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue(q));

  NetDeviceContainer d0 = access.Install(n0, r1), d1 = access.Install(n1, r1);
  NetDeviceContainer db = bott.Install(r1, r2);
  NetDeviceContainer d2 = access.Install(r2, n2), d3 = access.Install(r2, n3);

  InternetStackHelper stack; stack.Install(n);
  Ipv4AddressHelper a;
  a.SetBase("10.1.1.0", "255.255.255.0"); a.Assign(d0);
  a.SetBase("10.1.2.0", "255.255.255.0"); a.Assign(d1);
  a.SetBase("10.1.3.0", "255.255.255.0"); a.Assign(db);
  a.SetBase("10.1.4.0", "255.255.255.0"); Ipv4InterfaceContainer i2 = a.Assign(d2);   // [1] = n2
  a.SetBase("10.1.5.0", "255.255.255.0"); Ipv4InterfaceContainer i3 = a.Assign(d3);   // [1] = n3
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t tcpPort = 8080, udpPort = 9000;

  // ---- TCP flow n0 -> n3 (BulkSend = send as fast as TCP allows)
  BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(i3.GetAddress(1), tcpPort));
  bulk.SetAttribute("MaxBytes", UintegerValue(0));
  bulk.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer tcpApp = bulk.Install(n0);
  tcpApp.Start(Seconds(1.0)); tcpApp.Stop(Seconds(simTime));
  PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
  tcpSink.Install(n3).Start(Seconds(0.5));

  // ---- UDP flow n1 -> n2 (OnOff with OffTime 0 = constant bit rate)
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(i2.GetAddress(1), udpPort));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", StringValue(udpRate));
  onoff.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer udpApp = onoff.Install(n1);
  udpApp.Start(Seconds(1.0)); udpApp.Stop(Seconds(simTime));
  PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
  udpSink.Install(n2).Start(Seconds(0.5));

  // ---- Tracing (packet-level)
  AsciiTraceHelper ascii;
  access.EnableAsciiAll(ascii.CreateFileStream("dumbbell.tr"));   // + - d r events
  bott.EnablePcapAll("dumbbell");                                  // open in Wireshark

  // ---- FlowMonitor = automatic per-flow statistics
  FlowMonitorHelper fmh;
  Ptr<FlowMonitor> mon = fmh.InstallAll();
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  mon->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  for (auto &kv : mon->GetFlowStats())
  {
    Ipv4FlowClassifier::FiveTuple t = cls->FindFlow(kv.first);
    bool isTcp = (t.destinationPort == tcpPort), isUdp = (t.destinationPort == udpPort);
    if (!isTcp && !isUdp) continue;                   // skip the reverse ACK flow
    const FlowMonitor::FlowStats &s = kv.second;
    double lossPct = s.txPackets ? 100.0 * s.lostPackets / s.txPackets : 0;
    double delayMs = s.rxPackets ? 1000.0 * s.delaySum.GetSeconds() / s.rxPackets : 0;
    double dur = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
    double thr = dur > 0 ? s.rxBytes * 8.0 / dur / 1e6 : 0;
    std::cout << (isTcp ? "TCP" : "UDP") << " | tx=" << s.txPackets << " rx=" << s.rxPackets
              << " lost=" << s.lostPackets << " loss%=" << lossPct
              << " | avg delay(ms)=" << delayMs << " | thr(Mbps)=" << thr;
    if (isTcp) std::cout << " | retx-proxy%=" << lossPct;   // lost packets get retransmitted
    std::cout << "\n";
  }
  Simulator::Destroy();
  return 0;
}
```

## Experiments (use command-line args, no recompiling)
```bash
./ns3 run "scratch/dumbbell"                              # baseline
# Exp 1: bottleneck bandwidth
./ns3 run "scratch/dumbbell --bw=2Mbps" ; ./ns3 run "scratch/dumbbell --bw=5Mbps" ; ./ns3 run "scratch/dumbbell --bw=10Mbps"
# Exp 2: UDP rate
for r in 500kbps 1Mbps 2Mbps 5Mbps; do ./ns3 run "scratch/dumbbell --udpRate=$r"; done
# Exp 3: queue size
for q in 20 50 100; do ./ns3 run "scratch/dumbbell --queue=$q"; done
# Exp 4: propagation delay on the bottleneck
for d in 5ms 10ms 20ms 50ms; do ./ns3 run "scratch/dumbbell --delay=$d"; done
```
Record into a table: loss %, throughput, avg delay, retransmissions.

## What you should observe (explain these)
| Experiment | Expected trend | Why |
|---|---|---|
| Bottleneck 2→10 Mbps | Loss falls toward 0, TCP throughput rises | More capacity, queue rarely full |
| UDP rate ↑ (esp. 2, 5 Mbps) | UDP takes most bandwidth, TCP throughput collapses, more loss | UDP doesn't back off; TCP does |
| Queue 20→100 | Larger queue = fewer drops but **higher delay** (bufferbloat) | Packets wait longer in the buffer |
| Delay ↑ | End-to-end delay rises; TCP throughput can drop | TCP rate ≈ window/RTT; slower ACK feedback and slow-start |

## Notes on TCP retransmission rate
FlowMonitor has no retransmit counter. The program prints a **proxy** (lost packets ≈ retransmitted segments). For the **exact** count: open `dumbbell-*.pcap` in Wireshark and filter **`tcp.analysis.retransmission`**. Retransmission rate = those packets ÷ total TCP data packets. Mention both in your record.

## Reading the ASCII trace (`dumbbell.tr`)
First character: `+` enqueue, `-` dequeue (transmission start), `d` **drop**, `r` receive. Count drops: `grep -c "^d" dumbbell.tr`.

**Queue caveat:** if you also install a traffic-control queue disc on devices, its size (default 1000 packets) can hide your 50-packet device queue. We don't install one here, so the 50p queue is active.

### Viva questions
- *Why does the UDP flow hurt TCP?* UDP isn't congestion-controlled.
- *What is DropTail?* A FIFO queue that drops new arrivals when full.
- *Effect of a larger queue?* Less loss, more delay.

---

# 11. Assignment 9: TCP Reno vs Cubic vs BBR in a VANET scenario

## In one minute
5 vehicles → roadside router (RSU) → traffic server over a shared 10 Mbps bottleneck. Run the **same** experiment three times, changing only the congestion-control algorithm, and compare.

## The algorithms (what to write)
| Variant | Idea | Behavior under congestion |
|---|---|---|
| **Reno / NewReno** | Loss-based AIMD: +1 MSS per RTT, halve cwnd on loss | Classic sawtooth; fills the queue → delay rises |
| **Cubic** | Loss-based, cwnd grows as a **cubic function** of time since last loss; faster recovery on high-BDP links | Higher throughput than Reno on long fat paths; still fills buffers |
| **BBR** | **Model-based**: estimates bottleneck bandwidth and min RTT, paces at that rate | Keeps queue short, so **lower delay**; loss does not trigger backoff; can be unfair when sharing with Cubic |

Note: in NS-3 the type is **`TcpNewReno`** (the Reno family), **`TcpCubic`**, **`TcpBbr`**.

## Code
**`tcp_variants.cc`**
```cpp
// Assignment 9: 5 vehicles -> roadside router -> server. Compare Reno / Cubic / BBR
// run: ./ns3 run "scratch/tcp_variants --tcp=TcpCubic"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>
#include <vector>
using namespace ns3;

static void CwndTrace(Ptr<OutputStreamWrapper> s, uint32_t oldV, uint32_t newV)
{
  *s->GetStream() << Simulator::Now().GetSeconds() << "\t" << newV << std::endl;
}
static void HookCwnd(uint32_t nodeId, std::string file)
{
  AsciiTraceHelper h;
  Ptr<OutputStreamWrapper> s = h.CreateFileStream(file);
  Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) +
                                    "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                MakeBoundCallback(&CwndTrace, s));
}

int main(int argc, char *argv[])
{
  std::string tcp = "TcpNewReno";          // TcpNewReno | TcpCubic | TcpBbr
  double simTime = 30.0;
  CommandLine cmd; cmd.AddValue("tcp", "TCP variant", tcp); cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcp));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

  const uint32_t nV = 5;
  NodeContainer vehicles; vehicles.Create(nV);
  NodeContainer router; router.Create(1);
  NodeContainer server; server.Create(1);

  PointToPointHelper wireless, backbone;     // vehicle link = stand-in for the RSU wireless hop
  wireless.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
  wireless.SetChannelAttribute("Delay", StringValue("5ms"));
  backbone.SetDeviceAttribute("DataRate", StringValue("10Mbps"));   // shared bottleneck
  backbone.SetChannelAttribute("Delay", StringValue("20ms"));
  backbone.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("100p"));

  InternetStackHelper stack;
  stack.Install(vehicles); stack.Install(router); stack.Install(server);

  Ipv4AddressHelper a;
  for (uint32_t i = 0; i < nV; ++i)
  {
    NetDeviceContainer d = wireless.Install(vehicles.Get(i), router.Get(0));
    a.SetBase(("10.1." + std::to_string(i + 1) + ".0").c_str(), "255.255.255.0");
    a.Assign(d);
  }
  NetDeviceContainer ds = backbone.Install(router.Get(0), server.Get(0));
  a.SetBase("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer is = a.Assign(ds);                // is.GetAddress(1) = server
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  for (uint32_t i = 0; i < nV; ++i)
  {
    uint16_t port = 5000 + i;
    BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(is.GetAddress(1), port));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app = bulk.Install(vehicles.Get(i));
    app.Start(Seconds(1.0)); app.Stop(Seconds(simTime));
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sink.Install(server.Get(0)).Start(Seconds(0.5));
  }

  // sockets only exist after the apps start (t=1s) -> hook cwnd just after
  for (uint32_t i = 0; i < nV; ++i)
    Simulator::Schedule(Seconds(1.001), &HookCwnd, i, "cwnd-" + tcp + "-v" + std::to_string(i) + ".dat");

  FlowMonitorHelper fmh;
  Ptr<FlowMonitor> mon = fmh.InstallAll();
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  mon->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  std::vector<double> thr;
  double totTx = 0, totLost = 0, delaySum = 0, rxPk = 0;
  for (auto &kv : mon->GetFlowStats())
  {
    auto t = cls->FindFlow(kv.first);
    if (t.destinationPort < 5000 || t.destinationPort >= 5000 + nV) continue;   // data flows only
    const auto &s = kv.second;
    thr.push_back(s.rxBytes * 8.0 / (simTime - 1.0) / 1e6);
    totTx += s.txPackets; totLost += s.lostPackets;
    delaySum += s.delaySum.GetSeconds(); rxPk += s.rxPackets;
  }
  double sum = 0, sq = 0;
  for (double x : thr) { sum += x; sq += x * x; }
  double jain = sq > 0 ? (sum * sum) / (thr.size() * sq) : 0;      // 1.0 = perfectly fair

  std::cout << tcp << " | total thr=" << sum << " Mbps | avg delay=" << 1000 * delaySum / rxPk
            << " ms | loss=" << 100 * totLost / totTx << " % | Jain fairness=" << jain << "\n";
  Simulator::Destroy();
  return 0;
}
```

## Run and plot
```bash
for v in TcpNewReno TcpCubic TcpBbr; do ./ns3 run "scratch/tcp_variants --tcp=$v"; done
# each run writes cwnd-<variant>-v0.dat ... (time \t cwnd); plot with gnuplot:
gnuplot -e "set term png; set output 'cwnd.png'; set xlabel 'time (s)'; set ylabel 'cwnd (bytes)'; \
 plot 'cwnd-TcpNewReno-v0.dat' w l t 'Reno', 'cwnd-TcpCubic-v0.dat' w l t 'Cubic', 'cwnd-TcpBbr-v0.dat' w l t 'BBR'"
```

## Comparison table to fill
| Metric | Reno | Cubic | BBR |
|---|---|---|---|
| Total throughput (Mbps) | | | |
| Avg delay (ms) | | | |
| Packet loss (%) | | | |
| Jain fairness | | | |
| cwnd shape | sawtooth | cubic curve | stable/probing |

Typical outcome: all fill the link; BBR shows the **lowest delay**, Cubic/Reno higher delay and more loss. Report what *you* measured and explain deviations.

## Why low delay and reliability matter for intelligent transportation
- **Safety-critical messages** (collision warning, emergency braking, hazard alerts) are only useful if they arrive within tens of milliseconds. A vehicle at 100 km/h moves ~28 m per second, so a 200 ms delay means ~5.6 m traveled before the warning is received.
- **Real-time traffic control**: signals and routing decisions need fresh data. Stale data → wrong decisions.
- **Reliability**: lost camera/road-condition updates create blind spots; retransmissions add delay, so there's a delay-vs-reliability trade-off.
- **Mobility** makes links short-lived, so the protocol must react fast.
- This is why delay-friendly algorithms (BBR, small queues) are attractive, and why safety messages often use UDP-based/real-time paths while bulk uploads (video) use TCP.

### Viva questions
- *What does cwnd mean?* Congestion window: the max unacknowledged data in flight, set by the sender.
- *Slow start vs congestion avoidance?* Exponential growth until `ssthresh`, then linear.
- *Why is BBR called model-based?* Uses bandwidth × min-RTT estimates rather than loss.
- *What is Jain's index?* A fairness measure from 1/n to 1.

---

# 12. Assignment 10: Routing protocol comparison (RIP, OSPF, AODV)

## In one minute
| Protocol | Type | How it works | Reacts to failure |
|---|---|---|---|
| **RIP** | Distance-vector | Routers periodically tell neighbours their whole table; metric = hop count (max 15) | Slow (periodic updates; count-to-infinity risk) |
| **OSPF** | Link-state | Each router floods link states, runs **Dijkstra** to build the shortest-path tree | Fast, recomputes from topology change |
| **AODV** | Reactive (on-demand), for ad-hoc | Route discovered only when needed: RREQ flood → RREP; RERR on link break | Re-discovers route on demand (delay for the first packet) |

**Honest NS-3 note:** mainline NS-3 has `RipHelper` and `AodvHelper`, but **no OSPF module**. The code uses `Ipv4GlobalRouting` with `RespondToInterfaceEvents=true` as the OSPF stand-in (global link-state shortest path, recomputed on link events). State this clearly in your record, and check what your instructor expects (e.g., a third-party OSPF module).

## Topology used
```
S1 --\                 /-- R2 --\                 /-- D1
      R1 --(primary)--             R4 --
S2 --/  \                          /   \-- D2
          \-- R3 -- R5 -----------/ (longer backup path)
```
Primary path R1–R2–R4 (shorter); backup R1–R3–R5–R4. Link failure disables **R1–R2** at t = 30 s, and traffic must reroute via the backup.

## Code
**`routing_compare.cc`**
```cpp
// Assignment 10: RIP vs "OSPF-like" (global link-state) vs AODV, with congestion and link failure
// run: ./ns3 run "scratch/routing_compare --proto=rip --scenario=failure"
//   proto    : rip | ospf | aodv        scenario : normal | congestion | failure
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
using namespace ns3;

// ---------- count routing-table changes by polling every 0.1 s ----------
static uint32_t g_updates = 0;
static std::map<uint32_t, std::string> g_prev;

static std::string Signature(const std::string &table)      // keep only "destination>gateway" pairs
{
  std::istringstream in(table);
  std::string line, sig;
  while (std::getline(in, line))
  {
    if (line.empty() || !std::isdigit((unsigned char)line[0])) continue;   // route lines start with an IP
    std::istringstream ls(line);
    std::string dst, gw; ls >> dst >> gw;
    sig += dst + ">" + gw + ";";
  }
  return sig;
}
static void Poll(NodeContainer routers)
{
  for (uint32_t i = 0; i < routers.GetN(); ++i)
  {
    Ptr<Ipv4> ip = routers.Get(i)->GetObject<Ipv4>();
    std::ostringstream oss;
    Ptr<OutputStreamWrapper> w = Create<OutputStreamWrapper>(&oss);
    ip->GetRoutingProtocol()->PrintRoutingTable(w);
    std::string sig = Signature(oss.str());
    auto it = g_prev.find(i);
    if (it != g_prev.end() && it->second != sig) g_updates++;
    g_prev[i] = sig;
  }
  Simulator::Schedule(Seconds(0.1), &Poll, routers);
}

int main(int argc, char *argv[])
{
  std::string proto = "rip", scenario = "normal";
  CommandLine cmd;
  cmd.AddValue("proto", "rip|ospf|aodv", proto);
  cmd.AddValue("scenario", "normal|congestion|failure", scenario);
  cmd.Parse(argc, argv);

  const double tTraffic = 20, tEnd = 50, tFail = 30, tSim = 55;

  // node indexes
  enum { S1, S2, R1, R2, R3, R4, R5, D1, D2 };
  NodeContainer all; all.Create(9);
  NodeContainer routers(all.Get(R1), all.Get(R2), all.Get(R3), all.Get(R4), all.Get(R5));
  NodeContainer hosts(all.Get(S1), all.Get(S2), all.Get(D1), all.Get(D2));

  //          0        1        2       3       4       5       6       7       8
  //        S1-R1    S2-R1    R1-R2   R2-R4   R1-R3   R3-R5   R5-R4   R4-D1   R4-D2
  std::vector<std::pair<int, int>> edges = {{S1, R1}, {S2, R1}, {R1, R2}, {R2, R4}, {R1, R3},
                                             {R3, R5}, {R5, R4}, {R4, D1}, {R4, D2}};
  std::vector<NetDeviceContainer> devs;
  for (size_t k = 0; k < edges.size(); ++k)
  {
    PointToPointHelper p;
    bool core = (k >= 2 && k <= 6);
    p.SetDeviceAttribute("DataRate", StringValue(core ? "5Mbps" : "10Mbps"));
    p.SetChannelAttribute("Delay", StringValue("2ms"));
    devs.push_back(p.Install(all.Get(edges[k].first), all.Get(edges[k].second)));
  }

  // ---------- install the routing protocol ----------
  Ipv4StaticRoutingHelper staticRh;
  if (proto == "rip")
  {
    Config::SetDefault("ns3::Rip::UnsolicitedRoutingUpdate", TimeValue(Seconds(5)));
    RipHelper rip;
    Ipv4ListRoutingHelper list;
    list.Add(staticRh, 0); list.Add(rip, 10);
    InternetStackHelper sr; sr.SetRoutingHelper(list); sr.Install(routers);
    InternetStackHelper sh; sh.SetRoutingHelper(staticRh); sh.Install(hosts);
  }
  else if (proto == "aodv")
  {
    AodvHelper aodv;
    InternetStackHelper s; s.SetRoutingHelper(aodv); s.Install(all);
  }
  else   // "ospf": global (link-state, Dijkstra) routing = closest thing to OSPF in mainline ns-3
  {
    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));
    InternetStackHelper s; s.Install(all);
  }

  std::vector<Ipv4InterfaceContainer> ifs;
  Ipv4AddressHelper a;
  for (size_t k = 0; k < edges.size(); ++k)
  {
    a.SetBase(("10.1." + std::to_string(k + 1) + ".0").c_str(), "255.255.255.0");
    ifs.push_back(a.Assign(devs[k]));
  }
  if (proto == "ospf") Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  if (proto == "rip")                       // hosts need a default route to their router
  {
    int hostEdge[4] = {0, 1, 7, 8};         // S1,S2,D1,D2 edges
    int hostNode[4] = {S1, S2, D1, D2};
    for (int h = 0; h < 4; ++h)
    {
      int k = hostEdge[h];
      Ipv4Address gw = (hostNode[h] == S1 || hostNode[h] == S2) ? ifs[k].GetAddress(1) : ifs[k].GetAddress(0);
      Ptr<Ipv4StaticRouting> sr = staticRh.GetStaticRouting(all.Get(hostNode[h])->GetObject<Ipv4>());
      sr->SetDefaultRoute(gw, 1);
    }
  }

  // ---------- traffic: CBR/UDP via OnOff ----------
  struct Flow { int src, dst; int dstEdge; const char *rate; };
  std::vector<Flow> flows;
  flows.push_back({S1, D1, 7, "2Mbps"});
  if (scenario == "congestion")
  {
    flows.push_back({S1, D1, 7, "3Mbps"});
    flows.push_back({S2, D2, 8, "3Mbps"});
    flows.push_back({S2, D2, 8, "3Mbps"});
  }
  for (size_t i = 0; i < flows.size(); ++i)
  {
    uint16_t port = 9000 + i;
    Ipv4Address dstAddr = ifs[flows[i].dstEdge].GetAddress(1);      // D1/D2 side
    OnOffHelper on("ns3::UdpSocketFactory", InetSocketAddress(dstAddr, port));
    on.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    on.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    on.SetAttribute("DataRate", StringValue(flows[i].rate));
    on.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer app = on.Install(all.Get(flows[i].src));
    app.Start(Seconds(tTraffic)); app.Stop(Seconds(tEnd));
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sink.Install(all.Get(flows[i].dst)).Start(Seconds(1));
  }

  // ---------- link failure: take R1-R2 down on BOTH ends ----------
  if (scenario == "failure")
  {
    Ptr<Ipv4> ip1 = all.Get(R1)->GetObject<Ipv4>(), ip2 = all.Get(R2)->GetObject<Ipv4>();
    uint32_t if1 = ip1->GetInterfaceForDevice(devs[2].Get(0));
    uint32_t if2 = ip2->GetInterfaceForDevice(devs[2].Get(1));
    Simulator::Schedule(Seconds(tFail), [=]() { ip1->SetDown(if1); ip2->SetDown(if2); });
  }

  // ---------- routing-table-update counter (only counts during the traffic period) ----------
  Simulator::Schedule(Seconds(0.5), &Poll, routers);
  Simulator::Schedule(Seconds(tTraffic), []() { g_updates = 0; });

  FlowMonitorHelper fmh;
  Ptr<FlowMonitor> mon = fmh.InstallAll();
  Simulator::Stop(Seconds(tSim));
  Simulator::Run();
  mon->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  double tx = 0, rx = 0, dsum = 0;
  for (auto &kv : mon->GetFlowStats())
  {
    auto t = cls->FindFlow(kv.first);
    if (t.destinationPort < 9000 || t.destinationPort >= 9100) continue;   // ignore RIP/AODV control flows
    tx += kv.second.txPackets; rx += kv.second.rxPackets; dsum += kv.second.delaySum.GetSeconds();
  }
  std::cout << proto << "," << scenario << "," << (tx ? rx / tx : 0) << ","
            << (rx ? 1000 * dsum / rx : 0) << "," << g_updates << std::endl;   // proto,scenario,PDR,delay_ms,updates
  Simulator::Destroy();
  return 0;
}
```

## Run all 9 combinations
```bash
echo "proto,scenario,pdr,delay_ms,updates" > results.csv
for p in rip ospf aodv; do for s in normal congestion failure; do
  ./ns3 run "scratch/routing_compare --proto=$p --scenario=$s" 2>/dev/null | tail -1 >> results.csv
done; done
cat results.csv
```

## Plot the three required graphs
```python
import pandas as pd, matplotlib.pyplot as plt
df = pd.read_csv("results.csv")
for col, title in [("pdr","PDR"),("delay_ms","End-to-end delay (ms)"),("updates","Routing table updates")]:
    pv = df.pivot(index="scenario", columns="proto", values=col).loc[["normal","congestion","failure"]]
    pv.plot(kind="bar"); plt.ylabel(title); plt.title(f"{title} vs Network Condition")
    plt.xticks(rotation=0); plt.tight_layout(); plt.savefig(f"{col}.png")
```

## How "routing table updates" is measured here
Every 0.1 s the program snapshots each router's table (destination→gateway pairs) and counts **how many times a table changed during the traffic period**. It's an easy, protocol-neutral metric. State this definition in your record. Alternative: count control packets (RIP uses UDP port 520).

## Expected behavior to explain
- **Normal:** PDR ≈ 1 for all; AODV has a slightly higher first-packet delay (route discovery).
- **Congestion:** PDR drops for all (queues overflow on the 5 Mbps core). Routing protocol choice matters little since the bottleneck is capacity, not routes.
- **Failure:** during rerouting packets are lost. OSPF-like recovers fastest; RIP recovers when it processes the interface-down and triggered update; AODV needs an RERR + new discovery. Backup path is longer so delay increases after recovery.

Possible issues: AODV over point-to-point links works but is designed for wireless. If it misbehaves, ask whether Wi-Fi ad-hoc is expected. `OutputStreamWrapper(&oss)` needs a recent ns-3; if it fails, print tables to files with `Ipv4RoutingHelper::PrintRoutingTableAllAt` and compare.

### Viva questions
- *Distance vector vs link state?* DV shares tables with neighbours only; LS shares topology with everyone and runs Dijkstra.
- *Count-to-infinity?* DV loops incrementing metric after link failure. RIP limits to 15.
- *Why AODV for ad hoc?* Mobile nodes change links often, so on-demand routes avoid constant table maintenance.

---

# 13. Assignment 11: Heterogeneous wireless network with mobility

## Reality check about NS-3 support
| Technology | In NS-3? | What to use |
|---|---|---|
| Wi-Fi (802.11) | Yes | `WifiHelper`, Yans channel |
| Zigbee (802.15.4) | Yes: PHY/MAC as **lr-wpan**; Zigbee network layer only in newer versions | `LrWpanHelper` + `SixLowPanHelper` |
| Bluetooth | **No** in mainline | Approximate (below), or an external module, and **state the approximation in your report** |

**Bluetooth approximation** used in the Wi-Fi program (`--tech=bt`): 1 Mbps fixed rate, 4 dBm TX power, 10 m area, 100 kbps traffic. A short-range, low-rate wireless link.
**Ask your instructor** whether an external Bluetooth module is expected before you rely on this.

## Mobility concepts
- **RandomWalk2d:** each node picks a random direction and moves at the set speed for a time or distance, bouncing at the boundary.
- **RandomWaypoint:** pick a random destination, move there at the set speed, pause, repeat. (Needs a `PositionAllocator` attribute.)
- Speeds to run: **1, 5, 10 m/s**.

## Metrics (how each is obtained)
| Metric | How |
|---|---|
| Received power (dBm) | Sample `LogDistancePropagationLossModel::CalcRxPower(txDbm, txMobility, rxMobility)` every 0.5 s from node positions, and average (works for any technology) |
| Throughput | Σ rxBytes × 8 / time (FlowMonitor) |
| Packet loss % | (tx − rx)/tx × 100 |
| Delay | Σ delay / rx packets |

## Wi-Fi (and Bluetooth-like) code
**`wifi_mobility.cc`**
```cpp
// Assignment 11 (part 1): Wi-Fi (and a "Bluetooth-like" approximation) with mobile nodes
// run: ./ns3 run "scratch/wifi_mobility --speed=5 --tech=wifi"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/wifi-module.h"
#include <iostream>
using namespace ns3;

static double g_rxSum = 0; static uint32_t g_rxN = 0;
static Ptr<LogDistancePropagationLossModel> g_loss;

// every 0.5 s: received power (dBm) at the gateway from each mobile node
static void SampleRx(NodeContainer sta, Ptr<Node> gw, double txDbm)
{
  for (uint32_t i = 0; i < sta.GetN(); ++i)
  {
    g_rxSum += g_loss->CalcRxPower(txDbm, sta.Get(i)->GetObject<MobilityModel>(), gw->GetObject<MobilityModel>());
    g_rxN++;
  }
  Simulator::Schedule(Seconds(0.5), &SampleRx, sta, gw, txDbm);
}

int main(int argc, char *argv[])
{
  double speed = 5.0, simTime = 30.0;
  std::string tech = "wifi";            // wifi | bt  (bt = 1 Mbps, low power, short range approximation)
  CommandLine cmd;
  cmd.AddValue("speed", "node speed m/s", speed);
  cmd.AddValue("tech", "wifi|bt", tech);
  cmd.Parse(argc, argv);

  double txDbm = 16.0206, area = 50;    // ns-3 default Wi-Fi tx power
  NodeContainer sta, ap; sta.Create(5); ap.Create(1);

  YansWifiChannelHelper ch = YansWifiChannelHelper::Default();   // LogDistance loss + const-speed delay
  YansWifiPhyHelper phy; phy.SetChannel(ch.Create());
  WifiHelper wifi;
  if (tech == "bt")
  {
    wifi.SetStandard(WIFI_STANDARD_80211b);        // older ns-3: WIFI_PHY_STANDARD_80211b
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));
    txDbm = 4.0; area = 10;
    phy.Set("TxPowerStart", DoubleValue(txDbm)); phy.Set("TxPowerEnd", DoubleValue(txDbm));
  }
  else
  {
    wifi.SetStandard(WIFI_STANDARD_80211g);        // older ns-3: WIFI_PHY_STANDARD_80211g
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
  }
  WifiMacHelper mac; Ssid ssid("campus");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDev = wifi.Install(phy, mac, sta);
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDev = wifi.Install(phy, mac, ap);

  // ---- mobility: STAs random-walk at the chosen speed, AP fixed at the centre
  std::string A = std::to_string(area);
  MobilityHelper mob;
  mob.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                           "X", StringValue("ns3::UniformRandomVariable[Min=-" + A + "|Max=" + A + "]"),
                           "Y", StringValue("ns3::UniformRandomVariable[Min=-" + A + "|Max=" + A + "]"));
  mob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                       "Bounds", RectangleValue(Rectangle(-area, area, -area, area)),
                       "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]"),
                       "Mode", StringValue("Time"), "Time", StringValue("2s"));
  mob.Install(sta);
  MobilityHelper fixed; fixed.SetMobilityModel("ns3::ConstantPositionMobilityModel"); fixed.Install(ap);
  ap.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));

  InternetStackHelper stack; stack.Install(sta); stack.Install(ap);
  Ipv4AddressHelper a; a.SetBase("10.1.1.0", "255.255.255.0");
  a.Assign(staDev); Ipv4InterfaceContainer apIf = a.Assign(apDev);

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9000));
  sink.Install(ap.Get(0)).Start(Seconds(0.5));
  OnOffHelper on("ns3::UdpSocketFactory", InetSocketAddress(apIf.GetAddress(0), 9000));
  on.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  on.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  on.SetAttribute("DataRate", StringValue(tech == "bt" ? "100kbps" : "500kbps"));
  on.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer apps = on.Install(sta);
  apps.Start(Seconds(2)); apps.Stop(Seconds(simTime));

  g_loss = CreateObject<LogDistancePropagationLossModel>();       // same defaults as the Yans channel
  Simulator::Schedule(Seconds(2), &SampleRx, sta, ap.Get(0), txDbm);

  FlowMonitorHelper fmh; Ptr<FlowMonitor> mon = fmh.InstallAll();
  Simulator::Stop(Seconds(simTime)); Simulator::Run();
  mon->CheckForLostPackets();

  double tx = 0, rx = 0, rxBytes = 0, dsum = 0;
  for (auto &kv : mon->GetFlowStats())
  { tx += kv.second.txPackets; rx += kv.second.rxPackets; rxBytes += kv.second.rxBytes; dsum += kv.second.delaySum.GetSeconds(); }
  std::cout << tech << ",speed=" << speed << ",rxPower_dBm=" << g_rxSum / g_rxN
            << ",thr_kbps=" << rxBytes * 8 / (simTime - 2) / 1e3
            << ",loss%=" << (tx ? 100 * (tx - rx) / tx : 0)
            << ",delay_ms=" << (rx ? 1000 * dsum / rx : 0) << std::endl;
  Simulator::Destroy();
  return 0;
}
```

## Zigbee (802.15.4 / 6LoWPAN) code
**`lrwpan_mobility.cc`**
```cpp
// Assignment 11 (part 2): Zigbee-style network = IEEE 802.15.4 (lr-wpan) + 6LoWPAN, mobile end nodes
// Reference to copy from if something does not compile on your version:
//   src/lr-wpan/examples/lr-wpan-data.cc  and  src/sixlowpan/examples/example-ping-lr-wpan.cc
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include <iostream>
using namespace ns3;

static double g_rxSum = 0; static uint32_t g_rxN = 0;
static Ptr<LogDistancePropagationLossModel> g_loss;
static void SampleRx(NodeContainer nodes, double txDbm)    // node 0 = coordinator
{
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    g_rxSum += g_loss->CalcRxPower(txDbm, nodes.Get(i)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>());
    g_rxN++;
  }
  Simulator::Schedule(Seconds(0.5), &SampleRx, nodes, txDbm);
}

int main(int argc, char *argv[])
{
  double speed = 1.0, simTime = 30.0, area = 15.0;
  CommandLine cmd; cmd.AddValue("speed", "m/s", speed); cmd.Parse(argc, argv);

  NodeContainer nodes; nodes.Create(6);                 // 0 = coordinator, 1..5 = mobile end devices

  MobilityHelper mob;
  mob.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                           "X", StringValue("ns3::UniformRandomVariable[Min=-15|Max=15]"),
                           "Y", StringValue("ns3::UniformRandomVariable[Min=-15|Max=15]"));
  mob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                       "Bounds", RectangleValue(Rectangle(-area, area, -area, area)),
                       "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]"),
                       "Mode", StringValue("Time"), "Time", StringValue("2s"));
  NodeContainer mobile;
  for (uint32_t i = 1; i < nodes.GetN(); ++i) mobile.Add(nodes.Get(i));
  mob.Install(mobile);                                  // only end devices move
  MobilityHelper fixed; fixed.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  fixed.Install(nodes.Get(0));                          // coordinator stays at the origin

  LrWpanHelper lr;                                      // default: LogDistance loss, 250 kbps O-QPSK
  NetDeviceContainer lrDev = lr.Install(nodes);
  lr.AssociateToPan(lrDev, 0);

  InternetStackHelper v6;
  v6.SetIpv4StackInstall(false);
  v6.Install(nodes);
  SixLowPanHelper six;
  NetDeviceContainer sixDev = six.Install(lrDev);
  Ipv6AddressHelper ip6; ip6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifc = ip6.Assign(sixDev);
  Ipv6Address coord = ifc.GetAddress(0, 1);             // index 1 = global address

  UdpServerHelper srv(9000);
  srv.Install(nodes.Get(0)).Start(Seconds(1));
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    UdpClientHelper cli(coord, 9000);
    cli.SetAttribute("MaxPackets", UintegerValue(100000));
    cli.SetAttribute("Interval", TimeValue(MilliSeconds(200)));   // ~ 5 pkt/s per node
    cli.SetAttribute("PacketSize", UintegerValue(60));            // small: 802.15.4 frame is max 127 B
    cli.Install(nodes.Get(i)).Start(Seconds(3 + 0.1 * i));
  }

  g_loss = CreateObject<LogDistancePropagationLossModel>();
  Simulator::Schedule(Seconds(3), &SampleRx, nodes, 0.0);         // lr-wpan default tx power = 0 dBm

  FlowMonitorHelper fmh; Ptr<FlowMonitor> mon = fmh.InstallAll();
  Simulator::Stop(Seconds(simTime)); Simulator::Run();
  mon->CheckForLostPackets();
  double tx = 0, rx = 0, rxBytes = 0, dsum = 0;
  for (auto &kv : mon->GetFlowStats())
  { tx += kv.second.txPackets; rx += kv.second.rxPackets; rxBytes += kv.second.rxBytes; dsum += kv.second.delaySum.GetSeconds(); }
  std::cout << "zigbee,speed=" << speed << ",rxPower_dBm=" << g_rxSum / g_rxN
            << ",thr_kbps=" << rxBytes * 8 / (simTime - 3) / 1e3
            << ",loss%=" << (tx ? 100 * (tx - rx) / tx : 0)
            << ",delay_ms=" << (rx ? 1000 * dsum / rx : 0) << std::endl;
  Simulator::Destroy();
  return 0;
}
```
This one is the riskiest to compile across versions. If it fails, copy the setup from `src/sixlowpan/examples/example-ping-lr-wpan.cc` and keep the mobility, application and measurement sections.

## Run
```bash
for s in 1 5 10; do ./ns3 run "scratch/wifi_mobility --speed=$s --tech=wifi"; done
for s in 1 5 10; do ./ns3 run "scratch/wifi_mobility --speed=$s --tech=bt"; done
for s in 1 5 10; do ./ns3 run "scratch/lrwpan_mobility --speed=$s"; done
```

## What you should discuss
- **Signal strength** fluctuates as nodes move closer/farther. Averaged over time it changes little with speed (positions are random); the **variation** matters.
- **Throughput and loss:** higher speed → faster link changes → more frequent weak-signal periods → rate-adaptation lags (Wi-Fi) → more loss. Effects are modest in a small area and can look noisy; **run several seeds** (`--RngRun=1,2,3`) and average.
- **Delay:** rises when retries and rate fallback occur.
- **Technology comparison:** Wi-Fi (high rate, long range, high power) vs Bluetooth (short range, low rate) vs Zigbee (very low rate 250 kbps, low power, small frames). Zigbee is hit hardest in delay/throughput because of its low data rate and tiny frames; Wi-Fi tolerates mobility better thanks to rate adaptation and range.

### Viva questions
- *Why fix the gateway and move the clients?* Gateways (APs/coordinators) are static infrastructure; users move.
- *Doppler/fading?* Not modeled by simple log-distance loss. Mention as a limitation.
- *Why can received power not predict loss exactly?* Interference, collisions and rate adaptation also matter.

---

# 14. Last-night checklist

## Rehearsal plan (about 3 hours)
1. **Assignment 2 TCP/UDP** (30 min). Type both pairs from memory. Core skeleton of every other C program.
2. **Assignment 3** (20 min). Run against `python3 -m http.server`.
3. **Assignment 5** (20 min). It reuses the UDP skeleton.
4. **Assignment 6** (20 min). Practice the CRC by hand once, since you may be asked to show working.
5. **NS-3 skeleton** (30 min). Retype the minimal skeleton and the FlowMonitor recipe.
6. **Compile all NS-3 files once** (30 min), so version errors don't surprise you.
7. Read the viva questions aloud (30 min).

## The 10 lines you must know cold (C)
```c
int fd = socket(AF_INET, SOCK_STREAM, 0);                  // SOCK_DGRAM for UDP
struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(PORT);
a.sin_addr.s_addr = htonl(INADDR_ANY);                     // server  (client: inet_pton)
bind(fd, (struct sockaddr*)&a, sizeof a); listen(fd, 5);   // TCP server
int c = accept(fd, NULL, NULL);                            // TCP server
connect(fd, (struct sockaddr*)&a, sizeof a);               // TCP client
send(c, buf, n, 0); n = recv(c, buf, sizeof buf, 0);
sendto(fd, buf, n, 0, (struct sockaddr*)&peer, sizeof peer);
recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr*)&peer, &plen);
close(fd);
```

## Common errors
| Symptom | Likely cause |
|---|---|
| `implicit declaration of function` | Missing `#include` (`<arpa/inet.h>`, `<unistd.h>`, `<string.h>`) |
| `undefined reference to pthread_create` | Forgot `-pthread` |
| Client prints nothing | Newline not stripped or server not replying: check server terminal |
| NS-3: zero throughput | Sink not started, wrong port/IP, missing `PopulateRoutingTables()` |
| NS-3: all packets lost in RIP/AODV | Traffic started before convergence, so start apps later (we use 20 s) |
| NS-3: `Segmentation fault` after run | Reading a `Ptr` after `Simulator::Destroy()`, so read results before it |
| NS-3: pcap file missing | Enabled after `Run()`, or wrong prefix, so enable before `Run()` |

## If the examiner asks "explain your code"
Say it in this order: **topology → what each node does → how traffic is generated → what is measured → what the result shows and why.**

Good luck!
