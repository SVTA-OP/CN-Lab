# CN-Lab Reference Guide — where to find the code, offline

Covers **Exp2, 3, 5, 6** (C sockets) and **Exp7, 8, 9, 10, 11** (ns-3). Exp1 and Exp4 are skipped as requested.

Every experiment has the same four parts:

1. **What your code does** (file names from the repo, key calls).
2. **Where the reference code lives** on a stock Ubuntu install (man pages, headers, tools) or in the ns-3 tree.
3. **Grep / search commands** to find it quickly.
4. **Test commands.**

> **Note on your repo:** `Exp7/` only has `dualPortUDP.cc` (plus `untitled.txt`). The ns-3 tree itself is not committed. Experiments 7 to 11 only contain the single `.cc` you copy into `scratch/`.

---

## 0. One-time setup

### 0.1 C side (Ubuntu)

Section 2 (system calls) and section 3 (libc) man pages are **not** always installed by default. Install them once:

```bash
sudo apt install manpages-dev manpages-posix-dev man-db build-essential
# optional but useful:
sudo apt install glibc-doc netcat-openbsd dnsutils curl tcpdump net-tools iproute2
```

| What | Command |
|---|---|
| Open a function page (section 2/3) | `man 2 socket`, `man 3 getaddrinfo` |
| Overview pages (section 7) | `man 7 socket`, `man 7 ip`, `man 7 tcp`, `man 7 udp` |
| Search names + one-liners | `apropos socket` (same as `man -k socket`) |
| Search inside every page's text | `man -K "SO_RCVTIMEO"` (slow, but finds the page that mentions it) |
| Print only the EXAMPLES section | `man 3 getaddrinfo \| sed -n '/^EXAMPLES/,$p'` |
| Which pages exist for a name | `man -f bind` or `whatis bind` |
| POSIX versions of the pages | `man 3p socket`, `man 3p bind` (needs `manpages-posix-dev`) |
| libc manual with socket tutorial code | `info libc` then the "Sockets" chapter (needs `glibc-doc`) |
| Command `--help` | `curl --help all`, `nc -h`, `dig -h`, `ss --help` |

**Headers are documentation too.** Open them with `less`:

```bash
less /usr/include/sys/socket.h          # socket, bind, listen, accept, connect, send, recv, sockaddr
less /usr/include/netinet/in.h          # struct sockaddr_in, htons, INADDR_*, IPPROTO_*
less /usr/include/arpa/inet.h           # inet_pton, inet_ntop, inet_addr, inet_ntoa
less /usr/include/netdb.h               # gethostbyname, getaddrinfo, struct hostent, struct addrinfo
less /usr/include/sys/time.h            # gettimeofday, struct timeval
less /usr/include/x86_64-linux-gnu/bits/socket.h        # AF_INET, SOCK_STREAM, SOL_SOCKET, SO_* (arch path may differ)
less /usr/include/x86_64-linux-gnu/bits/socket_type.h   # SOCK_STREAM = 1, SOCK_DGRAM = 2
```

Find where a symbol is defined:

```bash
grep -rn "SO_RCVTIMEO" /usr/include | head
grep -rn "struct sockaddr_in {" -A8 /usr/include | head -20
```

### 0.2 ns-3 side (tarball)

The tarball you linked is `ns-3.49.tar.bz2`. The all-in-one bundle (`ns-allinone-3.49.tar.bz2`) contains the same `ns-3.49/` folder plus extras (netanim, bake, pybindgen). **Everything below is relative to the `ns-3.49/` folder.**

```bash
tar xjf ns-3.49.tar.bz2 && cd ns-3.49
./ns3 configure --enable-examples --enable-tests     # examples are OFF unless you enable them
./ns3 build                                          # first build takes a while
./ns3 run first                                      # sanity check (examples/tutorial/first.cc)
```

Where things live in the tree:

