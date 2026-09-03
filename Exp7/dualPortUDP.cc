/*
 * udp-dual-port.cc
 *
 * Topology:
 *   Node1 ────────────── Node2
 *           P2P Link
 *
 * Two independent UDP flows, distinguished purely by destination port:
 *   Flow 1: UdpClient(Node1) -> UdpServer(Node2)  port 5000
 *   Flow 2: UdpClient(Node1) -> UdpServer(Node2)  port 6000
 *
 * The point-to-point link delay is a command-line parameter so you can
 * sweep it (2ms, 5ms, 10ms, 20ms, ...) without editing the code.
 *
 * At the end of the run the script prints the throughput of each flow,
 * computed from UdpServer::GetReceived() (packets actually delivered to
 * the correct application, proving port-based demux worked).
 *
 * Build (with a standard ns-3 source tree, e.g. ns-3.4x):
 *   cp udp-dual-port.cc scratch/
 *   ./ns3 build
 *
 * Run (single flow, one latency):
 *   ./ns3 run "scratch/udp-dual-port --delay=5ms --secondFlow=false"
 *
 * Run (both flows):
 *   ./ns3 run "scratch/udp-dual-port --delay=5ms --secondFlow=true"
 *
 * Sweep example (bash):
 *   for d in 2 5 10 20; do
 *     ./ns3 run "scratch/udp-dual-port --delay=${d}ms --secondFlow=true --pcap=false"
 *   done
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpDualPortExample");

int
main(int argc, char* argv[])
{
    // ---------------------------------------------------------------
    // Configurable parameters
    // ---------------------------------------------------------------
    std::string delay = "5ms";     // link latency (one-way propagation delay)
    std::string bandwidth = "10Mbps"; // link data rate
    std::string rate1 = "5Mbps";   // Flow 1 UDP client sending rate
    std::string rate2 = "5Mbps";   // Flow 2 UDP client sending rate
    uint32_t packetSize = 1024;    // bytes per UDP packet payload
    double simTime = 10.0;         // seconds
    bool secondFlow = true;        // enable/disable the second application
    bool enablePcap = true;

    uint16_t port1 = 5000;
    uint16_t port2 = 6000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("delay", "Point-to-point link delay (e.g. 5ms)", delay);
    cmd.AddValue("bandwidth", "Point-to-point link data rate (e.g. 10Mbps)", bandwidth);
    cmd.AddValue("rate1", "Flow 1 UDP client data rate", rate1);
    cmd.AddValue("rate2", "Flow 2 UDP client data rate", rate2);
    cmd.AddValue("packetSize", "UDP payload size in bytes", packetSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("secondFlow", "Enable second UDP flow on port 6000", secondFlow);
    cmd.AddValue("pcap", "Enable pcap tracing", enablePcap);
    cmd.Parse(argc, argv);

    // ---------------------------------------------------------------
    // 1. Create the two nodes and the point-to-point link between them
    // ---------------------------------------------------------------
    NodeContainer nodes;
    nodes.Create(2); // nodes.Get(0) = Node1, nodes.Get(1) = Node2

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = p2p.Install(nodes);

    // ---------------------------------------------------------------
    // Internet stack + addressing
    // ---------------------------------------------------------------
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4Address node2Addr = interfaces.GetAddress(1); // Node2's IP

    // ---------------------------------------------------------------
    // 2. Configure UDP applications
    //    Convert the requested rate (bits/sec) into an interval between
    //    packets so UdpClientHelper (which is packet-count/interval based)
    //    reproduces the desired data rate.
    // ---------------------------------------------------------------
    auto rateToInterval = [&](const std::string& rateStr) -> Time {
        DataRate dr(rateStr);
        double bitsPerSec = dr.GetBitRate();
        double packetBits = packetSize * 8.0;
        double interval = packetBits / bitsPerSec; // seconds/packet
        return Seconds(interval);
    };

    uint32_t maxPackets = 4294967295u; // effectively unlimited; app stops at simTime

    // ---- Flow 1: Node1 -> Node2, port 5000 ----
    UdpServerHelper server1(port1);
    ApplicationContainer serverApps1 = server1.Install(nodes.Get(1)); // Node2
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simTime + 1.0));

    UdpClientHelper client1(node2Addr, port1);
    client1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client1.SetAttribute("Interval", TimeValue(rateToInterval(rate1)));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps1 = client1.Install(nodes.Get(0)); // Node1
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simTime));

    // ---- Flow 2: Node1 -> Node2, port 6000 (optional) ----
    ApplicationContainer serverApps2, clientApps2;
    if (secondFlow)
    {
        UdpServerHelper server2(port2);
        serverApps2 = server2.Install(nodes.Get(1)); // Node2
        serverApps2.Start(Seconds(0.0));
        serverApps2.Stop(Seconds(simTime + 1.0));

        UdpClientHelper client2(node2Addr, port2);
        client2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client2.SetAttribute("Interval", TimeValue(rateToInterval(rate2)));
        client2.SetAttribute("PacketSize", UintegerValue(packetSize));

        clientApps2 = client2.Install(nodes.Get(0)); // Node1
        clientApps2.Start(Seconds(1.0));
        clientApps2.Stop(Seconds(simTime));
    }

    // ---------------------------------------------------------------
    // 6. PCAP tracing (open in Wireshark, filter udp.port==5000 or 6000)
    // ---------------------------------------------------------------
    if (enablePcap)
    {
        p2p.EnablePcapAll("udp-dual-port");
    }

    // ---------------------------------------------------------------
    // Run
    // ---------------------------------------------------------------
    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    // ---------------------------------------------------------------
    // 3/5. Throughput computation from actual packets delivered to each
    // UdpServer application (proves port-based demultiplexing worked).
    // ---------------------------------------------------------------
    double activeTime = simTime - 1.0; // client active window

    Ptr<UdpServer> srv1 = DynamicCast<UdpServer>(serverApps1.Get(0));
    uint64_t rx1 = srv1->GetReceived();
    double bytes1 = static_cast<double>(rx1) * packetSize;
    double throughput1Mbps = (bytes1 * 8.0) / (activeTime * 1e6);

    std::cout << "\n===== Results (delay=" << delay << ", bandwidth=" << bandwidth << ") =====\n";
    std::cout << "Flow 1 (port " << port1 << "): received " << rx1
              << " packets, throughput = " << throughput1Mbps << " Mbps\n";

    if (secondFlow)
    {
        Ptr<UdpServer> srv2 = DynamicCast<UdpServer>(serverApps2.Get(0));
        uint64_t rx2 = srv2->GetReceived();
        double bytes2 = static_cast<double>(rx2) * packetSize;
        double throughput2Mbps = (bytes2 * 8.0) / (activeTime * 1e6);
        std::cout << "Flow 2 (port " << port2 << "): received " << rx2
                  << " packets, throughput = " << throughput2Mbps << " Mbps\n";
        std::cout << "Aggregate throughput = " << (throughput1Mbps + throughput2Mbps) << " Mbps\n";
    }
    std::cout << "===============================================\n";

    Simulator::Destroy();
    return 0;
}