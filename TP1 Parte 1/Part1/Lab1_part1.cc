#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab1Part1Script");

int
main(int argc, char* argv[])
{
    uint32_t nClients = 5;
    uint32_t nPackets = 4;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.AddValue("nPackets", "Number of packets per client", nPackets);
    cmd.Parse(argc, argv);

    if (nClients > 5) {
        std::cout << "Error: Maximum number of clients is 5." << std::endl;
        return 1;
    }
    if (nPackets > 5) {
        std::cout << "Error: Maximum number of packets per client is 5." << std::endl;
        return 1;
    }

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(nClients);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(clientNodes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> clientInterfaces;

    for (uint32_t i = 0; i < nClients; ++i)
    {
        NodeContainer linkNodes(clientNodes.Get(i), serverNode.Get(0)); 
        NetDeviceContainer linkDevices = pointToPoint.Install(linkNodes);

        std::string baseIp = "10.1." + std::to_string(i + 1) + ".0";
        address.SetBase(Ipv4Address(baseIp.c_str()), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(linkDevices);

        clientInterfaces.push_back(interfaces);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(15);
    ApplicationContainer serverApps = echoServer.Install(serverNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    Ipv4Address serverIp = clientInterfaces[0].GetAddress(1); 

    for (uint32_t i = 0; i < nClients; ++i)
    {
        UdpEchoClientHelper echoClient(serverIp, 15);
        echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(clientNodes.Get(i));

        Ptr<UniformRandomVariable> startTime = CreateObject<UniformRandomVariable>();
        startTime->SetAttribute("Min", DoubleValue(2.0));
        startTime->SetAttribute("Max", DoubleValue(7.0));

        clientApps.Start(Seconds(startTime->GetValue()));
        clientApps.Stop(Seconds(20.0));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}