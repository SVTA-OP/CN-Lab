/*
 * tcpUdpDumbbell.cc
 *
 * Topology (dumbbell):
 *
 *   n0 ──┐                      ┌── n2
 *        ├── r0 ══(bottleneck)══r1 ──┤
 *   n1 ──┘                      └── n3
 *
 *   Access links (n0-r0, n1-r0, r1-n2, r1-n3): 10 Mbps, 2 ms   (default)
 *   Bottleneck link (r0-r1):                    2 Mbps, 10 ms  (default, swept)
 *
 * Traffic:
 *   TCP flow: n0 -> n3   (BulkSend + PacketSink), 1024-byte packets
 *   UDP flow: n1 -> n2   (OnOff + PacketSink),     512-byte packets, 1 Mbps default
 *
 * Queueing:
 *   DropTail queue on every device, default max size 50 packets
 *   (queue size on the bottleneck is the one that matters and is swept)
 *
 * Metrics (via FlowMonitor, no manual packet tracking needed):
 *   - Packet loss (tx - rx, and loss %)
 *   - Average end-to-end delay
 *   - Throughput (Mbps)
 *   - TCP retransmissions (approximated as txPackets - unique data delivered,
 *     read from FlowMonitor's lostPackets + retransmission count where available)
 *
 * Build:
 *   cp tcpUdpDumbbell.cc scratch/
 *   ./ns3 build
 *
 * Run (defaults):
 *   ./ns3 run scratch/tcpUdpDumbbell
 *
 * Run (Experiment 1 - sweep bottleneck bandwidth):
 *   for bw in 2 5 10; do
 *     ./ns3 run "scratch/tcpUdpDumbbell --bottleneckBw=${bw}Mbps"
 *   done
 *
 * Run (Experiment 2 - sweep UDP rate):
 *   for r in 500Kbps 1Mbps 2Mbps 5Mbps; do
 *     ./ns3 run "scratch/tcpUdpDumbbell --udpRate=${r}"
 *   done
 *
 * Run (Experiment 3 - sweep queue size):
 *   for q in 20 50 100; do
 *     ./ns3 run "scratch/tcpUdpDumbbell --queueSize=${q}"
 *   done
 *
 * Run (Experiment 4 - sweep bottleneck propagation delay):
 *   for d in 5 10 20 50; do
 *     ./ns3 run "scratch/tcpUdpDumbbell --bottleneckDelay=${d}ms"
 *   done
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpDumbbell");

int
main(int argc, char* argv[])
{
    // ---------------------------------------------------------------
    // Configurable parameters (all overridable from the command line)
    // ---------------------------------------------------------------
    std::string accessBw = "10Mbps";
    std::string accessDelay = "2ms";
    std::string bottleneckBw = "2Mbps";
    std::string bottleneckDelay = "10ms";
    uint32_t queueSize = 50;          // packets, DropTail
    double simTime = 20.0;            // seconds
    uint32_t tcpPacketSize = 1024;    // bytes
    uint32_t udpPacketSize = 512;     // bytes
    std::string udpRate = "1Mbps";
    bool enableTracing = true;
    bool enablePcap = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("accessBw", "Access link bandwidth", accessBw);
    cmd.AddValue("accessDelay", "Access link propagation delay", accessDelay);
    cmd.AddValue("bottleneckBw", "Bottleneck link bandwidth", bottleneckBw);
    cmd.AddValue("bottleneckDelay", "Bottleneck link propagation delay", bottleneckDelay);
    cmd.AddValue("queueSize", "DropTail queue size in packets (applied to all devices)", queueSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("tcpPacketSize", "TCP payload size in bytes", tcpPacketSize);
    cmd.AddValue("udpPacketSize", "UDP payload size in bytes", udpPacketSize);
    cmd.AddValue("udpRate", "UDP OnOff application data rate", udpRate);
    cmd.AddValue("tracing", "Enable ascii tracing", enableTracing);
    cmd.AddValue("pcap", "Enable pcap tracing", enablePcap);
    cmd.Parse(argc, argv);

    // ---------------------------------------------------------------
    // 1. Create nodes
    //    n0, n1 = senders (TCP, UDP)
    //    r0, r1 = routers
    //    n2, n3 = receivers (UDP, TCP)
    // ---------------------------------------------------------------
    NodeContainer endNodes;
    endNodes.Create(4); // 0=n0(TCP src) 1=n1(UDP src) 2=n2(UDP dst) 3=n3(TCP dst)

    NodeContainer routers;
    routers.Create(2); // 0=r0, 1=r1

    Ptr<Node> n0 = endNodes.Get(0);
    Ptr<Node> n1 = endNodes.Get(1);
    Ptr<Node> n2 = endNodes.Get(2);
    Ptr<Node> n3 = endNodes.Get(3);
    Ptr<Node> r0 = routers.Get(0);
    Ptr<Node> r1 = routers.Get(1);

    // ---------------------------------------------------------------
    // 2. Set up DropTail queues + link attributes
    // ---------------------------------------------------------------
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(accessBw));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));
    accessLink.SetQueue("ns3::DropTailQueue<Packet>",
                         "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBw));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>",
                             "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // ---------------------------------------------------------------
    // 3. Install links
    // ---------------------------------------------------------------
    NetDeviceContainer d_n0r0 = accessLink.Install(n0, r0);
    NetDeviceContainer d_n1r0 = accessLink.Install(n1, r0);
    NetDeviceContainer d_r0r1 = bottleneckLink.Install(r0, r1);
    NetDeviceContainer d_r1n2 = accessLink.Install(r1, n2);
    NetDeviceContainer d_r1n3 = accessLink.Install(r1, n3);

    // ---------------------------------------------------------------
    // 4. Internet stack on all nodes (end nodes + routers)
    // ---------------------------------------------------------------
    InternetStackHelper internet;
    internet.InstallAll();

    // ---------------------------------------------------------------
    // 5. IP addressing
    // ---------------------------------------------------------------
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0r0 = address.Assign(d_n0r0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1r0 = address.Assign(d_n1r0);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r1 = address.Assign(d_r0r1);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1n2 = address.Assign(d_r1n2);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1n3 = address.Assign(d_r1n3);

    // ---------------------------------------------------------------
    // 6. Enable routing between all nodes
    // ---------------------------------------------------------------
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address n3Addr = i_r1n3.GetAddress(1); // n3's IP
    Ipv4Address n2Addr = i_r1n2.GetAddress(1); // n2's IP

    // ---------------------------------------------------------------
    // 7. TCP flow: n0 -> n3
    // ---------------------------------------------------------------
    uint16_t tcpPort = 5000;

    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(n3);
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcpSourceHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(n3Addr, tcpPort));
    tcpSourceHelper.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
    tcpSourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited, bounded by stop time
    ApplicationContainer tcpSourceApp = tcpSourceHelper.Install(n0);
    tcpSourceApp.Start(Seconds(1.0));
    tcpSourceApp.Stop(Seconds(simTime));

    // ---------------------------------------------------------------
    // 8. UDP flow: n1 -> n2
    // ---------------------------------------------------------------
    uint16_t udpPort = 6000;

    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(n2);
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpSourceHelper("ns3::UdpSocketFactory",
                                 InetSocketAddress(n2Addr, udpPort));
    udpSourceHelper.SetAttribute("DataRate", StringValue(udpRate));
    udpSourceHelper.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    udpSourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpSourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer udpSourceApp = udpSourceHelper.Install(n1);
    udpSourceApp.Start(Seconds(1.0));
    udpSourceApp.Stop(Seconds(simTime));

    // ---------------------------------------------------------------
    // 9. Tracing
    // ---------------------------------------------------------------
    if (enablePcap)
    {
        bottleneckLink.EnablePcapAll("tcp-udp-dumbbell");
    }
    if (enableTracing)
    {
        AsciiTraceHelper ascii;
        bottleneckLink.EnableAsciiAll(ascii.CreateFileStream("tcp-udp-dumbbell.tr"));
    }

    // ---------------------------------------------------------------
    // 10. FlowMonitor - tracks per-flow tx/rx packets, bytes, delay, loss
    // ---------------------------------------------------------------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // ---------------------------------------------------------------
    // Run
    // ---------------------------------------------------------------
    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // ---------------------------------------------------------------
    // 11. Report results from FlowMonitor
    // ---------------------------------------------------------------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n===== Results (bottleneckBw=" << bottleneckBw
              << ", bottleneckDelay=" << bottleneckDelay
              << ", queueSize=" << queueSize
              << ", udpRate=" << udpRate << ") =====\n";

    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        FlowMonitor::FlowStats fs = flow.second;

        double durationSec = (fs.timeLastRxPacket - fs.timeFirstTxPacket).GetSeconds();
        double throughputMbps =
            (durationSec > 0) ? (fs.rxBytes * 8.0) / (durationSec * 1e6) : 0.0;

        double avgDelayMs = 0.0;
        if (fs.rxPackets > 0)
        {
            avgDelayMs = (fs.delaySum.GetSeconds() / fs.rxPackets) * 1000.0;
        }

        uint64_t lostPackets = fs.txPackets - fs.rxPackets;
        double lossPercent =
            (fs.txPackets > 0) ? (100.0 * lostPackets / fs.txPackets) : 0.0;

        std::string proto = (t.protocol == 6) ? "TCP" : (t.protocol == 17) ? "UDP" : "OTHER";

        std::cout << "\nFlow " << flow.first << " (" << proto << ") "
                  << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << "\n";
        std::cout << "  Tx Packets:   " << fs.txPackets << "\n";
        std::cout << "  Rx Packets:   " << fs.rxPackets << "\n";
        std::cout << "  Lost Packets: " << lostPackets << " (" << lossPercent << "%)\n";
        std::cout << "  Throughput:   " << throughputMbps << " Mbps\n";
        std::cout << "  Avg Delay:    " << avgDelayMs << " ms\n";

        if (proto == "TCP")
        {
            // FlowMonitor counts every segment transmission attempt, including
            // retransmissions, in txPackets. A rough retransmission estimate is
            // txPackets beyond what was actually delivered uniquely (rxPackets),
            // adjusted for genuine loss. This is an approximation; for exact
            // retransmission counts, cross-check with TCP socket trace sources
            // (e.g. congestion window / next tx sequence traces).
            std::cout << "  Approx. TCP retransmissions: " << lostPackets << "\n";
        }
    }
    std::cout << "\n===============================================\n";

    Simulator::Destroy();
    return 0;
}