| Folder | What is in it |
|---|---|
| `examples/tutorial/` | `first.cc`, `second.cc`, `third.cc`, `fourth.cc`, `fifth.cc`, `sixth.cc`, `seventh.cc`, etc. The tutorial walks through these. |
| `examples/udp/`, `examples/udp-client-server/` | `udp-echo.cc`, `udp-client-server.cc`, `udp-trace-client-server.cc` |
| `examples/tcp/` | `tcp-bbr-example.cc`, `tcp-pacing.cc`, `tcp-star-server.cc`, `tcp-validation.cc` (also `tcp-variants-comparison.cc`, `tcp-bulk-send.cc` etc.; check with `ls`, as files move between releases) |
| `examples/routing/` | `rip-simple-network.cc`, `ripng-simple-network.cc`, `simple-global-routing.cc`, `simple-alternate-routing.cc`, `simple-routing-ping6.cc`, `dynamic-global-routing.cc`, `manet-routing-compare.cc` |
| `examples/wireless/` | `wifi-simple-adhoc.cc`, `wifi-simple-infra.cc`, `wifi-simple-interference.cc`, `wifi-tcp.cc`, `wifi-mixed-network.cc`, `wifi-ht-network.cc`, and more |
| `examples/mobility/` | random-walk and trace-based mobility examples |
| `src/<module>/examples/` | Each module's own examples, for example `src/aodv/examples/`, `src/lr-wpan/examples/`, `src/sixlowpan/examples/`, `src/flow-monitor/examples/`, `src/propagation/examples/`, `src/wave/examples/` |
| `src/<module>/helper/` | The helper classes you call (`*-helper.h`) |
| `src/<module>/model/` | The actual model classes |
| `doc/tutorial/source`, `doc/models/source`, `doc/manual/source` | Source of the online docs, readable offline as `.rst` |
| `scratch/` | Where you copy your own `.cc` file to run it |

**Release note:** the AODV example was `src/aodv/examples/aodv.cc` in old releases and is `aodv-example.cc` in recent ones (3.44 onward). Use the grep commands below rather than trusting a path from memory.

**Universal ns-3 search commands** (run from `ns-3.49/`):

```bash
grep -rl "UdpServerHelper"   examples src --include=*.cc      # which examples use a class
grep -rl "FlowMonitorHelper" examples src --include=*.cc
grep -rl "TcpBbr"            examples src --include=*.cc
grep -rl "RipHelper"         examples src --include=*.cc
grep -rl "LrWpanHelper"      examples src --include=*.cc
grep -rl "PointToPointDumbbellHelper" examples src --include=*.cc
grep -rn "AddValue"          examples/tcp/tcp-bbr-example.cc  # command-line options of an example
ls examples/*/ src/*/examples/                                # full inventory
```

**Built-in help for any program:**

```bash
./ns3 run "scratch/dualPortUDP --PrintHelp"                 # your CommandLine::AddValue options
./ns3 run "scratch/dualPortUDP --PrintAttributes=ns3::PointToPointNetDevice"
./ns3 run "scratch/dualPortUDP --PrintTypeIds"              # every class name
./ns3 run "scratch/dualPortUDP --PrintGroups"
NS_LOG="UdpServer=level_all|prefix_time" ./ns3 run scratch/dualPortUDP    # turn on logging
```

**Online references for 3.49** (same content as `doc/`):

* Tutorial: <https://www.nsnam.org/docs/release/3.49/tutorial/html/index.html>
* Model Library: <https://www.nsnam.org/docs/release/3.49/models/html/index.html>
* Manual: <https://www.nsnam.org/docs/release/3.49/manual/html/index.html>
* Doxygen API: <https://www.nsnam.org/docs/release/3.49/doxygen/index.html>

---

# PART A — C socket programming

## Common skeletons (used by Exp2, 3, 5, 6)

Every one of your C programs is built from these two templates.

**TCP server:** `socket()` → `bind()` → `listen()` → `accept()` → `recv()/read()` → `send()/write()` → `close()`
**TCP client:** `socket()` → `connect()` → `send()` → `recv()` → `close()`
**UDP server:** `socket()` → `bind()` → `recvfrom()` → `sendto()`
**UDP client:** `socket()` → `sendto()` → `recvfrom()`

Best offline sample code on a default Ubuntu box:

| Need | Where |
|---|---|
| **Complete working TCP+UDP client and server in C** | `man 3 getaddrinfo` → EXAMPLES section. It has a full UDP server and UDP client. Easy to adapt for TCP by changing `SOCK_DGRAM` to `SOCK_STREAM` and adding `listen`/`accept`. |
| Sockaddr structs and byte order | `man 7 ip` (IPv4 `sockaddr_in`), `man 3 byteorder` (`htons`/`ntohs`/`htonl`) |
| Text ↔ binary IP | `man 3 inet_pton`, `man 3 inet_ntop`, `man 3 inet` (for `inet_addr`, `inet_ntoa`) |
| Socket options | `man 7 socket` (lists every `SO_*` option) and `man 2 setsockopt` |
| TCP behaviour | `man 7 tcp` |
| UDP behaviour | `man 7 udp` |
| Return codes and `errno` | the ERRORS section at the bottom of each man page, plus `man 3 perror`, `man 3 errno` |

