/*
 * vanetTcpCongestionControl.cc
 *
 * Scenario: Intelligent Transportation System / VANET
 *
 *   Vehicle1 ──┐
 *   Vehicle2 ──┤
 *   Vehicle3 ──┼── Roadside Router ──(Internet link)── Traffic Control Server
 *   Vehicle4 ──┤
 *   Vehicle5 ──┘
 *
 * Each vehicle sends a continuous TCP stream (BulkSend) of traffic/camera/
 * road-condition data to the traffic control server. Vehicles are split
 * across three TCP congestion-control variants so all three compete for
 * the same bottleneck (the router->server "Internet" link) at once:
 *
 *   Vehicle1, Vehicle4 -> TCP NewReno  (classic Reno-style AIMD)
 *   Vehicle2, Vehicle5 -> TCP Cubic
 *   Vehicle3           -> TCP BBR
 *
 * This lets you directly compare Throughput / Delay / Packet loss / Fairness
 * / Congestion window evolution between the three algorithms under the same
 * congestion conditions, which is the point of the assignment.
 *
 * Access links (vehicle -> router): represent the wireless V2I hop.
 * Bottleneck link (router -> server): represents the shared uplink to the
 * Internet/traffic-control server, i.e. where congestion actually happens.
 *
 * Outputs:
 *   - Per-flow throughput, delay, loss via FlowMonitor (printed at the end)
 *   - Per-vehicle congestion-window trace files: cwnd-vehicleN.dat
 *     (columns: time_seconds  cwnd_bytes) for plotting Reno vs Cubic vs BBR
 *   - Jain's fairness index computed across the 5 flows' throughputs
 *
 * Build:
 *   cp vanetTcpCongestionControl.cc scratch/
 *   ./ns3 build
 *
 * Run (defaults):
 *   ./ns3 run scratch/vanetTcpCongestionControl
 *
 * Run (sweep bottleneck bandwidth to vary congestion severity):
 *   for bw in 1 2 5 10; do
 *     ./ns3 run "scratch/vanetTcpCongestionControl --bottleneckBw=${bw}Mbps"
 *   done
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-module.h"

#include <fstream>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetTcpCongestionControl");

// One output stream per vehicle for cwnd tracing
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_cwndStreams;

static void
CwndTrace(uint32_t vehicleId, uint32_t oldCwnd, uint32_t newCwnd)
{
    *g_cwndStreams[vehicleId]->GetStream()
        << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Called shortly after each app starts, once the socket exists, to hook the trace
static void
ConnectCwndTrace(uint32_t vehicleNodeId, uint32_t vehicleLabel)
{
    std::ostringstream path;
    path << "/NodeList/" << vehicleNodeId
         << "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
    Config::ConnectWithoutContextFailSafe(path.str(), MakeBoundCallback(&CwndTrace, vehicleLabel));
}

int
main(int argc, char* argv[])
{
    // ---------------------------------------------------------------
    // Configurable parameters
    // ---------------------------------------------------------------
    std::string accessBw = "10Mbps";      // vehicle <-> router (wireless V2I hop)
    std::string accessDelay = "5ms";
    std::string bottleneckBw = "2Mbps";   // router -> server "Internet" uplink
    std::string bottleneckDelay = "20ms"; // WAN-ish latency to the server
    uint32_t queueSize = 50;              // packets, DropTail
    double simTime = 30.0;                // seconds
    uint32_t packetSize = 1024;           // bytes, TCP payload (sensor/camera data)

    CommandLine cmd(__FILE__);
    cmd.AddValue("accessBw", "Vehicle-to-router access link bandwidth", accessBw);
    cmd.AddValue("accessDelay", "Vehicle-to-router access link delay", accessDelay);
    cmd.AddValue("bottleneckBw", "Router-to-server bottleneck bandwidth", bottleneckBw);
    cmd.AddValue("bottleneckDelay", "Router-to-server link delay", bottleneckDelay);
    cmd.AddValue("queueSize", "DropTail queue size in packets", queueSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("packetSize", "TCP payload size in bytes", packetSize);
    cmd.Parse(argc, argv);

    // ---------------------------------------------------------------
    // 1. Nodes: 5 vehicles, 1 roadside router, 1 traffic control server
    // ---------------------------------------------------------------
    NodeContainer vehicles;
    vehicles.Create(5); // 0..4 = Vehicle1..Vehicle5

    NodeContainer router;
    router.Create(1);

    NodeContainer server;
    server.Create(1);

    // ---------------------------------------------------------------
    // 2. Links
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

    std::vector<NetDeviceContainer> vehicleDevices(5);
    for (uint32_t i = 0; i < 5; ++i)
    {
        vehicleDevices[i] = accessLink.Install(vehicles.Get(i), router.Get(0));
    }
    NetDeviceContainer routerServerDevices = bottleneckLink.Install(router.Get(0), server.Get(0));

    // ---------------------------------------------------------------
    // 3. Internet stack
    // ---------------------------------------------------------------
    InternetStackHelper internet;
    internet.InstallAll();

    // ---------------------------------------------------------------
    // 4. Assign per-node TCP congestion control BEFORE apps are installed.
    //    Vehicle1, Vehicle4 -> NewReno
    //    Vehicle2, Vehicle5 -> Cubic
    //    Vehicle3           -> BBR
    // ---------------------------------------------------------------
    std::vector<std::string> variantName = {"Reno", "Cubic", "BBR", "Reno", "Cubic"};
    std::vector<std::string> variantTypeId = {
        "ns3::TcpNewReno", // Vehicle1
        "ns3::TcpCubic",   // Vehicle2
        "ns3::TcpBbr",     // Vehicle3
        "ns3::TcpNewReno", // Vehicle4
        "ns3::TcpCubic"    // Vehicle5
    };

    for (uint32_t i = 0; i < 5; ++i)
    {
        std::ostringstream path;
        path << "/NodeList/" << vehicles.Get(i)->GetId()
             << "/$ns3::TcpL4Protocol/SocketType";
        Config::Set(path.str(), TypeIdValue(TypeId::LookupByName(variantTypeId[i])));
    }

    // ---------------------------------------------------------------
    // 5. IP addressing
    // ---------------------------------------------------------------
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> vehicleInterfaces(5);

    for (uint32_t i = 0; i < 5; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        vehicleInterfaces[i] = address.Assign(vehicleDevices[i]);
    }

    address.SetBase("10.1.100.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address serverAddr = routerServerInterfaces.GetAddress(1);

    // ---------------------------------------------------------------
    // 6. Applications: each vehicle streams TCP data to the server
    //    on its own port so FlowMonitor and PacketSink can separate flows.
    // ---------------------------------------------------------------
    uint16_t basePort = 5000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;

    for (uint32_t i = 0; i < 5; ++i)
    {
        uint16_t port = basePort + i;

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(server.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        sinkApps.Add(sinkApp);

        BulkSendHelper sourceHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(serverAddr, port));
        sourceHelper.SetAttribute("SendSize", UintegerValue(packetSize));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited, bounded by stop time
        ApplicationContainer sourceApp = sourceHelper.Install(vehicles.Get(i));
        // stagger starts slightly so all 5 don't hit slow-start at exactly the same instant
        sourceApp.Start(Seconds(1.0 + 0.1 * i));
        sourceApp.Stop(Seconds(simTime));
        sourceApps.Add(sourceApp);

        // Hook cwnd tracing once the socket exists (shortly after app start)
        uint32_t vehicleLabel = i + 1; // Vehicle1..Vehicle5
        std::ostringstream fname;
        fname << "cwnd-vehicle" << vehicleLabel << "-" << variantName[i] << ".dat";
        AsciiTraceHelper asciiTraceHelper;
        g_cwndStreams[vehicleLabel] = asciiTraceHelper.CreateFileStream(fname.str());

        Simulator::Schedule(Seconds(1.0 + 0.1 * i + 0.01),
                             &ConnectCwndTrace,
                             vehicles.Get(i)->GetId(),
                             vehicleLabel);
    }

    // ---------------------------------------------------------------
    // 7. Tracing (pcap on the bottleneck link, where congestion is visible)
    // ---------------------------------------------------------------
    bottleneckLink.EnablePcapAll("vanet-tcp-congestion");

    // ---------------------------------------------------------------
    // 8. FlowMonitor
    // ---------------------------------------------------------------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // ---------------------------------------------------------------
    // Run
    // ---------------------------------------------------------------
    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // ---------------------------------------------------------------
    // 9. Report results per flow + Jain's fairness index
    // ---------------------------------------------------------------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n===== VANET TCP Congestion Control Results =====\n";
    std::cout << "bottleneckBw=" << bottleneckBw << ", bottleneckDelay=" << bottleneckDelay
              << ", queueSize=" << queueSize << "\n";

    std::vector<double> throughputs; // for fairness index, one per vehicle flow

    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        FlowMonitor::FlowStats fs = flow.second;

        // Only report forward (vehicle -> server) data flows, i.e. destination port
        // in [5000, 5004]; ACK-direction flows are the reverse 5-tuple.
        if (t.destinationPort < basePort || t.destinationPort >= basePort + 5)
        {
            continue;
        }

        uint32_t vehicleIdx = t.destinationPort - basePort;

        double durationSec = (fs.timeLastRxPacket - fs.timeFirstTxPacket).GetSeconds();
        double throughputMbps =
            (durationSec > 0) ? (fs.rxBytes * 8.0) / (durationSec * 1e6) : 0.0;

        double avgDelayMs = 0.0;
        if (fs.rxPackets > 0)
        {
            avgDelayMs = (fs.delaySum.GetSeconds() / fs.rxPackets) * 1000.0;
        }

        uint64_t lostPackets = fs.txPackets - fs.rxPackets;
        double lossPercent = (fs.txPackets > 0) ? (100.0 * lostPackets / fs.txPackets) : 0.0;

        std::cout << "\nVehicle" << (vehicleIdx + 1) << " [" << variantName[vehicleIdx] << "] "
                  << t.sourceAddress << " -> " << t.destinationAddress << ":" << t.destinationPort
                  << "\n";
        std::cout << "  Tx Packets:   " << fs.txPackets << "\n";
        std::cout << "  Rx Packets:   " << fs.rxPackets << "\n";
        std::cout << "  Lost Packets: " << lostPackets << " (" << lossPercent << "%)\n";
        std::cout << "  Throughput:   " << throughputMbps << " Mbps\n";
        std::cout << "  Avg Delay:    " << avgDelayMs << " ms\n";
        std::cout << "  Approx. retransmissions: " << lostPackets << "\n";

        throughputs.push_back(throughputMbps);
    }

    // Jain's Fairness Index = (sum(x_i))^2 / (n * sum(x_i^2))
    // Ranges 0..1; 1 = perfectly fair (all flows get equal throughput).
    if (!throughputs.empty())
    {
        double sum = 0.0, sumSq = 0.0;
        for (double x : throughputs)
        {
            sum += x;
            sumSq += x * x;
        }
        double n = static_cast<double>(throughputs.size());
        double fairnessIndex = (sum * sum) / (n * sumSq);

        std::cout << "\nJain's Fairness Index across " << throughputs.size()
                  << " vehicle flows: " << fairnessIndex << "  (1.0 = perfectly fair)\n";
    }

    std::cout << "\nPer-vehicle congestion window traces written to cwnd-vehicleN-<variant>.dat\n";
    std::cout << "===============================================\n";

    Simulator::Destroy();
    return 0;
}
