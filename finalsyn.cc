#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wimax-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipcs-classifier-record.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/service-flow.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WimaxSimpleExample");

int main(int argc, char *argv[])
{
    bool verbose = false;

    int duration = 15, schedType = 0;
    WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;

    CommandLine cmd;
    cmd.AddValue("scheduler", "type of scheduler to use with the network devices", schedType);
    cmd.AddValue("duration", "duration of the simulation in seconds", duration);
    cmd.AddValue("verbose", "turn on all WimaxNetDevice log components", verbose);
    cmd.Parse(argc, argv);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    switch (schedType)
    {
    case 0:
        scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;
        break;
    case 1:
        scheduler = WimaxHelper::SCHED_TYPE_MBQOS;
        break;
    case 2:
        scheduler = WimaxHelper::SCHED_TYPE_RTPS;
        break;
    default:
        scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;
    }

    NodeContainer ssNodes;
    NodeContainer bsNodes;

    ssNodes.Create(2);
    bsNodes.Create(1);

    WimaxHelper wimax;

    NetDeviceContainer ssDevs, bsDevs;

    ssDevs = wimax.Install(ssNodes,
                           WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,
                           WimaxHelper::SIMPLE_PHY_TYPE_OFDM,
                           scheduler);
    bsDevs = wimax.Install(bsNodes, WimaxHelper::DEVICE_TYPE_BASE_STATION, WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);

    wimax.EnableAscii("bs-devices", bsDevs);
    wimax.EnableAscii("ss-devices", ssDevs);

    Ptr<SubscriberStationNetDevice> ss[2];

    for (int i = 0; i < 2; i++)
    {
        ss[i] = ssDevs.Get(i)->GetObject<SubscriberStationNetDevice>();
        ss[i]->SetModulationType(WimaxPhy::MODULATION_TYPE_QAM16_12);
    }

    Ptr<BaseStationNetDevice> bs;

    bs = bsDevs.Get(0)->GetObject<BaseStationNetDevice>();

    InternetStackHelper stack;
    stack.Install(bsNodes);
    stack.Install(ssNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer SSinterfaces = address.Assign(ssDevs);
    Ipv4InterfaceContainer BSinterface = address.Assign(bsDevs);

    if (verbose)
    {
        wimax.EnableLogComponents();
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.Install(ssNodes);

    mobility.Install(bsNodes);

    UdpServerHelper udpServer;
    ApplicationContainer serverApps;
    UdpClientHelper udpClient;
    ApplicationContainer clientApps;

    udpServer = UdpServerHelper(100);

    serverApps = udpServer.Install(bsNodes.Get(0));
    serverApps.Start(Seconds(2));
    serverApps.Stop(Seconds(duration));

    udpClient = UdpClientHelper(BSinterface.GetAddress(0), 100);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1200));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps = udpClient.Install(ssNodes.Get(0));
    clientApps.Start(Seconds(2));
    clientApps.Stop(Seconds(duration));

    PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 300)));
    ApplicationContainer app = sink.Install(bsNodes.Get(0));

    app.Start(Seconds(1.0));
    app.Stop(Seconds(duration));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.3"), 300)));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    onOffHelper.SetAttribute("DataRate", StringValue("1Kbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    app = onOffHelper.Install(ssNodes.Get(1));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(duration));

    AnimationInterface anim("mobility_new.xml");

    anim.UpdateNodeDescription(ssNodes.Get(0), "udp_node1");
    anim.UpdateNodeDescription(ssNodes.Get(1), "tcp_node2");

    anim.UpdateNodeDescription(bsNodes.Get(0), "base_station");

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(duration + 0.1));

    wimax.EnablePcap("wimax-simple-ss0", ssNodes.Get(0)->GetId(), ss[0]->GetIfIndex());
    wimax.EnablePcap("wimax-simple-ss1", ssNodes.Get(1)->GetId(), ss[1]->GetIfIndex());
    wimax.EnablePcap("wimax-simple-bs0", bsNodes.Get(0)->GetId(), bs->GetIfIndex());

    IpcsClassifierRecord DlClassifierUgs(Ipv4Address("0.0.0.0"),
                                         Ipv4Mask("0.0.0.0"),
                                         SSinterfaces.GetAddress(0),
                                         Ipv4Mask("255.255.255.255"),
                                         0,
                                         65000,
                                         100,
                                         100,
                                         17,
                                         1);
    ServiceFlow DlServiceFlowUgs = wimax.CreateServiceFlow(ServiceFlow::SF_DIRECTION_DOWN,
                                                           ServiceFlow::SF_TYPE_RTPS,
                                                           DlClassifierUgs);

    IpcsClassifierRecord UlClassifierUgs(SSinterfaces.GetAddress(1),
                                         Ipv4Mask("255.255.255.255"),
                                         Ipv4Address("0.0.0.0"),
                                         Ipv4Mask("0.0.0.0"),
                                         0,
                                         65000,
                                         100,
                                         100,
                                         17,
                                         1);
    ServiceFlow UlServiceFlowUgs = wimax.CreateServiceFlow(ServiceFlow::SF_DIRECTION_UP,
                                                           ServiceFlow::SF_TYPE_RTPS,
                                                           UlClassifierUgs);
    ss[0]->AddServiceFlow(UlServiceFlowUgs);
    ss[1]->AddServiceFlow(UlServiceFlowUgs);

    NS_LOG_INFO("Starting simulation.....");
    Simulator::Run();

    ss[0] = 0;
    ss[1] = 0;
    bs = 0;
    flowmon->SerializeToXmlFile("mobility_new-flow_monitor.flowmon", true, true);
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
