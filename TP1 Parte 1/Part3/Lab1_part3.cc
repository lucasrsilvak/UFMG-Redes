#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab1Part3");

int main (int argc, char *argv[])
{
    uint32_t nWifi = 4;
    uint32_t nPackets = 10;
    bool verbose = true;
    
    CommandLine cmd;
    cmd.AddValue ("nWifi", "Number of wifi STA nodes per network", nWifi);
    cmd.AddValue ("nPackets", "Number of packets to send", nPackets);
    cmd.AddValue ("verbose", "Enable logging", verbose);
    cmd.Parse (argc,argv);

    if (nWifi > 9) nWifi = 9;
    if (nPackets > 20) nPackets = 20;

    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer p2pNodes;
    p2pNodes.Create (2);

    NodeContainer wifiStaNodes1, wifiStaNodes2;
    wifiStaNodes1.Create (nWifi);
    wifiStaNodes2.Create (nWifi);

    NodeContainer wifiApNode1 = p2pNodes.Get (0);
    NodeContainer wifiApNode2 = p2pNodes.Get (1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
    NetDeviceContainer p2pDevices = pointToPoint.Install (p2pNodes);

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy1;
    phy1.SetChannel (channel1.Create ());

    WifiHelper wifi1;
    wifi1.SetStandard (WIFI_STANDARD_80211g);
    wifi1.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac1;
    Ssid ssid1 = Ssid ("ns-3-wifi-1");
    mac1.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices1 = wifi1.Install (phy1, mac1, wifiStaNodes1);
    mac1.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
    NetDeviceContainer apDevices1 = wifi1.Install (phy1, mac1, wifiApNode1);

    YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy2;
    phy2.SetChannel (channel2.Create ());

    WifiHelper wifi2;
    wifi2.SetStandard (WIFI_STANDARD_80211g);
    wifi2.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac2;
    Ssid ssid2 = Ssid ("ns-3-wifi-2");
    mac2.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices2 = wifi2.Install (phy2, mac2, wifiStaNodes2);
    mac2.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
    NetDeviceContainer apDevices2 = wifi2.Install (phy2, mac2, wifiApNode2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (0.0), "MinY", DoubleValue (0.0),
                                    "DeltaX", DoubleValue (5.0), "DeltaY", DoubleValue (10.0),
                                    "GridWidth", UintegerValue (3), "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiStaNodes1);
    mobility.Install (wifiStaNodes2);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (p2pNodes);

    InternetStackHelper stack;
    stack.Install (p2pNodes);
    stack.Install (wifiStaNodes1);
    stack.Install (wifiStaNodes2);

    Ipv4AddressHelper address;

    address.SetBase ("10.1.1.0", "255.255.255.0");
    address.Assign (p2pDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    NetDeviceContainer wifi1_devices;
    wifi1_devices.Add(staDevices1);
    wifi1_devices.Add(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(wifi1_devices);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    NetDeviceContainer wifi2_devices;
    wifi2_devices.Add(staDevices2);
    wifi2_devices.Add(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(wifi2_devices);
    
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (wifiStaNodes1.Get (nWifi - 1));
    
    Ipv4Address serverAddress = staInterfaces1.GetAddress (nWifi - 1);
    UdpEchoClientHelper echoClient (serverAddress, 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (wifiStaNodes2.Get (nWifi - 1));
    
    double simulationTime = 2.0 + (nPackets * 1.0) + 10.0;
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (Seconds (simulationTime));

    pointToPoint.EnablePcapAll ("lab1-part3");
    phy1.EnablePcap ("lab1-part3", apDevices1.Get (0));
    phy2.EnablePcap ("lab1-part3", apDevices2.Get (0));

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}