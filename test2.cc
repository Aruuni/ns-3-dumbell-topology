#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;
using namespace std;


static void
CwndTracer(Ptr<OutputStreamWrapper> stream, string cca,  uint32_t oldval, uint32_t newval)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ", " << newval / 1448.0 << std::endl;
    cout << Simulator::Now().GetSeconds() << "  |  " << newval / 1448.0 <<  " | "<< cca <<endl;
}

static void
RTTTracer(Ptr<OutputStreamWrapper> stream, string cca,  Time oldval, Time newval)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << ", " << newval.GetMilliSeconds()  << std::endl;
    cout << Simulator::Now().GetSeconds() << "  |  " << newval.GetMilliSeconds() <<  " | "<< cca <<endl;
}

uint32_t prevBytes = 0;
Time prevTime = Seconds(0);

static void
TraceThroughput(Ptr<FlowMonitor> monitor,  string cca)
{
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    auto itr = stats.begin();
    Time curTime = Now();
    std::ofstream thr( cca + "-throughput.dat", std::ios::out | std::ios::app);
    thr << curTime << ", "
        << 8 * (itr->second.txBytes - prevBytes) /
               (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds()))
        << std::endl;
    cout << curTime << ", "
        << 8 * (itr->second.txBytes - prevBytes) /
               (1000 * 1000 * (curTime.GetSeconds() - prevTime.GetSeconds()))
        << std::endl;
    prevTime = curTime;
    prevBytes = itr->second.txBytes;
    Simulator::Schedule(Seconds(0.1), &TraceThroughput, monitor, cca);
}



void
TraceCwnd(uint32_t nodeId, string caa)
{
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(caa + "-cwnd.dat");
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) + 
                                  "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", 
                                  MakeBoundCallback(&CwndTracer, stream, caa));
}
void
TraceRTT(uint32_t nodeId, string caa)
{
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(caa + "-rtt.dat");
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(nodeId) + 
                                  "/$ns3::TcpL4Protocol/SocketList/0/RTT", 
                                  MakeBoundCallback(&RTTTracer, stream, caa));
}


int
main(int argc, char* argv[])
{
    std::string cca[] = { "TcpBbr" };
    //, "TcpVegas"
    int PORT = 50001;
    Time stopTime = Seconds(1);
    uint packetSize = 1448;
    int Nodes = sizeof(cca) / sizeof(cca[0]);
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(6291456));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    
    
    //Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    //Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    //Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("1p")));
    //Config::SetDefault("ns3::FifoQueueDisc::MaxSize", QueueSizeValue(QueueSize("900p")));
    
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    
    
    NodeContainer senders, receivers, routers;
    senders.Create(Nodes);
    receivers.Create(Nodes);
    routers.Create(2);


    PointToPointHelper botLink, p2pLink;
    std::ostringstream packetSizeValuebotLink;
    packetSizeValuebotLink << (100000 * 5 / packetSize) << "p";
    botLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    botLink.SetChannelAttribute("Delay", StringValue("5ms"));
    botLink.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize(packetSizeValuebotLink.str())));

    std::ostringstream packetSizeValuep2pLink;
    packetSizeValuep2pLink << (1000000 * 10 / packetSize) << "p";
    p2pLink.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2pLink.SetChannelAttribute("Delay", StringValue("10ms"));
    botLink.SetQueue("ns3::DropTailQueue", "MaxSize",  QueueSizeValue(QueueSize(packetSizeValuep2pLink.str())));
    
    NetDeviceContainer routerDevices = botLink.Install(routers);
    NetDeviceContainer senderDevices, receiverDevices, leftRouterDevices, rightRouterDevices;
    
    for(int i = 0; i < Nodes; ++i) {
		NetDeviceContainer cleft = p2pLink.Install(routers.Get(0), senders.Get(i));
		leftRouterDevices.Add(cleft.Get(0));
		senderDevices.Add(cleft.Get(1));
		//cleft.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

		NetDeviceContainer cright = p2pLink.Install(routers.Get(1), receivers.Get(i));
		rightRouterDevices.Add(cright.Get(0));
		receiverDevices.Add(cright.Get(1));
		//cright.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
	}
    
    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(receivers);
    internet.Install(routers);


    //("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));

    // sets the congestion control algorithm for each node to the corresponding value in the array 
    for (int i = 0; i < Nodes; i++) {
        Config::Set("/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::" + cca[i])));
    }

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FifoQueueDisc");
    tch.SetQueueLimits("ns3::DynamicQueueLimits", "HoldTime", StringValue("10ms"));
    tch.Install(senderDevices);
    tch.Install(receiverDevices);
    tch.Install(leftRouterDevices);
    tch.Install(rightRouterDevices);


	Ipv4AddressHelper routerIP = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");
	Ipv4AddressHelper senderIP = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
	Ipv4AddressHelper receiverIP = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");

    Ipv4InterfaceContainer routerIFC, senderIFCs, receiverIFCs, leftRouterIFCs, rightRouterIFCs;
    routerIFC = routerIP.Assign(routerDevices); 

    for(int i = 0; i < Nodes; ++i) {
		NetDeviceContainer senderDevice;
		senderDevice.Add(senderDevices.Get(i));
		senderDevice.Add(leftRouterDevices.Get(i));
		
        Ipv4InterfaceContainer senderIFC = senderIP.Assign(senderDevice);
		senderIFCs.Add(senderIFC.Get(0));
		leftRouterIFCs.Add(senderIFC.Get(1));
		
        senderIP.NewNetwork();

		NetDeviceContainer receiverDevice;
		receiverDevice.Add(receiverDevices.Get(i));
		receiverDevice.Add(rightRouterDevices.Get(i));
		
        Ipv4InterfaceContainer receiverIFC = receiverIP.Assign(receiverDevice);
		receiverIFCs.Add(receiverIFC.Get(0));
		rightRouterIFCs.Add(receiverIFC.Get(1));
		receiverIP.NewNetwork();
	}
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ApplicationContainer senderApp, receiverApp;
    for (uint32_t i = 0; i < senders.GetN(); i++) {
        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(receiverIFCs.GetAddress(i), PORT));
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
        senderApp.Add(sender.Install(senders.Get(i)));
        Simulator::Schedule(Seconds(0.1) + MilliSeconds(1), &TraceCwnd,  senders.Get(i)->GetId(), cca[i]);
        Simulator::Schedule(Seconds(0.1) + MilliSeconds(1), &TraceRTT,  senders.Get(i)->GetId(), cca[i]);

    }

    senderApp.Start(Seconds(0.0));

    senderApp.Stop(stopTime);

    // Create receiver applications on each receiver node
    for (uint32_t i = 0; i < receivers.GetN(); i++) {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), PORT));
        receiverApp.Add(sink.Install(receivers.Get(i)));
    }
    receiverApp.Start(Seconds(0.0));
    receiverApp.Stop(stopTime);
    
    for (int i = 0; i < Nodes; i++) {
        FlowMonitorHelper flowmon;
        Simulator::Schedule(Seconds(0.1) + MilliSeconds(1), &TraceThroughput, flowmon.Install(senderApp.Get(i)->GetNode()), cca[i]);
    }
    
    
    
    
    Simulator::Stop(stopTime + TimeStep(1));
    Simulator::Run();
    Simulator::Destroy();

    exit(0);
}