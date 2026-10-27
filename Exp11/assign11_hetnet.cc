/*
 * Assignment 11: Smart-campus heterogeneous wireless network
 *
 *  Wi-Fi   : 802.11g infrastructure (AP = gateway), IPv4, Tx 16 dBm
 *  Bluetooth: NS-3 has no Bluetooth module -> approximated by a short-range,
 *            low-power (4 dBm) 1 Mbps 802.11b link (DSSS 1 Mbps), IPv4
 *  Zigbee  : IEEE 802.15.4 (lr-wpan) PHY/MAC + 6LoWPAN, IPv6, Tx 0 dBm
 *            (Zigbee NWK layer is not in mainline NS-3)
 *
 * Each technology: 1 fixed gateway at the area centre + 3 mobile nodes using
 * RandomWalk2d at a given speed. Mobile nodes send CBR/UDP uplink to the gateway.
 *
 * Run all speeds (1, 5, 10 m/s): ./ns3 run assign11_hetnet
 * Single speed:                   ./ns3 run "assign11_hetnet --speed=5"
 * Appends to results11.csv: tech,speed,rxpower_dbm,throughput_kbps,loss_pct,delay_ms
 */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace ns3;

struct Stats
{
    uint64_t tx = 0, rx = 0, bytes = 0;
    double delay = 0;       // seconds, summed
    double rxPower = 0;     // dBm, summed
    uint64_t rxPowerN = 0;
};

static Stats g_st[3]; // 0=WiFi 1=Bluetooth 2=Zigbee
static const double START = 5.0, STOP = 55.0;
static Ptr<LogDistancePropagationLossModel> g_loss;

// Sender: payload carries send timestamp (first 8 bytes) for delay measurement
void
SendLoop(Ptr<Socket> s, uint32_t size, Time iv, Stats* st)
{
    int64_t ts = Simulator::Now().GetNanoSeconds();
    Ptr<Packet> p = Create<Packet>(reinterpret_cast<uint8_t*>(&ts), 8);
    p->AddAtEnd(Create<Packet>(size - 8));
    st->tx++;
    s->Send(p);
    if (Simulator::Now() < Seconds(STOP))
    {
        Simulator::Schedule(iv, &SendLoop, s, size, iv, st);
    }
}

void
RecvCb(Stats* st, Ptr<Socket> s)
{
    Ptr<Packet> p;
    Address from;
    while ((p = s->RecvFrom(from)))
    {
        int64_t ts = 0;
        p->CopyData(reinterpret_cast<uint8_t*>(&ts), 8);
        st->rx++;
        st->bytes += p->GetSize();
        st->delay += (Simulator::Now().GetNanoSeconds() - ts) * 1e-9;
    }
}

// Sample received power (dBm) between each mobile node and its gateway
void
SampleRxPower(NodeContainer mobiles, Ptr<Node> gw, double txDbm, Stats* st)
{
    for (uint32_t i = 0; i < mobiles.GetN(); ++i)
    {
        st->rxPower += g_loss->CalcRxPower(txDbm,
                                           mobiles.Get(i)->GetObject<MobilityModel>(),
                                           gw->GetObject<MobilityModel>());
        st->rxPowerN++;
    }
    Simulator::Schedule(Seconds(1.0), &SampleRxPower, mobiles, gw, txDbm, st);
}

void
SetupMobility(NodeContainer& gw, NodeContainer& mob, double speed, double area)
{
    MobilityHelper g;
    Ptr<ListPositionAllocator> pa = CreateObject<ListPositionAllocator>();
    pa->Add(Vector(area / 2, area / 2, 0));
    g.SetPositionAllocator(pa);
    g.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    g.Install(gw);

    MobilityHelper m;
    m.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                           "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string(area) + "]"),
                           "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string(area) + "]"));
    m.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                       "Bounds", RectangleValue(Rectangle(0, area, 0, area)),
                       "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]"),
                       "Mode", StringValue("Time"),
                       "Time", StringValue("2s"));
    m.Install(mob);
}

