//
// Copyright (C) OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see http://www.gnu.org/licenses/.
//
//
// This implementation used the "PriorityQueue" as a base!
//

package inet.queueing.queue;

import inet.queueing.contract.IPacketBuffer;
import inet.queueing.contract.IPacketClassifier;
import inet.queueing.contract.IPacketQueue;
import inet.queueing.contract.IPacketScheduler;

module HTBQueue extends CompoundPacketQueue
{
    parameters:
        int numQueues;
        bool htbHysterisis = default(false); // Use hysterisis?
        xml htbTreeConfig = default(xml("<config></config>")); // The htb tree structure
    submodules:
        classifier: <default("HTBClassifier")> like IPacketClassifier {
            parameters:
                htbTreeConfig = default(htbTreeConfig);
                @display("p=100,100");
        }
        queue[numQueues]: <default("PacketQueue")> like IPacketQueue {
            parameters:
                bufferModule = default(exists(buffer) ? "^.buffer" : "");
                @display("p=325,100,column,125");
        }
        scheduler: <default("HTBScheduler")> like IPacketScheduler {
            parameters:
                htbHysterisis = htbHysterisis; // Use hysterisis?
                htbTreeConfig = default(htbTreeConfig);
                @display("p=550,100");
        }
    connections:
        in --> { @display("m=w"); } --> classifier.in;
        for i=0..sizeof(queue)-1 {
            classifier.out++ --> queue[i].in;
            queue[i].out --> scheduler.in++;
        }
        scheduler.out --> { @display("m=e"); } --> out;
}

