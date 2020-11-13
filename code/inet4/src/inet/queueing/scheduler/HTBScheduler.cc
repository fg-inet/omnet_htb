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

#include <stdlib.h>
#include <string.h>
#include "inet/queueing/scheduler/HTBScheduler.h"
#include "inet/common/XMLUtils.h"

namespace inet {
namespace queueing {

Define_Module(HTBScheduler);

//HTBScheduler::htbClass *HTBScheduler::htbInitializeNewClass() {
//    htbClass *newClass = new htbClass();
//    return newClass;
//}

void HTBScheduler::printClass(htbClass *cl) {
    EV << "Class: " << cl->name << endl;
    EV << "   - assigned rate: " << cl->assignedRate << endl;
    EV << "   - ceiling rate: " << cl->ceilingRate << endl;
    EV << "   - burst size: " << cl->burstSize << endl;
    EV << "   - cburst size: " << cl->cburstSize << endl;
    EV << "   - quantum: " << cl->quantum << endl;
    EV << "   - mbuffer: " << cl->mbuffer << endl;
    EV << "   - chackpoint time: " << cl->checkpointTime << endl;
    EV << "   - level: " << cl->level << endl;
    EV << "   - number of children: " << cl->numChildren << endl;
    if (cl->parent != NULL) {
        EV << "   - parent class name: " << cl->parent->name << endl;
    }
    EV << "   - current tokens: " << cl->tokens << endl;
    EV << "   - current ctokens: " << cl->ctokens << endl;
    EV << "   - current class mode: " << cl->mode << endl;

    char actPrios[2*maxHtbNumPrio+1];

    for (int i = 0; i < maxHtbNumPrio; i++) {
        actPrios[2*i] = cl->activePriority[i] ? '1' : '0';
        actPrios[2*i+1] = ';';
    }
    actPrios[2*maxHtbNumPrio] = '\0';

    EV << "   - active priorities: " << actPrios << endl;

    if (strstr(cl->name,"leaf")) {
        EV << "   - leaf priority: " << cl->leaf.priority << endl;
        EV << "   - current queue level: " << cl->leaf.queueLevel << endl;
        EV << "   - queue num: " << cl->leaf.queueId << endl;
    }
}

HTBScheduler::htbClass *HTBScheduler::createAndAddNewClass(cXMLElement* oneClass, int queueId) {
    htbClass* newClass = new htbClass();
    newClass->name = oneClass->getAttribute("id");
    const char* parentName = oneClass->getFirstChildWithTag("parentId")->getNodeValue();

    int rate = atoi(oneClass->getFirstChildWithTag("rate")->getNodeValue());
    newClass->assignedRate = rate;
    int ceil = atoi(oneClass->getFirstChildWithTag("ceil")->getNodeValue());
    newClass->ceilingRate = ceil;
    int burst = atoi(oneClass->getFirstChildWithTag("burst")->getNodeValue());
    newClass->burstSize = burst;
    int cburst = atoi(oneClass->getFirstChildWithTag("cburst")->getNodeValue());
    newClass->cburstSize = cburst;
    int level = atoi(oneClass->getFirstChildWithTag("level")->getNodeValue());
    newClass->level = level;
    int quantum = atoi(oneClass->getFirstChildWithTag("quantum")->getNodeValue());
    newClass->quantum = quantum;
    int mbuffer = atoi(oneClass->getFirstChildWithTag("mbuffer")->getNodeValue());
    newClass->mbuffer = mbuffer;
    newClass->checkpointTime = simTime();
    newClass->tokens = burst;
    newClass->ctokens = cburst;

    if (strstr(newClass->name,"inner")) {
        newClass->parent = rootClass;
        rootClass->numChildren += 1;
        innerClasses.push_back(newClass);
    } else if (strstr(newClass->name,"leaf")) {
        for (auto innerCl : innerClasses) {
            if (!strcmp(innerCl->name, parentName)) {
                newClass->parent = innerCl;
                innerCl->numChildren += 1;
                leafClasses.push_back(newClass);
            }
        }
        memset(newClass->leaf.deficit, 0, sizeof(newClass->leaf.deficit));
        newClass->leaf.queueLevel = 0;
        newClass->leaf.queueId = atoi(oneClass->getFirstChildWithTag("queueNum")->getNodeValue());
        newClass->leaf.priority = atoi(oneClass->getFirstChildWithTag("priority")->getNodeValue());
    } else if (strstr(newClass->name,"root")) {
        newClass->parent = NULL;
        rootClass = newClass;
    } else {
        newClass->parent = NULL;
    }
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
//            classes.push_back(htbInitializeNewClass()); // Initialize the dummy test vector
        }
        htbConfig = par("htbTreeConfig");