---

## Exp2 — Student record lookup over TCP and UDP

**Files:** `Exp2/tcp_student_server.c`, `tcp_student_client.c`, `udp_student_server.c`, `udp_student_client.c`, `students.csv`
**What they do:**

* The TCP server listens on **port 9000** and the UDP server on **port 9001**. Both load `students.csv` with `fopen`/`fgets`, split each line with `strtok`, and look up a roll number sent by the client.
* The client sends the roll number as text and prints the reply.
* *(Observation: the committed TCP server has `listen(server_fd, 1)` and a plain `accept` loop. I found no `pthread` calls in this version, so it serves one client at a time.)*

| Piece of code | Where to look it up |
|---|---|
| `socket(AF_INET, SOCK_STREAM, 0)` / `SOCK_DGRAM` | `man 2 socket` |
| `bind()` | `man 2 bind` |
| `listen(fd, 1)`; the second argument is the backlog | `man 2 listen`, then `man 7 tcp` (`tcp_max_syn_backlog`) |
| `accept(fd, NULL, NULL)` | `man 2 accept` |
| `connect()` | `man 2 connect` |
| `read()` / `write()` on a socket | `man 2 read`, `man 2 write` |
| `send()` / `recv()` | `man 2 send`, `man 2 recv` |
| `sendto()` / `recvfrom()` (UDP) | `man 2 send` (sendto is on the same page), `man 2 recv` (recvfrom is on the same page) |
| `htons(9000)` | `man 3 htons` (same page as `byteorder`) |
| `inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr)` | `man 3 inet_pton` |
| `strtok`, `strcspn`, `strcmp`, `strcpy` | `man 3 strtok` (has an EXAMPLES section), `man 3 strcspn`, `man 3 strcmp` |
| `fopen`, `fgets`, `fclose` | `man 3 fopen`, `man 3 fgets` |
| **Threads** (if your lab asks for concurrent TCP clients) | `man 3 pthread_create`, `man 3 pthread_join`, `man 7 pthreads`. Compile with `gcc ... -pthread`. |
| Alternative to threads: `fork()` per client | `man 2 fork`, `man 2 waitpid` |

**Test commands**

```bash
gcc tcp_student_server.c -o tcp_student_server && ./tcp_student_server &
gcc tcp_student_client.c -o tcp_student_client && ./tcp_student_client
# poke the servers without writing a client:
echo "3122247001057" | nc 127.0.0.1 9000          # TCP:  nc -h
echo "3122247001057" | nc -u -w1 127.0.0.1 9001    # UDP: -u for UDP, -w1 to quit after 1 s
ss -tlnp | grep 9000                              # confirm the TCP port is listening (man 8 ss)
ss -ulnp | grep 9001                              # UDP listener
```

---

## Exp3 — HTTP client (GET, save to file, timing)

**Files:** `Exp3/q1.c`, `Exp3/q2.c` (the two files are byte-for-byte identical in the repo), `Exp3/CN_Web_Server/index.html`, and sample outputs `downloaded_index.html`, `downloaded_SVTA-OP`
**What it does:** reads a URL from stdin, parses `http://host[:port]/path`, resolves the host with `gethostbyname`, opens a TCP socket, and records timestamps with `gettimeofday` (connect time, time to first byte, total). It sends `GET path HTTP/1.1` with `Connection: close`, strips the headers by finding `\r\n\r\n` with `strstr`, and writes the body to `downloaded_<name>` with `fwrite`.

| Piece of code | Where to look it up |
|---|---|
| `gethostbyname()` and `struct hostent` | `man 3 gethostbyname`. It is obsolete; **`man 3 getaddrinfo` is the modern replacement and its EXAMPLES section has the full client code.** |
| `gettimeofday()` and `struct timeval` | `man 2 gettimeofday`. Better clock: `man 2 clock_gettime` |
| `snprintf` for building the request | `man 3 snprintf` |
| Parsing URL: `strchr`, `strrchr`, `strncpy`, `strstr`, `atoi` | `man 3 strchr`, `man 3 strstr`, `man 3 strncpy`, `man 3 atoi` |
| `fopen(fname, "wb")`, `fwrite` | `man 3 fopen`, `man 3 fwrite` |
| **HTTP request format** (what to put in the GET) | Not in man pages. RFC 9112 (HTTP/1.1): <https://www.rfc-editor.org/rfc/rfc9112>. Quick look at a real one: `curl -v http://example.com` (shows request and response headers) |
| `Host:` header requirement, `Connection: close` | RFC 9112 sections on "Host" and "Connection"; or `curl --help all \| grep -i header` |

