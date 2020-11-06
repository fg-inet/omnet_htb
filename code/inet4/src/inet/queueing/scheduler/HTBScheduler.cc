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

#include "inet/queueing/scheduler/HTBScheduler.h"

namespace inet {
namespace queueing {

Define_Module(HTBScheduler);

HTBScheduler::htbClass *HTBScheduler::htbInitializeNewClass() {
    htbClass *newClass = new htbClass();
    return newClass;
}

void HTBScheduler::initialize(int stage)
{
    // This was here from the priority scheduler and I think is still needed.
    // It gets references to our leaf queues into the scheduler
    PacketSchedulerBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        for (auto provider : providers) {
            collections.push_back(dynamic_cast<IPacketCollection *>(provider)); // Get pointers to queues
            classes.push_back(htbInitializeNewClass()); // Initialize the dummy test vector
        }
    }
    // TODO: Figure out in which initialization stage we should initialize the following stuff :)
    // TODO: Initialize and prepare the HTB structure
    // TODO: Read in the XML with tree configuration
    // TODO: Initialize the class tree here


}

// Should be fine as is.
int HTBScheduler::getNumPackets() const
{
    int size = 0;
    for (auto collection : collections)
        size += collection->getNumPackets();
    return size;
}

// Should be fine as is.
b HTBScheduler::getTotalLength() const
{
    b totalLength(0);
    for (auto collection : collections)
        totalLength += collection->getTotalLength();
    return totalLength;
}

// Should be fine as is??
Packet *HTBScheduler::getPacket(int index) const
{
    int origIndex = index;
    for (auto collection : collections) {
        auto numPackets = collection->getNumPackets();
        if (index < numPackets)
            return collection->getPacket(index);
        else
            index -= numPackets;
    }
    throw cRuntimeError("Index %i out of range", origIndex);
}

// TODO: We need to care for correct class management!!!
void HTBScheduler::removePacket(Packet *packet)
{
    Enter_Method("removePacket");
    for (auto collection : collections) {
        int numPackets = collection->getNumPackets();
        for (int j = 0; j < numPackets; j++) {
            if (collection->getPacket(j) == packet) {
                collection->removePacket(packet);
                return;
            }
        }
    }
    throw cRuntimeError("Cannot find packet");
}

// TODO: This is where the HTB scheduling comes into play
int HTBScheduler::schedulePacket()
{
    for (int i = 0; i < (int)providers.size(); i++) {
        if (providers[i]->canPopSomePacket(inputGates[i]->getPathStartGate())) {
            htbDequeue(i);
            return i;
        }
    }
    return -1;
}

void HTBScheduler::printTest() {
    EV_INFO << "Print test was successful!!" << endl;
    return;
}

void HTBScheduler::htbEnqueue(int index, Packet *packet) {
    int packetLen = packet->getByteLength();
    classes.at(index)->queueLevel += packetLen;
    EV_INFO << "HTBScheduler: Bytes in queue at index " << index << " = " << classes.at(index)->queueLevel << endl;
    return;
}

void HTBScheduler::htbDequeue(int index) {
    Packet *thePacketToPop = providers[index]->canPopPacket();
    classes.at(index)->checkpointTime = SimTime();
    int packetLen = thePacketToPop->getByteLength();
    classes.at(index)->queueLevel -= packetLen;
    EV_INFO << "HTBScheduler: Bytes in queue at index " << index << " = " << classes.at(index)->queueLevel << endl;
    return;
}

} // namespace queueing
} // namespace inet

