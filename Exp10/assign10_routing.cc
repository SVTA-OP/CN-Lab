/*
 * Assignment 10: Comparative evaluation of RIP, OSPF(link-state) and AODV
 *
 * Topology (point-to-point links):
 *
 *   s0 --\                 /-- R1 --\                 /-- d0
 *         R0 ---(primary)--           R3 --------------
 *   s1 --/  \                         /               \-- d1
 *            \-- R2 ---- R4 ---------/   (alternate, longer)
 *
 * Primary path   : R0-R1-R3   (2 hops)
 * Alternate path : R0-R2-R4-R3 (3 hops)  -> used after R0-R1 fails
 *
 * NOTE: mainline NS-3 has no OSPF module. OSPF is represented by NS-3's
 * Global (link-state / Dijkstra) routing, recomputed on interface events.
 *
 * Run:  ./ns3 run "assign10_routing --proto=rip --cond=failure"
 *   proto : rip | ospf | aodv
 *   cond  : normal | congestion | failure
 * Appends one line to results10.csv: proto,cond,pdr,delay_ms,updates
 */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

using namespace ns3;

static NodeContainer g_routers;
static std::map<uint32_t, std::set<std::string>> g_prev;
static uint32_t g_updates = 0;

// Poll every router's table; count a "routing table update" whenever the
// set of (destination, next-hop) entries changes.
void
Snapshot()
{
    for (uint32_t i = 0; i < g_routers.GetN(); ++i)
    {
        std::ostringstream oss;
        Ptr<OutputStreamWrapper> w = Create<OutputStreamWrapper>(&oss);
        g_routers.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol()->PrintRoutingTable(w);

        std::set<std::string> cur;
        std::istringstream in(oss.str());
        std::string line;
        while (std::getline(in, line))
        {
            if (line.find("Node:") != std::string::npos ||
                line.find("Priority") != std::string::npos ||
                line.find("Destination") != std::string::npos ||
                line.find("Routing") != std::string::npos)
            {
                continue;
            }
            std::istringstream ls(line);
            std::string a, b;
            ls >> a >> b;
            if (!a.empty())
            {
                cur.insert(a + " " + b);
            }
        }
        if (g_prev.count(i) && g_prev[i] != cur)
        {
            g_updates++;
        }
        g_prev[i] = cur;
    }
    Simulator::Schedule(Seconds(0.5), &Snapshot);
}

void
LinkDown(Ipv4InterfaceContainer ic)
{
    for (uint32_t k = 0; k < 2; ++k)
    {
        ic.Get(k).first->SetDown(ic.Get(k).second);
    }
    std::cout << "[" << Simulator::Now().GetSeconds() << "s] Link R0-R1 DOWN\n";
}

Ipv4InterfaceContainer
MakeLink(NodeContainer n, PointToPointHelper& p, Ipv4AddressHelper& a)
{
    NetDeviceContainer d = p.Install(n);
    Ipv4InterfaceContainer i = a.Assign(d);
    a.NewNetwork();
    return i;
}

void
AddFlow(Ptr<Node> src, Ipv4Address dst, uint16_t port, std::string rate)
{
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dst, port));
    onoff.SetConstantRate(DataRate(rate), 1000); // CBR
    ApplicationContainer a = onoff.Install(src);
    a.Start(Seconds(5.0));
    a.Stop(Seconds(28.0));
}