**Serving your own test page** (no web server to install):

```bash
cd Exp3/CN_Web_Server && python3 -m http.server 8000
# in another terminal:
./q1        # Enter URL: http://127.0.0.1:8000/index.html
```

Compare your numbers against curl's own timing:

```bash
curl -o /dev/null -s -w "connect=%{time_connect}s  ttfb=%{time_starttransfer}s  total=%{time_total}s  size=%{size_download}\n" http://127.0.0.1:8000/index.html
```

`wget --help | less` and `man curl` (search `/--write-out`) are the other in-box references.

---

## Exp5 — UDP DNS resolver simulation (cache, NXDOMAIN, timeout)

**Files:** `Exp5/DNSServer.c`, `Exp5/DNSClient.c`
**What they do:**

* The server binds UDP **port 8080** and holds a small name→IP table (`TABLE_SIZE 10`). It replies `IP:<addr>`, `ERROR:NXDOMAIN` or `ERROR:INVALID_FORMAT`.
* The client has a 16-entry cache (`CACHE_SIZE 16`), uses `strcasecmp` for lookups, and sets a **3 s receive timeout** with `setsockopt(SO_RCVTIMEO)`.
* A timed-out `recvfrom` returns -1 with `errno` set to `EAGAIN`/`EWOULDBLOCK`, and the client treats that as a DNS timeout.

| Piece of code | Where to look it up |
|---|---|
| `SOCK_DGRAM`, `recvfrom`, `sendto` | `man 2 socket`, `man 2 recv`, `man 2 send`, `man 7 udp` |
| **`setsockopt(... SO_RCVTIMEO, &tv ...)`** | `man 7 socket` (search `/SO_RCVTIMEO`), plus `man 2 setsockopt` |
| `struct timeval` | `man 2 gettimeofday` or `/usr/include/x86_64-linux-gnu/bits/types/struct_timeval.h` |
| Checking the timeout case: `errno == EAGAIN \|\| errno == EWOULDBLOCK` | ERRORS section of `man 2 recv`, then `man 3 errno` |
| `strcasecmp` (case-insensitive name compare) | `man 3 strcasecmp` |
| `inet_ntoa(client_addr.sin_addr)` | `man 3 inet_ntoa` (page named `inet`) |
| `isalnum`/`ctype` checks for "invalid format" | `man 3 isalnum` |
| **What a real DNS query looks like** | `dig example.com`, `dig +trace example.com`, `dig nonexistent.invalid` (shows `status: NXDOMAIN`), `dig -h`, `man dig` |
| Real resolver config | `man 5 resolv.conf`, `cat /etc/resolv.conf`, `resolvectl status`, `man 5 hosts` (`/etc/hosts` is your local "table") |
| The real protocol | RFC 1035: <https://www.rfc-editor.org/rfc/rfc1035> (your code uses plain text instead of this binary format) |
| Real library call that does the same job | `man 3 getaddrinfo` |

**Test commands**

```bash
gcc DNSServer.c -o server && ./server
gcc DNSClient.c -o client && ./client
echo -n "google.com" | nc -u -w3 127.0.0.1 8080     # watch the server log print the request
# test the timeout path: stop the server, run the client, wait 3 s
# test invalid format: send something like "bad name!!" through nc
```

---

## Exp6 — Error detection over TCP: Checksum and CRC

**Files:** `Exp6/CheckSumSender.c`, `CheckSumReciever.c`, `CRCSender.c`, `CRCReciever.c`
**What they do:**

* **Checksum sender:** sums the bytes, takes the one's complement (`~sum & 0xFF`), appends the checksum byte, and sends the frame over TCP to `127.0.0.1`.
* **Checksum receiver:** `socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen` → `accept` → `recv`. It flips a bit (`frame[0] ^= 0x01`) to simulate an error, re-sums, and expects `0xFF`/`0` when the frame is clean.
* **CRC sender:** converts the message to a bit string (`(msg[i] >> b) & 1`), appends zeros, does modulo-2 division with generator `10011` (x⁴ + x + 1) in `crc_divide`, and sends data + CRC bits.
* **CRC receiver:** divides the whole frame by the same generator. A zero remainder means no error.

