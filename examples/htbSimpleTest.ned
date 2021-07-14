//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package _5gns.simulations;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import inet.visualizer.integrated.IntegratedVisualizer;
import inet.visualizer.linklayer.InterfaceTableVisualizer;
import ned.DatarateChannel;
import inet.node.internetcloud.InternetCloud;
//import inet.applications.generic.linkAdjuster;

network htbSimpleTest
{
    parameters:
        @display("bgb=710,417");
        int nVID = default(1); // Number of video clients
        int nLVD = default(1); // Number of live video clients
        int nFDO = default(1); // Number of file download clients
        int nSSH = default(1); // Number of SSH clients
        int nVIP = default(1); // Number of VoIP clients
    types:
        channel C extends DatarateChannel
        {
        }
    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=10,10;is=s");
        }
//        adjustmentModule: linkAdjuster {
//            @display("p=30,120;is=s");
//        };
        hostVID[nVID]: StandardHost;
        hostLVD[nLVD]: StandardHost;
        hostFDO[nFDO]: StandardHost;
        hostSSH[nSSH]: StandardHost;
        hostVIP[nVIP]: StandardHost;
        router0: Router {
            @display("p=233,203");
        }
        router1: Router {
            @display("p=438,203");
        }
        visualizer: IntegratedVisualizer {
            @display("p=18,71");
        }
        serverVID: StandardHost {
            @display("p=560,53");
        }
        serverLVD: StandardHost {
            @display("p=560,53");
        }
        serverFDO: StandardHost {
            @display("p=560,153");
        }
        serverSSH: StandardHost {
            @display("p=560,253");
        }
        serverVIP: StandardHost {
            @display("p=560,353");
        }
    connections:
//        router0.pppg++ <--> connFIX0: C { datarate = 100Mbps; delay = 1ms; } <--> router1.pppg++;
		router0.pppg++ <--> connFIX0: C { delay = 1ms; } <--> router1.pppg++;

        for i=0..nVID-1 {
            hostVID[i].pppg++ <--> C {  datarate = 1Gbps; delay = 100ns; } <--> router0.pppg++;
        }
        for i=0..nLVD-1 {
            hostLVD[i].pppg++ <--> C {  datarate = 1Gbps; delay = 100ns; } <--> router0.pppg++;
        }
        for i=0..nFDO-1 {
            hostFDO[i].pppg++ <--> C {  datarate = 1Gbps; delay = 100ns; } <--> router0.pppg++;
        }
        for i=0..nSSH-1 {
            hostSSH[i].pppg++ <--> C {  datarate = 1Gbps; delay = 100ns; } <--> router0.pppg++;
        }
        for i=0..nVIP-1 {
            hostVIP[i].pppg++ <--> C {  datarate = 1Gbps; delay = 100ns; } <--> router0.pppg++;
        }
        serverVID.pppg++ <--> C {  datarate = 10Gbps; delay = 100ns; } <--> router1.pppg++;
        serverLVD.pppg++ <--> C {  datarate = 10Gbps; delay = 100ns; } <--> router1.pppg++;
        serverFDO.pppg++ <--> C {  datarate = 10Gbps; delay = 100ns; } <--> router1.pppg++;
        serverSSH.pppg++ <--> C {  datarate = 10Gbps; delay = 100ns; } <--> router1.pppg++;
        serverVIP.pppg++ <--> C {  datarate = 10Gbps; delay = 100ns; } <--> router1.pppg++;
}