int
main(int argc, char* argv[])
{
    std::string proto = "rip";
    std::string cond = "normal";
    CommandLine cmd;
    cmd.AddValue("proto", "rip | ospf | aodv", proto);
    cmd.AddValue("cond", "normal | congestion | failure", cond);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    // ---- Nodes ----
    NodeContainer hosts;
    hosts.Create(4); // 0:s0 1:s1 2:d0 3:d1
    g_routers.Create(5); // R0..R4
    Ptr<Node> s0 = hosts.Get(0), s1 = hosts.Get(1), d0 = hosts.Get(2), d1 = hosts.Get(3);
    Ptr<Node> R0 = g_routers.Get(0), R1 = g_routers.Get(1), R2 = g_routers.Get(2),
              R3 = g_routers.Get(3), R4 = g_routers.Get(4);

    // ---- Routing protocol / Internet stack ----
    InternetStackHelper hostStack, routerStack;
    if (proto == "rip")
    {
        RipHelper rip;
        rip.Set("UnsolicitedRoutingUpdate", TimeValue(Seconds(5)));
        rip.Set("TimeoutDelay", TimeValue(Seconds(15)));
        Ipv4StaticRoutingHelper sr;
        Ipv4ListRoutingHelper list;
        list.Add(sr, 0);
        list.Add(rip, 10);
        routerStack.SetRoutingHelper(list);
    }
    else if (proto == "aodv")
    {
        AodvHelper aodv;
        hostStack.SetRoutingHelper(aodv);
        routerStack.SetRoutingHelper(aodv);
    }
    // "ospf": default stack => Ipv4GlobalRouting (link-state)
    hostStack.Install(hosts);
    routerStack.Install(g_routers);

    // ---- Links & addressing ----
    PointToPointHelper access, core, alt;
    access.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    access.SetChannelAttribute("Delay", StringValue("1ms"));
    core.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    core.SetChannelAttribute("Delay", StringValue("2ms"));
    alt.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    alt.SetChannelAttribute("Delay", StringValue("5ms"));

    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iS0 = MakeLink(NodeContainer(R0, s0), access, addr);
    Ipv4InterfaceContainer iS1 = MakeLink(NodeContainer(R0, s1), access, addr);
    Ipv4InterfaceContainer iPri = MakeLink(NodeContainer(R0, R1), core, addr); // fails later
    MakeLink(NodeContainer(R1, R3), core, addr);
    MakeLink(NodeContainer(R0, R2), alt, addr);
    MakeLink(NodeContainer(R2, R4), alt, addr);
    MakeLink(NodeContainer(R4, R3), alt, addr);
    Ipv4InterfaceContainer iD0 = MakeLink(NodeContainer(R3, d0), access, addr);
    Ipv4InterfaceContainer iD1 = MakeLink(NodeContainer(R3, d1), access, addr);

    if (proto == "rip")
    {
        // hosts are not RIP speakers: give them a default route
        Ipv4StaticRoutingHelper sr;
        sr.GetStaticRouting(s0->GetObject<Ipv4>())->SetDefaultRoute(iS0.GetAddress(0), 1);
        sr.GetStaticRouting(s1->GetObject<Ipv4>())->SetDefaultRoute(iS1.GetAddress(0), 1);
        sr.GetStaticRouting(d0->GetObject<Ipv4>())->SetDefaultRoute(iD0.GetAddress(0), 1);
        sr.GetStaticRouting(d1->GetObject<Ipv4>())->SetDefaultRoute(iD1.GetAddress(0), 1);
    }
    if (proto == "ospf")
    {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // ---- Traffic (CBR/UDP) ----
    Ipv4Address a0 = iD0.GetAddress(1), a1 = iD1.GetAddress(1);
    for (uint16_t p = 9; p < 15; ++p)
    {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), p));
        sink.Install(d0).Start(Seconds(0));
        sink.Install(d1).Start(Seconds(0));
    }
    AddFlow(s0, a0, 9, "1500kbps");  // base flows
    AddFlow(s1, a1, 10, "1500kbps");
    if (cond == "congestion")        // extra simultaneous flows
    {
        AddFlow(s0, a1, 11, "1500kbps");
        AddFlow(s1, a0, 12, "1500kbps");
        AddFlow(s0, a0, 13, "1500kbps");
        AddFlow(s1, a1, 14, "1500kbps");
    }
    if (cond == "failure")
    {
        Simulator::Schedule(Seconds(15.0), &LinkDown, iPri);
    }

    Simulator::Schedule(Seconds(0.5), &Snapshot);
    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> mon = fmh.InstallAll();

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();

    // ---- Metrics ----
    mon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> cls = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    uint64_t tx = 0, rx = 0;
    double delay = 0;
    for (auto& kv : mon->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = cls->FindFlow(kv.first);
        if (t.destinationPort < 9 || t.destinationPort > 14)
        {
            continue; // ignore routing-protocol control traffic
        }
        tx += kv.second.txPackets;
        rx += kv.second.rxPackets;
        delay += kv.second.delaySum.GetSeconds();
    }
    double pdr = tx ? 100.0 * rx / tx : 0;
    double dms = rx ? 1000.0 * delay / rx : 0;
    std::cout << proto << " " << cond << " PDR=" << pdr << "% delay=" << dms
              << "ms updates=" << g_updates << std::endl;

    std::ofstream f("results10.csv", std::ios::app);
    f << proto << "," << cond << "," << pdr << "," << dms << "," << g_updates << "\n";

    Simulator::Destroy();
    return 0;
}