| Piece of code | Where to look it up |
|---|---|
| `setsockopt(SO_REUSEADDR)`, which avoids "Address already in use" after a restart | `man 7 socket` (search `/SO_REUSEADDR`) |
| Socket calls | same pages as Exp2 |
| Bit operations (`>>`, `&`, `^`, `~`) | Not in man pages. Any C reference. Practice in `python3 -c "print(bin(0b1011 ^ 0b0110))"` |
| **Internet checksum algorithm (one's-complement sum)** | RFC 1071 <https://www.rfc-editor.org/rfc/rfc1071> (the algorithm TCP/UDP/IP use). Real header structs: `/usr/include/netinet/ip.h`, `/usr/include/netinet/udp.h`, `/usr/include/netinet/tcp.h` |
| **CRC** | There is no CRC function in libc. Command-line CRC tools: `cksum` (`man 1 cksum`, CRC-32 variant), `sum`, `md5sum`; Python: `python3 -c "import zlib; print(hex(zlib.crc32(b'hello')))"` |
| CRC theory and polynomials | Computer Networks textbook chapter on error detection, or the Linux kernel's `lib/crc32.c` if you have kernel source |
| Cross-check your sender's output | `printf 'hello' \| cksum` prints a CRC-32 and the byte count. It will not match your 4-bit CRC, but it is a useful sanity check for the idea |

**Test commands**

```bash
# Terminal 1:
gcc CheckSumReciever.c -o chcksumrecv && ./chcksumrecv
# Terminal 2:
gcc CheckSumSender.c -o chcksumsend && ./chcksumsend
# same pattern for CRCReciever.c / CRCSender.c
```

Always start the **receiver first**. It has to be in `accept()` before the sender calls `connect()`.

---

# PART B — ns-3 experiments (all run from `ns-3.49/`)

**How to run any of your files**

```bash
cp dualPortUDP.cc ns-3.49/scratch/
cd ns-3.49
./ns3 run "scratch/dualPortUDP --delay=5ms"
```

If `./ns3 run` says the target doesn't exist, run `./ns3 configure` once more. It re-scans `scratch/`.

Pattern shared by every ns-3 script: **Nodes → Devices+Channel (helpers) → Internet stack → IP addresses → Applications → Tracing/FlowMonitor → `Simulator::Run()`**. Tutorial chapters "Conceptual Overview" and "Building Topologies" explain this.

---

## Exp7 — Two UDP flows over a point-to-point link

**File:** `Exp7/dualPortUDP.cc`
**What it does:** 2 nodes, one `PointToPointHelper` link (`DataRate`, `Delay` set via the command line). Flow 1 goes to **port 5000** and an optional flow 2 to **port 6000**. It uses `UdpServerHelper` and `UdpClientHelper` (`MaxPackets`, `Interval`, `PacketSize` attributes), prints received counts from `UdpServer`, and can write pcap with `p2p.EnablePcapAll("udp-dual-port")`. Command-line options include `--delay --bandwidth --rate1 --rate2 --packetSize --simTime --secondFlow --pcap`.

| Piece of code | Where to find reference |
|---|---|
| Closest full example | **`examples/udp-client-server/udp-client-server.cc`**, which uses exactly `UdpServerHelper` and `UdpClientHelper` |
| Echo variant | `examples/udp/udp-echo.cc`, `examples/tutorial/first.cc` (two nodes + P2P + UdpEcho) |
| Two nodes, P2P, step-by-step explanation | Tutorial → "Building Topologies" and `examples/tutorial/first.cc` |
| Trace-file-driven UDP | `examples/udp-client-server/udp-trace-client-server.cc` |
| `PointToPointHelper` attributes | `./ns3 run "scratch/dualPortUDP --PrintAttributes=ns3::PointToPointNetDevice"`, `--PrintAttributes=ns3::PointToPointChannel`; source in `src/point-to-point/` |
| `UdpClient` / `UdpServer` classes | Model Library → Applications; source: `src/applications/model/udp-client.cc`, `udp-server.cc`; helper: `src/applications/helper/udp-client-server-helper.h` |
| `CommandLine cmd(__FILE__)` and `cmd.AddValue` | Manual → "Using the Command Line Argument System"; `examples/tutorial/third.cc`, `src/core/examples/command-line-example.cc` |
| pcap tracing | Tutorial → "Using the Tracing System" (`EnablePcapAll`), then read with `tcpdump -nn -r udp-dual-port-0-0.pcap` or Wireshark |
| Counting delivery | `UdpServer::GetReceived()` (Doxygen) |

```bash
grep -rl "UdpServerHelper" examples src --include=*.cc
./ns3 run "scratch/dualPortUDP --secondFlow=true --rate2=2Mbps --pcap=true"
tcpdump -nn -r udp-dual-port-1-0.pcap | head        # man tcpdump
```

---

## Exp8 — TCP and UDP sharing a dumbbell bottleneck

**File:** `Exp8/tcpudpDumbell.cc`
**What it does:** 4 end nodes + 2 routers. **TCP** flow n0→n3 uses `BulkSendHelper` with a `PacketSink`. **UDP** flow n1→n2 uses `OnOffHelper` with a `PacketSink`. The access link and bottleneck link are separate `PointToPointHelper` objects with `DropTailQueue` set via `SetQueue(...)`. Routing comes from `Ipv4GlobalRoutingHelper::PopulateRoutingTables()`. Metrics come from `FlowMonitor` and are classified with `Ipv4FlowClassifier`. Options: `--accessBw --accessDelay --bottleneckBw --bottleneckDelay --queueSize --simTime --tcpPacketSize --udpPacketSize --udpRate --tracing --pcap`.

| Piece of code | Where to find reference |
|---|---|
| TCP bulk transfer | `examples/tcp/tcp-bbr-example.cc`, `examples/tcp/tcp-star-server.cc`; also `grep -rl BulkSendHelper examples src --include=*.cc` |
| OnOff UDP source | `grep -rl OnOffHelper examples --include=*.cc`; tutorial `examples/tutorial/seventh.cc` and `examples/wireless/wifi-simple-adhoc.cc` style |
| Dumbbell topology helper (optional shortcut to hand-built nodes) | `grep -rl PointToPointDumbbellHelper examples src`; helper in `src/point-to-point-layout/model/point-to-point-dumbbell.h` |
| **FlowMonitor** (throughput, delay, loss per flow) | `src/flow-monitor/examples/` (for example `wifi-olsr-flowmon.cc`); Model Library → "Flow Monitor"; tutorial section on "Flow Monitor" |
| `DropTailQueue`, `QueueSize`, `SetQueue` | Model Library → "Queue" and "Traffic Control layer"; `src/network/utils/drop-tail-queue.h` |
| Traffic control layer (RED, CoDel, FIFO queue discs) if you extend the experiment | `src/traffic-control/examples/` (`red-tests.cc`, `queue-discs-benchmark.cc`, `traffic-control-example.cc`) |
| Global routing | `examples/routing/simple-global-routing.cc` |
| ASCII tracing | Tutorial → "Using the Tracing System": `EnableAsciiAll(ascii.CreateFileStream("x.tr"))` |

```bash
./ns3 run "scratch/tcpudpDumbell --bottleneckBw=2Mbps --udpRate=1Mbps"
for q in 10 50 100; do ./ns3 run "scratch/tcpudpDumbell --queueSize=$q"; done
```

---

## Exp9 — VANET TCP congestion control (NewReno vs Cubic vs BBR)

**File:** `Exp9/vanetTcpCongestionControl.cc`
**What it does:** 5 vehicles send `BulkSend` TCP to one server through one router. Vehicles 1 and 4 use NewReno, 2 and 5 use Cubic, vehicle 3 uses BBR. The variant is chosen per node with `Config::Set("/NodeList/N/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpBbr")))`. The congestion window is traced via `/NodeList/N/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow`. FlowMonitor prints throughput, delay, loss and fairness. The vehicle link is a P2P link. It is not a true wireless/WAVE model.

| Piece of code | Where to find reference |
|---|---|
| **Per-variant TCP setup and cwnd tracing** | **`examples/tcp/tcp-bbr-example.cc`** (cwnd trace, BBR) and `examples/tcp/tcp-variants-comparison.cc` if present in your release. Check with `ls examples/tcp` and `grep -rl "CongestionWindow" examples src --include=*.cc` |
| Choosing TCP variant globally | `Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"))` is shown in `tcp-bbr-example.cc` |
| TCP congestion control models | Model Library → "TCP models in ns-3"; sources: `src/internet/model/tcp-congestion-ops.cc`, `tcp-cubic.cc`, `tcp-bbr.cc`, `tcp-linux-reno.cc` |
| Trace source names (`CongestionWindow`, `SlowStartThreshold`, `RTT`) | `./ns3 run "scratch/x --PrintTypeIds"` or Doxygen page for `ns3::TcpSocketBase` → "TraceSources" |
| `Config::Connect` path syntax | Manual → "Config Paths" / "Using the Tracing System" |
| `Simulator::Schedule(Seconds(t), &func, args)` (used to hook the trace after the socket exists) | Manual → "Events and Simulator"; the tutorial's `fifth.cc` is a fully worked congestion-window trace example |
| **Real VANET** (802.11p, mobile vehicles) if you want to compare | `src/wave/examples/` (`wave-simple-80211p.cc`, `vanet-routing-compare.cc`); mobility models in `examples/mobility/` and Model Library → "Mobility" |
| Jain fairness index | Not in ns-3; computed in your code from per-flow throughput. Formula in any networking text (Jain, Chiu, Hawe 1984) |

```bash
./ns3 run scratch/vanetTcpCongestionControl
ls cwnd-vehicle*.dat                              # one trace per vehicle, e.g. cwnd-vehicle1-<variant>.dat
gnuplot -persist -e "plot 'cwnd-vehicle3-TcpBbr.dat' with lines"   # use the exact name from ls; man gnuplot
```

---

## Exp10 — RIP vs OSPF (link-state) vs AODV, with a link failure

**File:** `Exp10/assign10_routing.cc`
**What it does:** hosts s0, s1, d0, d1 and routers R0 to R4. Primary path R0–R1–R3 (2 hops) and an alternate path R0–R2–R4–R3 (3 hops). Run with `--proto=rip|ospf|aodv --cond=normal|congestion|failure`. The `failure` case schedules `LinkDown` at 15 s. RIP uses `RipHelper` with `Ipv4ListRoutingHelper` and static default routes on hosts. AODV uses `AodvHelper` set through `InternetStackHelper::SetRoutingHelper`. "OSPF" is **Ipv4GlobalRouting** (link-state / Dijkstra) because mainline ns-3 has no OSPF module (your file says so in its header). Results are appended to `results10.csv`.

| Piece of code | Where to find reference |
|---|---|
| **RIP + link failure** | **`examples/routing/rip-simple-network.cc`**. It builds a RIP network and takes a link down mid-run with `SetDown`. |
| RIPng (IPv6) | `examples/routing/ripng-simple-network.cc` |
| Global (link-state style) routing | `examples/routing/simple-global-routing.cc`, `simple-alternate-routing.cc`, `dynamic-global-routing.cc` (link goes down while running) |
| **AODV** | `src/aodv/examples/aodv-example.cc` (called `aodv.cc` in older releases); model: `src/aodv/model/aodv-routing-protocol.cc`; Model Library → "AODV" |
| Comparing several routing protocols | `examples/routing/manet-routing-compare.cc` (AODV, OLSR, DSDV, DSR) |
| `InternetStackHelper::SetRoutingHelper`, `Ipv4ListRoutingHelper` (priorities) | Doxygen for those classes; Model Library → "Routing overview" |
| `PrintRoutingTable` / `PrintRoutingTableAllAt` (your `Snapshot()` function) | `Ipv4RoutingHelper::PrintRoutingTableAllAt` in Doxygen; used in `rip-simple-network.cc` |
| Static routes: `Ipv4StaticRoutingHelper`, `SetDefaultRoute` | `examples/routing/simple-routing-ping6.cc`, `src/internet/helper/ipv4-static-routing-helper.h` |
| Bringing an interface down (`Ipv4::SetDown`) | `rip-simple-network.cc` and `dynamic-global-routing.cc` |
| Why the RIP convergence delay appears | Model Library → "RIP" (timers: `UnsolicitedRoutingUpdate`, `TimeoutDelay`, `GarbageCollectionDelay`) |
| Packet delivery ratio and delay | FlowMonitor, as in Exp8 |
| OSPF concepts | Textbook; ns-3 has no OSPF in mainline. Third-party OSPF modules exist on the ns-3 App Store but are not in the tarball |

```bash
./ns3 run "scratch/assign10_routing --proto=rip  --cond=failure"
./ns3 run "scratch/assign10_routing --proto=ospf --cond=failure"
./ns3 run "scratch/assign10_routing --proto=aodv --cond=failure"
cat results10.csv
./ns3 run "rip-simple-network --PrintHelp"      # the shipped example, for comparison
```

---

## Exp11 — Wi-Fi vs "Bluetooth" vs Zigbee under mobility

**File:** `Exp11/assign11_hetnet.cc`
**What it does:** three separate networks, each with 1 fixed gateway at the centre and 3 `RandomWalk2dMobilityModel` nodes sending CBR/UDP uplink.

* **Wi-Fi:** 802.11g infrastructure (`WifiHelper`, `YansWifiChannelHelper`, `StaWifiMac` / `ApWifiMac`, `IdealWifiManager`), Tx 16 dBm.
* **Bluetooth:** ns-3 has **no Bluetooth module**, so this is approximated by a low-power 802.11b 1 Mbps link (`ConstantRateWifiManager`), Tx 4 dBm.
* **Zigbee:** IEEE 802.15.4 with `LrWpanHelper` plus `SixLowPanHelper` and IPv6, Tx 0 dBm. The Zigbee network layer is not in mainline.
* It samples received power with a propagation loss model, runs speeds 1, 5, 10 m/s (or one with `--speed=`) and appends to `results11.csv`.

| Piece of code | Where to find reference |
|---|---|
| **Wi-Fi infrastructure (AP + STA)** | `examples/wireless/wifi-simple-infra.cc`, `wifi-tcp.cc`, `wifi-mixed-network.cc` |
| Wi-Fi ad hoc | `examples/wireless/wifi-simple-adhoc.cc` |
| `WIFI_STANDARD_80211b/g`, `SetRemoteStationManager` | Model Library → "Wi-Fi"; `src/wifi/helper/wifi-helper.h`; the examples above |
| `YansWifiChannelHelper`, `YansWifiPhyHelper`, `Ssid`, `StaWifiMac`, `ApWifiMac` | `wifi-simple-infra.cc` shows the whole chain |
| **802.15.4 (Zigbee PHY/MAC)** | `src/lr-wpan/examples/` (`lr-wpan-data.cc`, `lr-wpan-packet-print.cc`, `lr-wpan-error-distance-plot.cc`, `lr-wpan-beacon-mode.cc`); Model Library → "LR-WPAN" |
| **6LoWPAN + IPv6 over 802.15.4** | `src/sixlowpan/examples/` (`example-sixlowpan.cc`, `example-ping-lr-wpan.cc`); Model Library → "6LoWPAN" |
| `Ipv6AddressHelper`, `Ipv6InterfaceContainer` | `examples/routing/simple-routing-ping6.cc`, `examples/ipv6/` |
| **Mobility: `RandomWalk2dMobilityModel`, `RandomRectanglePositionAllocator`** | Model Library → "Mobility"; `examples/mobility/`; `src/mobility/model/random-walk-2d-mobility-model.h`; also `examples/wireless/` examples that set `MobilityHelper` |
| **Propagation / Rx power** (`GetRxPower`, LogDistance, Friis) | `src/propagation/examples/` (for example `main-propagation-loss.cc`); Model Library → "Propagation"; `src/propagation/model/` |
| Tx power setting (`TxPowerStart/End`) | `./ns3 run "scratch/assign11_hetnet --PrintAttributes=ns3::WifiPhy"`; `src/wifi/model/phy.h` docs |
| Sockets used directly (`Socket::CreateSocket`, `Bind`, `SendTo`, `Inet6SocketAddress`) | Tutorial → "Sockets APIs"; `examples/tutorial/fifth.cc`, `sixth.cc` |
| `Simulator::Schedule` for the send loop | `fifth.cc`; Manual → "Events and Simulator" |
| Bluetooth | Not in ns-3 mainline. Your approximation is standard practice for the lab. |

```bash
./ns3 run assign11_hetnet                         # all three speeds
./ns3 run "assign11_hetnet --speed=5"
cat results11.csv
```

---

## Quick "I forgot the API" cheat sheet

| I need… | Do this |
|---|---|
| Example of class **X** | `grep -rl "X" examples src --include=*.cc` |
| Options of an attribute-based class | `./ns3 run "scratch/myprog --PrintAttributes=ns3::ClassName"` |
| Trace source names for a class | Doxygen page for the class → **TraceSources** section |
| How a helper is used | `src/<module>/helper/*.h` plus the examples that call it |
| Meaning of a Wi-Fi/TCP/RIP/AODV parameter | `doc/models/source/<module>.rst` in the tarball (or Model Library online) |
| C function signature and return values | `man 2 <name>` or `man 3 <name>` |
| Which header a C function needs | the SYNOPSIS block at the top of its man page |
| Why my socket call failed | `perror("name")`, then ERRORS section of that man page, then `man 3 errno` |
| Who is using a port | `ss -tulpn \| grep <port>` |
| Capture traffic from a C lab program | `sudo tcpdump -i lo -nn port 9000` (`man tcpdump`) |