// Wi-Fi style technology (used for Wi-Fi and the Bluetooth approximation)
void
BuildWifiLike(int idx, bool bluetooth, double speed, double area, const char* net, uint16_t port)
{
    NodeContainer gw, mob;
    gw.Create(1);
    mob.Create(3);
    SetupMobility(gw, mob, speed, area);

    WifiHelper wifi;
    double txDbm;
    if (bluetooth)
    {
        wifi.SetStandard(WIFI_STANDARD_80211b);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("DsssRate1Mbps"),
                                     "ControlMode", StringValue("DsssRate1Mbps"));
        txDbm = 4.0;
    }
    else
    {
        wifi.SetStandard(WIFI_STANDARD_80211g);
        wifi.SetRemoteStationManager("ns3::IdealWifiManager");
        txDbm = 16.0;
    }
    YansWifiChannelHelper ch = YansWifiChannelHelper::Default(); // LogDistance, own channel per tech
    YansWifiPhyHelper phy;
    phy.SetChannel(ch.Create());
    phy.Set("TxPowerStart", DoubleValue(txDbm));
    phy.Set("TxPowerEnd", DoubleValue(txDbm));

    Ssid ssid(bluetooth ? "bt-net" : "wifi-net");
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDev = wifi.Install(phy, mac, mob);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDev = wifi.Install(phy, mac, gw);

    InternetStackHelper in;
    in.Install(gw);
    in.Install(mob);
    Ipv4AddressHelper a;
    a.SetBase(net, "255.255.255.0");
    Ipv4InterfaceContainer gi = a.Assign(apDev);
    a.Assign(staDev);

    Stats* st = &g_st[idx];
    Ptr<Socket> rx = Socket::CreateSocket(gw.Get(0), UdpSocketFactory::GetTypeId());
    rx->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    rx->SetRecvCallback(MakeBoundCallback(&RecvCb, st));

    uint32_t size = bluetooth ? 500 : 1000;
    Time iv = bluetooth ? MilliSeconds(20) : MilliSeconds(10);
    for (uint32_t i = 0; i < mob.GetN(); ++i)
    {
        Ptr<Socket> s = Socket::CreateSocket(mob.Get(i), UdpSocketFactory::GetTypeId());
        s->Connect(InetSocketAddress(gi.GetAddress(0), port));
        Simulator::Schedule(Seconds(START + 0.1 * i), &SendLoop, s, size, iv, st);
    }
    Simulator::Schedule(Seconds(START), &SampleRxPower, mob, gw.Get(0), txDbm, st);
}

void
BuildZigbee(int idx, double speed, double area, uint16_t port)
{
    NodeContainer gw, mob;
    gw.Create(1);
    mob.Create(3);
    SetupMobility(gw, mob, speed, area);
    NodeContainer all(gw, mob);

    LrWpanHelper lr;
    lr.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lr.AddPropagationLossModel("ns3::LogDistancePropagationLossModel");
    NetDeviceContainer dev = lr.Install(all);
    lr.CreateAssociatedPan(dev, 10);

    InternetStackHelper in;
    in.Install(all);
    SixLowPanHelper six;
    NetDeviceContainer sdev = six.Install(dev);

    Ipv6AddressHelper a6;
    a6.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ic = a6.Assign(sdev);

    Stats* st = &g_st[idx];
    Ptr<Socket> rx = Socket::CreateSocket(gw.Get(0), UdpSocketFactory::GetTypeId());
    rx->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), port));
    rx->SetRecvCallback(MakeBoundCallback(&RecvCb, st));

    for (uint32_t i = 0; i < mob.GetN(); ++i)
    {
        Ptr<Socket> s = Socket::CreateSocket(mob.Get(i), UdpSocketFactory::GetTypeId());
        s->Connect(Inet6SocketAddress(ic.GetAddress(0, 1), port));
        Simulator::Schedule(Seconds(START + 0.1 * i), &SendLoop, s, 40u, MilliSeconds(50), st);
    }
    Simulator::Schedule(Seconds(START), &SampleRxPower, mob, gw.Get(0), 0.0, st);
}

// Runs ONE complete simulation for a given node speed
void
RunOne(double speed, double area)
{
    // reset per-run state
    for (auto& st : g_st)
    {
        st = Stats();
    }
    g_loss = CreateObject<LogDistancePropagationLossModel>(); // same defaults as channels

    BuildWifiLike(0, false, speed, area, "10.1.1.0", 9001);
    BuildWifiLike(1, true, speed, area, "10.1.2.0", 9002);
    BuildZigbee(2, speed, area, 9003);

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();

    const char* names[3] = {"WiFi", "Bluetooth", "Zigbee"};
    std::ofstream f("results11.csv", std::ios::app);
    for (int i = 0; i < 3; ++i)
    {
        Stats& s = g_st[i];
        double pw = s.rxPowerN ? s.rxPower / s.rxPowerN : 0;
        double thr = s.bytes * 8.0 / (STOP - START) / 1000.0;
        double loss = s.tx ? 100.0 * (s.tx - s.rx) / s.tx : 0;
        double dly = s.rx ? 1000.0 * s.delay / s.rx : 0;
        std::cout << names[i] << " v=" << speed << " Rx=" << pw << "dBm thr=" << thr
                  << "kbps loss=" << loss << "% delay=" << dly << "ms\n";
        f << names[i] << "," << speed << "," << pw << "," << thr << "," << loss << "," << dly << "\n";
    }
    Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
    double speed = -1; // -1 => run all speeds
    double area = 100.0;
    CommandLine cmd;
    cmd.AddValue("speed", "node speed m/s (omit to run 1, 5 and 10)", speed);
    cmd.AddValue("area", "square side (m)", area);
    cmd.Parse(argc, argv);

    std::vector<double> speeds = {1.0, 5.0, 10.0};
    if (speed > 0)
    {
        speeds = {speed};
    }

    std::remove("results11.csv"); // fresh results file
    for (double v : speeds)
    {
        RunOne(v, area);
    }
    std::cout << "Results written to results11.csv" << std::endl;
    return 0;
}