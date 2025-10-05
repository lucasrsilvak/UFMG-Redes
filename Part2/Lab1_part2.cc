#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab1Part2");

int main (int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nCsma = 3;
    uint32_t nPackets = 1;

    CommandLine cmd;
    cmd.AddValue ("nCsma", "Number of extra CSMA nodes", nCsma);
    cmd.AddValue ("nPackets", "Number of packets sent by the client", nPackets);
    cmd.AddValue ("verbose", "Enable echo application logs", verbose);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    nCsma = nCsma == 0 ? 1 : nCsma;

    NodeContainer p2pNodes;
    p2pNodes.Create (2);

    NodeContainer csmaNodes;
    csmaNodes.Add (p2pNodes.Get (1));
    csmaNodes.Create (nCsma);

    NodeContainer serverP2P;
    serverP2P.Add (csmaNodes.Get (nCsma));
    serverP2P.Create (1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices = p2p.Install (p2pNodes);
    NetDeviceContainer p2pServerDevices = p2p.Install (serverP2P);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

    InternetStackHelper stack;
    stack.Install (p2pNodes.Get (0));
    stack.Install (csmaNodes);
    stack.Install (serverP2P.Get (1));

    Ipv4AddressHelper address;

    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = address.Assign (p2pDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pServerInterfaces = address.Assign (p2pServerDevices);

    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (serverP2P.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (2.0 + nPackets * 1.0 + 2.0));

    UdpEchoClientHelper echoClient (p2pServerInterfaces.GetAddress (1), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (p2pNodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (2.0 + nPackets * 1.0 + 2.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    p2p.EnablePcapAll ("lab1-part2-p2p");
    csma.EnablePcap ("lab1-part2-csma", csmaDevices.Get (1), true);
    p2p.EnablePcap ("lab1-part2-server-p2p", p2pServerDevices.Get (0), true);

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}