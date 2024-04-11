#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace std;
using namespace ns3;

// Define the log component
NS_LOG_COMPONENT_DEFINE("Group4_Netsim2024");

// Simulation constants
const uint32_t kGridWidth = 6;
const double kMinX = 0.0;
const double kMinY = 0.0;
const double kDeltaX = 5.0;
const double kDeltaY = 10.0;
const uint32_t kPort = 443;
const uint32_t kPacketSize = 512;
const Time kSimulationStartTime = Seconds(1.0);
const Time kSimulationStopTime = Seconds(25.0);

// Function to configure logging
void ConfigureLogging() {
    // Configure log level for UdpEchoClient and UdpEchoServer applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
}

// Function to configure RTS/CTS threshold
void ConfigureRtsCtsThreshold() {
    // Set RTS/CTS threshold value
    UintegerValue threshold = 1000;
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", threshold);
}

// Function to set up WiFi nodes in ad-hoc mode
void SetupWifiNodes(NodeContainer &nodes, int nWifi) {
    // Create WiFi nodes
    nodes.Create(nWifi);
    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(kMinX),
                                  "MinY", DoubleValue(kMinY),
                                  "DeltaX", DoubleValue(kDeltaX),
                                  "DeltaY", DoubleValue(kDeltaY),
                                  "GridWidth", UintegerValue(kGridWidth),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
}

// Function to set up the WiFi network
void SetupWifiNetwork(NodeContainer &nodes) {
    // Set up WiFi helper
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211p);

    // Set up physical layer
    YansWifiPhyHelper physicalLayer;
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    physicalLayer.SetChannel(channelHelper.Create());

    // Set up MAC layer
    WifiMacHelper macLayer;
    macLayer.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer netDevices = wifiHelper.Install(physicalLayer, macLayer, nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(netDevices);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap on all WiFi devices
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> device = netDevices.Get(i);
        std::string filename = "wifi-node-" + std::to_string(i) + ".pcap";
        physicalLayer.EnablePcap(filename, device);
    }
}

// Function to set up applications
void SetupApplications(NodeContainer &nodes, int nWifi, int interval, int maxPackets) {
    for (int i = 0; i < nWifi; i++) {
        // Set up UdpEchoClient application
        UdpEchoClientHelper clientHelper(nodes.Get((i + 1) % nWifi)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), kPort);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
        clientHelper.SetAttribute("PacketSize", UintegerValue(kPacketSize));
        ApplicationContainer app = clientHelper.Install(nodes.Get(i));
        app.Start(kSimulationStartTime);
        app.Stop(kSimulationStopTime);
    }

    // Set up UdpEchoServer application
    UdpEchoServerHelper serverHelper(kPort);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(nWifi - 1));
    serverApp.Start(kSimulationStartTime);
    serverApp.Stop(kSimulationStopTime);
}

// Function to print flow statistics
void PrintFlowStatistics(Ptr<FlowMonitor> flowMonitor, int nWifi) {
    // Check for lost packets
    flowMonitor->CheckForLostPackets();
    // Get flow statistics
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    Time simulationTime = kSimulationStopTime - kSimulationStartTime;
    for (int i = 2; i <= nWifi; i++) {
        cout << "======= " << "FlowID: " << i << " =======" << endl;
        cout << "TX bitrates: ";
        if (stats[i].txBytes > 0)
            cout << (stats[i].txBytes * 8.0) / (simulationTime.GetSeconds() * 1000.0) << " kbit/s" << endl;
        else
            cout << "None" << endl;

        cout << "RX bitrate: ";
        if (stats[i].rxBytes > 0)
            cout << (stats[i].rxBytes * 8.0) / (simulationTime.GetSeconds() * 1000.0) << " kbit/s" << endl;
        else
            cout << "None" << endl;

        cout << "TX packets: " << stats[i].txPackets << endl;
        cout << "RX packets: " << stats[i].rxPackets << endl;
        cout << "Mean delay: ";
        
        if (stats[i].rxPackets > 0)
            cout << stats[i].delaySum / stats[i].rxPackets << endl;
        else
            cout << "None" << endl;

        double lossRatio = (double)(stats[i].lostPackets) / (double)(stats[i].txPackets) * 100.0;
        cout << "Packet loss ratio: " << lossRatio << "%" << endl;
    }
}

// Function to export animation XML
void ExportAnimation(NodeContainer &nodes, int nWifi) {
    char animfile[100];
    sprintf(animfile, "anim-%d-nodes.xml", nWifi);
    AnimationInterface anim(animfile);

    // Add WiFi nodes to animation
    for (int i = 0; i < nWifi; ++i) {
        anim.SetConstantPosition(nodes.Get(i), i * 10, 0); // Set positions of WiFi nodes in the animation
    }
}

// Function to run the simulation
void RunSimulation(int nWifi, int interval, int maxPackets) {
    // Configure logging and RTS/CTS threshold
    ConfigureLogging();
    ConfigureRtsCtsThreshold();

    // Create node container and set up WiFi nodes and network
    NodeContainer nodes;
    SetupWifiNodes(nodes, nWifi);
    SetupWifiNetwork(nodes);

    // Install flow monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
    
    // Set up applications
    SetupApplications(nodes, nWifi, interval, maxPackets);

    // Stop simulation at specified time
    Simulator::Stop(kSimulationStopTime);
    cout << "Simulation running..." << endl;
    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    stringstream strStream;
    strStream << "flowmonitor-" << nWifi << "-nodes.xml";
    flowMonitor->SerializeToXmlFile(strStream.str(), true, true);

    ExportAnimation(nodes, nWifi);

    PrintFlowStatistics(flowMonitor, nWifi);
}

// Main function
int main(int argc, char* argv[]) {
    // Default simulation parameters
    int maxPackets = 10;
    int interval = 5;

    CommandLine cmd;
    cmd.AddValue("maxPackets", "Max packets to send", maxPackets);
    cmd.AddValue("interval", "Interval between packets in milliseconds", interval);
    cmd.Parse(argc, argv);

    for (int node = 2; node <= 30; node++) {
        RunSimulation(node, interval, maxPackets);
        cout << "Simulation for " << node << " nodes" << endl;
    }
    
    return 0;
}