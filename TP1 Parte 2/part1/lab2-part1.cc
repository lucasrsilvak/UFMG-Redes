#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab2Part1");

static std::map<uint32_t, Ptr<OutputStreamWrapper>> cWndStream;
static std::map<uint32_t, bool> firstCwnd;

static std::pair<uint32_t, uint32_t>
GetIdsFromContext (std::string context)
{
    std::size_t const n1 = context.find_first_of ("/", 1);
    std::size_t const n2 = context.find_first_of ("/", n1 + 1);
    uint32_t nodeId = std::stoul (context.substr (n1 + 1, n2 - n1 - 1));

    std::size_t const s1 = context.find ("SocketList/");
    if (s1 == std::string::npos)
    {
        return {nodeId, 0};
    }
    std::size_t const s2 = context.find ("/", s1 + 11);
    uint32_t socketId = std::stoul (context.substr (s1 + 11, s2 - (s1 + 11)));

    return {nodeId, socketId};
}

static void
CwndTracer (std::string context, uint32_t oldval, uint32_t newval)
{
    std::pair<uint32_t, uint32_t> ids = GetIdsFromContext (context);
    uint32_t mapId = ids.first * 1000 + ids.second;

    if (firstCwnd.find (mapId) == firstCwnd.end ())
    {
        firstCwnd[mapId] = true;
    }

    if (firstCwnd[mapId])
    {
        *cWndStream[mapId]->GetStream () << "0.0 " << oldval << std::endl;
        firstCwnd[mapId] = false;
    }
    *cWndStream[mapId]->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
}

static void
TraceCwnd (std::string cwnd_tr_file_name, uint32_t nodeId, uint32_t socketId)
{
    uint32_t mapId = nodeId * 1000 + socketId;
    AsciiTraceHelper ascii;
    cWndStream[mapId] = ascii.CreateFileStream (cwnd_tr_file_name);
    
    Config::Connect ("/NodeList/" + std::to_string (nodeId) +
                     "/$ns3::TcpL4Protocol/SocketList/" + std::to_string (socketId) +
                     "/CongestionWindow",
                     MakeCallback (&CwndTracer));
}

int
main (int argc, char *argv[])
{  
    std::string dataRate = "1Mbps";
    std::string delay = "20ms";
    double errorRate = 0.00001;
    uint16_t nFlows = 1;
    std::string transport_prot = "TcpNewReno";
    
    bool tracing = false;
    std::string prefix_file_name = "lab2-part1";
    uint32_t mtu_bytes = 1500;
    uint64_t data_mbytes = 0; 
    double duration = 20.0;

    CommandLine cmd (__FILE__);
    cmd.AddValue ("dataRate", "Bottleneck link data rate", dataRate);
    cmd.AddValue ("delay", "Bottleneck link delay", delay);
    cmd.AddValue ("errorRate", "Bottleneck link error rate", errorRate);
    cmd.AddValue ("nFlows", "Number of flows (max 20)", nFlows);
    cmd.AddValue ("transport_prot", "Transport protocol (e.g., TcpCubic, TcpNewReno)", transport_prot);
    cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
    cmd.Parse (argc, argv);

    transport_prot = std::string ("ns3::") + transport_prot;

    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid),
                         "TypeId " << transport_prot << " not found");
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));

    Header *temp_header = new Ipv4Header ();
    uint32_t ip_header = temp_header->GetSerializedSize ();
    delete temp_header;
    temp_header = new TcpHeader ();
    uint32_t tcp_header = temp_header->GetSerializedSize ();
    delete temp_header;
    uint32_t tcp_adu_size = mtu_bytes - (ip_header + tcp_header);

    NS_LOG_INFO ("Create nodes.");
    NodeContainer nodes;
    nodes.Create (4);
    NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
    NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
    NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));

    NS_LOG_INFO ("Create channels.");
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    p2pAccess.SetChannelAttribute ("Delay", StringValue ("0.01ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    p2pBottleneck.SetChannelAttribute ("Delay", StringValue (delay));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (errorRate));

    p2pBottleneck.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (em));

    NetDeviceContainer d0d1 = p2pAccess.Install (n0n1);
    NetDeviceContainer d1d2 = p2pBottleneck.Install (n1n2);
    NetDeviceContainer d2d3 = p2pAccess.Install (n2n3);

    NS_LOG_INFO ("Install internet stack.");
    InternetStackHelper stack;
    stack.Install (nodes);

    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);

    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

    ipv4.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign (d2d3);

    NS_LOG_INFO ("Enable static global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    NS_LOG_INFO ("Create Applications.");
    uint16_t port = 50000;
    double sinkStartTime = 0.0;
    double sourceStartTime = 1.0;
    double simStopTime = duration; 

    Ipv4Address destAddress = i2i3.GetAddress (1);
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port + i));
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
        sinkApp.Start (Seconds (sinkStartTime));
        sinkApp.Stop (Seconds (simStopTime));
        sinkApps.Add (sinkApp);

        Address remoteAddress (InetSocketAddress (destAddress, port + i));
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcp_adu_size));
        
        BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", Address ());
        sourceHelper.SetAttribute ("Remote", AddressValue (remoteAddress));
        sourceHelper.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
        sourceHelper.SetAttribute ("MaxBytes", UintegerValue (data_mbytes));

        ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (0));
        sourceApp.Start (Seconds (sourceStartTime));
        sourceApp.Stop (Seconds (simStopTime));
    }

    if (tracing)
    {
        NS_LOG_INFO ("Enable CWND Tracing.");
        for (uint32_t i = 0; i < nFlows; ++i)
        {
            std::string flowString = "-flow" + std::to_string (i);
            std::string traceFile = prefix_file_name + flowString + "-cwnd.data";
            
            Simulator::Schedule (Seconds (sourceStartTime + 0.00001),
                                 &TraceCwnd,
                                 traceFile,
                                 nodes.Get (0)->GetId(),
                                 i);
        }
    }

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (Seconds (simStopTime));
    Simulator::Run ();
    NS_LOG_INFO ("Simulation Done.");
    
    double activeTime = simStopTime - sourceStartTime;
    uint64_t totalRx = 0;

    std::cout << std::endl
              << "------ Lab 2 Part 1 Goodput (" << transport_prot << ") ------" << std::endl;
    for (uint32_t i = 0; i < nFlows; ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (i));
        uint64_t bytesReceived = sink->GetTotalRx ();
        totalRx += bytesReceived;

        double goodput_bps = (bytesReceived * 8.0) / activeTime;
        std::cout << "Flow " << i << " (N0->N3): " << bytesReceived << " bytes received, "
                  << "Goodput: " << goodput_bps << " bps" << std::endl;
    }
    double avgGoodput_bps = (totalRx * 8.0) / (nFlows * activeTime);
    std::cout << "Average Flow Goodput: " << avgGoodput_bps << " bps" << std::endl;
    std::cout << "----------------------------------------------------" << std::endl;

    Simulator::Destroy ();
    return 0;
}