        cXMLElementList classes = htbConfig->getChildren();//  ->getChildrenByTagName("root");
        for (auto & oneClass : classes) {
            htbClass* newClass = createAndAddNewClass(oneClass, 0);
            printClass(newClass);
        }
        printClass(rootClass);

        for (int i=0;i<maxHtbDepth; i++){
            levels[i]=new htbLevel();
            levels[i]->levelId = i;
        }



    }
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


void HTBScheduler::activateClass(htbClass *cl, int priority) {
    if (!cl->activePriority[priority]) {
        cl->activePriority[priority] = true;
        htbLevel *lvl = levels[cl->level];
        lvl->selfFeeds[priority].insert(cl);
    }
}

void HTBScheduler::deactivateClass(htbClass *cl, int priority) {
    if (cl->activePriority[priority]) {
        cl->activePriority[priority] = false;
        htbLevel *lvl =  levels[cl->level];
        lvl->selfFeeds[priority].erase(cl);
        lvl->waitingClasses.erase(cl);
    }
}

void HTBScheduler::htbEnqueue(int index, Packet *packet) {
    int packetLen = packet->getByteLength();
    EV_INFO << "HTBScheduler: htbEnqueue " << index << "; Enqueue " << packetLen << " bytes." << endl;
    htbClass *currLeaf = leafClasses.at(index);
    currLeaf->leaf.queueLevel += packetLen;
    activateClass(currLeaf, currLeaf->leaf.priority);
    printClass(currLeaf);
    printLevel(levels[currLeaf->level], currLeaf->level);

//    EV_INFO << "HTBScheduler: Bytes in queue at index " << index << " = " << leafClasses.at(index)->type.leaf.queueLevel << endl;
    return;
}

void HTBScheduler::htbDequeue(int index) {
    Packet *thePacketToPop = providers[index]->canPopPacket();
    htbClass *currLeaf = leafClasses.at(index);
    currLeaf->checkpointTime = SimTime();
    int packetLen = thePacketToPop->getByteLength();
    EV_INFO << "HTBScheduler: htbDequeue " << index << "; Dequeue " << packetLen << " bytes." << endl;
    currLeaf->leaf.queueLevel -= packetLen;

    if (currLeaf->leaf.queueLevel == 0) {
        deactivateClass(currLeaf, currLeaf->leaf.priority);
    }
    printClass(currLeaf);
    printLevel(levels[currLeaf->level], currLeaf->level);
//    printInner(currLeaf);
//    EV_INFO << "HTBScheduler: Bytes in queue at index " << index << " = " << leafClasses.at(index)->type.leaf.queueLevel << endl;
    return;
}

void HTBScheduler::printLevel(htbLevel *level, int index) {
//    EV << "Self feeds for Level:  " << std::distance(std::find(std::begin(levels), std::end(levels), level),levels) << endl;
    EV << "Self feeds for Level:  " << level->levelId << endl;
    for (int i = 0; i < maxHtbNumPrio; i++) {
        std::string printSet = "";
        for (auto & elem : level->selfFeeds[i]){
            printSet.append(elem->name);
            printSet.append(" ;");
        }
        printSet.append(" Next to dequeue: ");
        if (level->nextToDequeue[i] != NULL) {
            printSet.append(level->nextToDequeue[i]->name);
        } else {
            printSet.append("None");
        }
        EV << "   - Self feed for priority " << i  << ": " << printSet << endl;
    }
}

void HTBScheduler::printInner(htbClass *cl) {
    EV << "Inner feeds for Inner Class:  " << cl->name << endl;
    for (int i = 0; i < maxHtbNumPrio; i++) {
        std::string printSet = "";
        for (auto & elem : cl->inner.innerFeeds[i]){
            printSet.append(elem->name);
            printSet.append(" ;");
        }
        printSet.append(" Next to dequeue: ");
        if (cl->inner.nextToDequeue[i] != NULL) {
            printSet.append(cl->inner.nextToDequeue[i]->name);
        } else {
            printSet.append("None");
        }
        EV << "   - Inner feed for priority " << i  << ": " << printSet << endl;
    }
}

void HTBScheduler::updateClassMode(htbClass *cl, simtime_t diff) {
    return;
}

void HTBScheduler::accountTokens(htbClass *cl, int bytes, simtime_t diff) {
    return;
}

void HTBScheduler::accountCTokens(htbClass *cl, int bytes, simtime_t diff) {
    return;
}

void HTBScheduler::chargeClass(htbClass *leafCl, int borrowLevel) {
    return;
}



} // namespace queueing
} // namespace inet





