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
#include "inet/queueing/scheduler/HTBScheduler.h"
#include <string.h>
#include "inet/common/XMLUtils.h"

#include "inet/linklayer/ppp/Ppp.h"

namespace inet {
namespace queueing {

Define_Module(HTBScheduler);

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
        EV << "   - current queue level: " << collections[cl->leaf.queueId]->getNumPackets() << endl;
        EV << "   - queue num: " << cl->leaf.queueId << endl;
    }
}

// Gets information from XML config, creates a class (leaf/inner/root) and correctly adds it to tree structure
HTBScheduler::htbClass *HTBScheduler::createAndAddNewClass(cXMLElement* oneClass, int queueId) {
    // Create class, set its name and parents' name
    htbClass* newClass = new htbClass();
    newClass->name = oneClass->getAttribute("id");
    const char* parentName = oneClass->getFirstChildWithTag("parentId")->getNodeValue();

    // Configure class settings - rate, ceil, burst, cburst, quantum, etc.
    long long rate = atoi(oneClass->getFirstChildWithTag("rate")->getNodeValue())*1e3;
    newClass->assignedRate = rate;
    long long ceil = atoi(oneClass->getFirstChildWithTag("ceil")->getNodeValue())*1e3;
    newClass->ceilingRate = ceil;
    long long burstTemp = atoi(oneClass->getFirstChildWithTag("burst")->getNodeValue());
    long long burst = burstTemp*8*1e+9/linkDatarate;
    newClass->burstSize = burst;
    long long cburstTemp = atoi(oneClass->getFirstChildWithTag("cburst")->getNodeValue());
    long long cburst = cburstTemp*8*1e+9/linkDatarate;
    newClass->cburstSize = cburst;
    int level = atoi(oneClass->getFirstChildWithTag("level")->getNodeValue()); // Level in the tree structure. 0 = LEAF!!!
    newClass->level = level;
    int quantum = atoi(oneClass->getFirstChildWithTag("quantum")->getNodeValue());
    newClass->quantum = quantum;
    long long mbuffer = atoi(oneClass->getFirstChildWithTag("mbuffer")->getNodeValue())*1e9;
    newClass->mbuffer = mbuffer;
    newClass->checkpointTime = simTime(); // Initialize to now. It says when was the last time the tokens were updated.
    newClass->tokens = burst;
    newClass->ctokens = cburst;

    // Handle different types of classes:
    if (strstr(newClass->name,"inner")) { // INNER
        if (strstr(parentName,"root")) {
            newClass->parent = rootClass;
            rootClass->numChildren += 1;
            innerClasses.push_back(newClass);
        } else if (strstr(parentName,"inner")) {
            for (auto innerCl : innerClasses) {
                if (!strcmp(innerCl->name, parentName)) {
                    newClass->parent = innerCl;
                    innerCl->numChildren += 1;
                    innerClasses.push_back(newClass);
                }
            }
        }
    } else if (strstr(newClass->name,"leaf")) { // LEAF
        if (strstr(parentName,"root")) {
            newClass->parent = rootClass;
            rootClass->numChildren += 1;
            leafClasses.push_back(newClass);
        } else if (strstr(parentName,"inner")) {
            for (auto innerCl : innerClasses) {
                if (!strcmp(innerCl->name, parentName)) {
                    newClass->parent = innerCl;
                    innerCl->numChildren += 1;
                    leafClasses.push_back(newClass);
                }
            }
        }
        memset(newClass->leaf.deficit, 0, sizeof(newClass->leaf.deficit));
//        newClass->leaf.queueLevel = 0;
        newClass->leaf.queueId = atoi(oneClass->getFirstChildWithTag("queueNum")->getNodeValue());
        newClass->leaf.priority = atoi(oneClass->getFirstChildWithTag("priority")->getNodeValue());
        // Statistics collection for queue levels
//        char queueLevelSignalName[50];
//        sprintf(queueLevelSignalName, "class-%s-queueLevel", newClass->name);
//        newClass->leaf.queueLvl = registerSignal(queueLevelSignalName);
//        char queueLevelStatisticName[50];
//        sprintf(queueLevelStatisticName, "class-%s-queueLevel", newClass->name);
//        cProperty *queueLevelStatisticTemplate = getProperties()->get("statisticTemplate", "queueLevel");
//        getEnvir()->addResultRecorders(this, newClass->leaf.queueLvl, queueLevelStatisticName, queueLevelStatisticTemplate);
//        emit(newClass->leaf.queueLvl, newClass->leaf.queueLevel);

    } else if (strstr(newClass->name,"root")) { // ROOT
        newClass->parent = NULL;
        rootClass = newClass;
    } else {
        newClass->parent = NULL;
    }

    // Statistics collection is created dynamically for each class:
    // Statistics collection for token levels
    char tokenLevelSignalName[50];
    sprintf(tokenLevelSignalName, "class-%s-tokenLevel", newClass->name);
    newClass->tokenBucket = registerSignal(tokenLevelSignalName);
    char tokenLevelStatisticName[50];
    sprintf(tokenLevelStatisticName, "class-%s-tokenLevel", newClass->name);
    cProperty *tokenLevelStatisticTemplate = getProperties()->get("statisticTemplate", "tokenLevel");
    getEnvir()->addResultRecorders(this, newClass->tokenBucket, tokenLevelStatisticName, tokenLevelStatisticTemplate);
    emit(newClass->tokenBucket, newClass->tokens);

    // Statistics collection for ctoken levels
    char ctokenLevelSignalName[50];
    sprintf(ctokenLevelSignalName, "class-%s-ctokenLevel", newClass->name);
    newClass->ctokenBucket = registerSignal(ctokenLevelSignalName);
    char ctokenLevelStatisticName[50];
    sprintf(ctokenLevelStatisticName, "class-%s-ctokenLevel", newClass->name);
    cProperty *ctokenLevelStatisticTemplate = getProperties()->get("statisticTemplate", "ctokenLevel");
    getEnvir()->addResultRecorders(this, newClass->ctokenBucket, ctokenLevelStatisticName, ctokenLevelStatisticTemplate);
    emit(newClass->ctokenBucket, newClass->ctokens);

    // Statistics collection for class mode
    char classModeSignalName[50];
    sprintf(classModeSignalName, "class-%s-mode", newClass->name);
    newClass->classMode = registerSignal(classModeSignalName);
    char classModeStatisticName[50];
    sprintf(classModeStatisticName, "class-%s-mode", newClass->name);
    cProperty *classModeStatisticTemplate = getProperties()->get("statisticTemplate", "classMode");
    getEnvir()->addResultRecorders(this, newClass->classMode, classModeStatisticName, classModeStatisticTemplate);
    emit(newClass->classMode, newClass->mode);

    return newClass;
}





void HTBScheduler::initialize(int stage)
{
    PacketSchedulerBase::initialize(stage); // Initialize the packet scheduler module
    if (stage == INITSTAGE_LOCAL) {
        // Get the datarate of the link connected to interface
        EV << "Get link datarate" << endl;
        int interfaceIndex = getParentModule()->getParentModule()->getParentModule()->getIndex();
        linkDatarate = getParentModule()->getParentModule()->getParentModule()->getParentModule()->gateByOrdinal(interfaceIndex)->getPreviousGate()->getChannel()->getNominalDatarate();
        EV << "SchedInit: Link datarate = " << linkDatarate << endl;

        // Get all leaf queues. IMPORTANT: Leaf queue id MUST correspond to leaf class id!!!!!
        for (auto provider : providers) {
            collections.push_back(dynamic_cast<IPacketCollection *>(provider)); // Get pointers to queues
        }

        // Load configs
        htbConfig = par("htbTreeConfig");
        htb_hysteresis = par("htbHysterisis");

        // Create all classes and build the tree structure
        cXMLElementList classes = htbConfig->getChildren();
        for (auto & oneClass : classes) {
            htbClass* newClass = createAndAddNewClass(oneClass, 0);
            printClass(newClass);
        }
        printClass(rootClass);

        // Create all levels
        for (int i=0;i<maxHtbDepth; i++){
            levels[i]=new htbLevel();
            levels[i]->levelId = i;
        }

        classModeChangeEvent = new cMessage("probablyClassNotRedEvent"); // Omnet++ event to take action when new elements to dequeue are available
    }
}

/*
 * METHODS FOR OMNET++ EVENTS - BEGIN
 */

// Wrapper for scheduleAt method
void HTBScheduler::scheduleAt(simtime_t t, cMessage *msg) {
    Enter_Method("scheduleAt");
    cSimpleModule::scheduleAt(t, msg);
}

// Wrapper for cancelEvent method
cMessage *HTBScheduler::cancelEvent(cMessage *msg) {
    Enter_Method("cancelEvent");
    return cSimpleModule::cancelEvent(msg);
}

// Handle internal events
void HTBScheduler::handleMessage(cMessage *message)
{
    Enter_Method("handleMessage");
    if (message == classModeChangeEvent) {
        EV << "Class mode should change from red now!"  << endl;
        check_and_cast<Ppp*>(getParentModule()->getParentModule())->refreshOutGateConnection(true);
//        int interfaceIndex = getParentModule()->getParentModule()->getParentModule()->getIndex();
//        double ber = check_and_cast<cDatarateChannel*>(getParentModule()->getParentModule()->getParentModule()->getParentModule()->gateByOrdinal(interfaceIndex)->getPreviousGate()->getChannel())->getDatarate();
//        check_and_cast<cDatarateChannel*>(getParentModule()->getParentModule()->getParentModule()->getParentModule()->gateByOrdinal(interfaceIndex)->getPreviousGate()->getChannel())->setDatarate(ber);
//        EV << "handle message will now call get num packets" << endl;
//        getNumPackets();
    }
    else
        throw cRuntimeError("Unknown self message");
}

/*
 * METHODS FOR OMNET++ EVENTS - END
 */

// Refreshes information on class mode when a change in the mode was expected.
// Not a real omnet event. Called only on dequeue when the next queue to pop is determined.
// Does all "events" on a level
simtime_t HTBScheduler::doEvents(int level) {
    // No empty events = nothing to do
    if (levels[level]->waitingClasses.empty()) {
        return 0;
    }

    int setLen = levels[level]->waitingClasses.size(); // Limit for the loop
    int i = 0; // Secondary iterator for the loop

    // Iterate over all pending "events". Care for deletion in a for loop!
    for (auto it = levels[level]->waitingClasses.begin(); it != levels[level]->waitingClasses.end() && i < setLen; ) {
        htbClass *cl = *it; // Class to update
        it++;

        // If the event for a class is still in the future then return the event time
        if (cl->nextEventTime > simTime()) {
            return cl->nextEventTime;
        }

        // Take the class out of waiting queue and update its mode
        levels[level]->waitingClasses.erase(cl);
        long long diff = (long long) std::min((simTime() - cl->checkpointTime).inUnit(SIMTIME_NS), (int64_t)cl->mbuffer);
        updateClassMode(cl, &diff);
        // If still not green (cen_send) readd to wait queue with new nex event time.
        if (cl->mode != can_send) {
            htbAddToWaitTree(cl, diff);
        }
        i++;
    }
    return simTime(); // If we managed all events, then just return current time.
}


// Returns the number of packets available to dequeue. If there are packets in the queue,
// but none are available to dequeue will schedule an event to update the interface and
// "force it" to dequeue again.
int HTBScheduler::getNumPackets() const
{
    const_cast<HTBScheduler *>(this)->cancelEvent(classModeChangeEvent);
    int fullSize = 0;  // Number of all packets in the queue
    int dequeueSize = 0; // Number of packets available for dequeueing
    int leafId = 0; // Id of the leaf/queue
    simtime_t changeTime = simTime() + SimTime(100000, SIMTIME_NS);; // Time at which we expect the next change of class mode
    for (auto collection : collections) { // Iterate over all leaf queues
        if (leafClasses.at(leafId) == nullptr) { // Leafs need to exist. If they don't, we've done sth. wrong :)
            throw cRuntimeError("There is no leaf at index %i!", leafId);
        }
        long long diff = (long long) std::min((simTime() - leafClasses.at(leafId)->checkpointTime).inUnit(SIMTIME_NS), (int64_t)leafClasses.at(leafId)->mbuffer);
        if (const_cast<HTBScheduler *>(this)->classMode(leafClasses.at(leafId), &diff) != cant_send) { // The case when there are packets available at leaf for dequeuing
            dequeueSize += collection->getNumPackets();
        } //else { // The class is red, so there is nothing available to dequeue
//            if (changeTime == 0 || leafClasses.at(leafId)->nextEventTime < changeTime) { // Update change time if applicable
//                changeTime = leafClasses.at(leafId)->nextEventTime;
//                EV << "Set change time to: " << changeTime << endl;
//            }
//        }
        fullSize += collection->getNumPackets(); // Keep track of all packets in leaf queues
        EV << "Queue " << leafId << " -> Num packets: " << collection->getNumPackets() << endl;
        leafId++;
    }

    EV << "Curr num packets to dequeue: " << dequeueSize << "; All packets: " << fullSize << endl;
    if (dequeueSize == 0 && fullSize > 0 && changeTime > simTime()) { // We have packets in queue, just none are available for dequeue!

        const_cast<HTBScheduler *>(this)->scheduleAt(changeTime, classModeChangeEvent);  // schedule an Omnet event on when we expect things to change.
    }
    return dequeueSize;
}


b HTBScheduler::getTotalLength() const
{
    b totalLength(0);
    for (auto collection : collections)
        totalLength += collection->getTotalLength();
    return totalLength;
}


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

// This is where the HTB scheduling comes into play
int HTBScheduler::schedulePacket() {
    int level;
    simtime_t nextEvent;

    int dequeueIndex = -1;

    //TODO: Dequeue direct???

    //TODO: What if queue empty? Probably implemented in the INET queuing module already...

    nextEvent = simTime(); // TODO: Probably redundant. Remove???

    // Go over all levels until we find something to dequeue
    for (level = 0; level < maxHtbDepth; level++) {
        printLevel(levels[level], level);
        nextEvent = doEvents(level); // Do all events for level

        // Go through all priorities on level until we find something to dequeue
        for (int prio = 0; prio < maxHtbNumPrio; prio++) {
            EV << "Dequeue - testing: level = " << level << "; priority = " << prio << endl;
            if (levels[level]->nextToDequeue[prio] != NULL) { // Next to dequeue is always right. If it's not null, then we can dequeue something there. If it's null, we know there is nothing to dequeue.
                EV << "Dequeue - Found class to dequeue on level " << level << " and priority " << prio << endl;
                dequeueIndex = htbDequeue(prio, level); // Do the dequeue in the HTB tree. Actual dequeue is done by the interface (I think)
            }
            if (dequeueIndex != -1) { // We found the first valid thing, just break!
                EV << "Dequeue - The class to dequeue (level = " << level << "; priority = " << prio << ") yielded a valid queue number " << dequeueIndex << " for dequeuing!" << endl;
                break;
            }
        }
        if (dequeueIndex != -1) { // We found the first valid thing, just break!
            break;
        }
    }
    EV << "Dequeue - Queue index to dequeue returned: " << dequeueIndex << endl;
    return dequeueIndex;
}

// Activates a class for a priority. Only really called for leafs
void HTBScheduler::activateClass(htbClass *cl, int priority) {
    if (!cl->activePriority[priority]) {
        cl->activePriority[priority] = true; // Class is now active for priority
        activateClassPrios(cl); // Take care of all other things associated with the active class (like putting it into appropriate feeds)
    }
}


// Deactivates a class for a priority. Only really called for leafs. Leafs may only be active for one priority.
void HTBScheduler::deactivateClass(htbClass *cl, int priority) {
    if (cl->activePriority[priority]) {
        deactivateClassPrios(cl); // Take care of all other things associated with deactivating a class
        cl->activePriority[priority] = false; // Class is now inactive for priority
    }
}

// Inform the htb about a newly enqueued packet. Enqueueing is actually done in the classifier.
void HTBScheduler::htbEnqueue(int index, Packet *packet) {
    int packetLen = packet->getByteLength();
    EV_INFO << "HTBScheduler: htbEnqueue " << index << "; Enqueue " << packetLen << " bytes." << endl;
    htbClass *currLeaf = leafClasses.at(index);
//    currLeaf->leaf.queueLevel += packetLen; //TODO: Take care of dropped packets!!!! Queue overflow or so...
//    emit(currLeaf->leaf.queueLvl, currLeaf->leaf.queueLevel);
    activateClass(currLeaf, currLeaf->leaf.priority);
    printClass(currLeaf);
    printLevel(levels[currLeaf->level], currLeaf->level);

//    EV_INFO << "HTBScheduler: Bytes in queue at index " << index << " = " << leafClasses.at(index)->type.leaf.queueLevel << endl;
    return;
}

// Gets a leaf that can be dequeued from a priority and level. Level does not have to be 0 here
HTBScheduler::htbClass* HTBScheduler::getLeaf(int priority, int level) {
    htbClass *cl = levels[level]->nextToDequeue[priority]; // We just take the next element as is. It will be correct thanks to activate/deactivate priorities
    // If there is nothing to dequeue just return NULL (actually)
    if (cl == NULL) {
        return cl;
    }
    // Otherwise, follow the tree to the leaf
    while (cl->level > 0) {
         cl = cl->inner.nextToDequeue[priority];
    }
    // And return the found leaf. Thanks to the rest of code, guaranteed to return leaf or NULL.
    return cl;
}

// Determine which leaf queue to dequeue based on the htb tree. Based heavily on Linux HTB source code
int HTBScheduler::htbDequeue(int priority, int level) {
    int retIndex = -1;
    htbClass *cl = getLeaf(priority, level); // Get first leaf we could find
    htbClass *start = cl;

    Packet *thePacketToPop = nullptr;

    htbClass * delClass = nullptr;

    do {
        if (delClass == start) {   // Fix start if we just deleted it
            start = cl;
            delClass = nullptr;
        }
        if (!cl) {
            EV << "Class does not exist!" << endl;
            return -1;
        }
        EV << "Checking class " << cl->name << endl;
        printClass(cl);
        // Take care if the queue is empty, but was not deactivated
        if (collections[cl->leaf.queueId]->isEmpty()) {
            EV << "Class " << cl->name << " was empty!!" << endl;
            htbClass *next;
            deactivateClass(cl, priority); // Also takes care of DRR if we deleted the nextToDequeue. Only needs to be called once, as leafs can only be active for one prio
            delClass = cl;
            next = getLeaf(priority, level); // Get next leaf available


            cl = next;
            continue;
        }
        thePacketToPop = providers[cl->leaf.queueId]->canPopPacket();

        if (thePacketToPop != nullptr) {
            EV << "Couldn't pop packet on class " << cl->name << "!" << endl;
            retIndex = cl->leaf.queueId;
            break;
        }

        cl = getLeaf(priority, level);

    } while (cl != start);

    if (thePacketToPop != nullptr) {
        EV_INFO << "HTBScheduler: htbDequeue " << retIndex << "; Dequeue " << thePacketToPop->getByteLength() << " bytes." << endl;
//        cl->leaf.queueLevel -= thePacketToPop->getByteLength();
//        emit(cl->leaf.queueLvl, cl->leaf.queueLevel);
        cl->leaf.deficit[level] -= thePacketToPop->getByteLength();
        if (cl->leaf.deficit[level] < 0) {
            cl->leaf.deficit[level] += cl->quantum;
            htbClass *tempNode = cl;
            while (tempNode != rootClass) {
                if (tempNode->parent->inner.innerFeeds[priority].size() > 1 && tempNode->mode == may_borrow) { //TODO: Probably need to take care of round robin!!
                    tempNode->parent->inner.nextToDequeue[priority] = *std::next(tempNode->parent->inner.innerFeeds[priority].find(tempNode));
                    if (tempNode->parent->inner.nextToDequeue[priority] == *tempNode->parent->inner.innerFeeds[priority].end()) {
                        tempNode->parent->inner.nextToDequeue[priority] = *tempNode->parent->inner.innerFeeds[priority].begin();
                    }
                    if (tempNode->parent->inner.nextToDequeue[priority] != *tempNode->parent->inner.innerFeeds[priority].begin()) {
                        break;
                    }
                } else if (levels[tempNode->level]->selfFeeds[priority].size() > 1 && tempNode->mode == can_send) {
                    EV << "We are going to advance the nextToDequeue on level " << tempNode->level << " and priority " << priority << endl;
                    printLevel(levels[tempNode->level], tempNode->level);
                    levels[tempNode->level]->nextToDequeue[priority] = *std::next(levels[tempNode->level]->selfFeeds[priority].find(tempNode));
                    if (levels[tempNode->level]->nextToDequeue[priority] == *levels[tempNode->level]->selfFeeds[priority].end()) {
                        levels[tempNode->level]->nextToDequeue[priority] = *levels[tempNode->level]->selfFeeds[priority].begin();
                    }
                    if (levels[tempNode->level]->nextToDequeue[priority] == nullptr) {
                        throw cRuntimeError("Next to dequeue would be null even though it shouldn't be!");
                    }
                    EV << "NextToDequeue is now " << levels[tempNode->level]->nextToDequeue[priority]->name << " on level " << tempNode->level << " and priority " << priority << endl;
                    break;
                } else if (levels[tempNode->level]->selfFeeds[priority].size() == 1 && tempNode->mode == can_send) {
                    break;
                }
                tempNode = tempNode->parent;
            }
        }
        /* this used to be after charge_class but this constelation
         * gives us slightly better performance
         */
        if (collections[cl->leaf.queueId]->isEmpty()) {
            deactivateClass(cl, priority); // Called for leaf. Leaf can be active for only one prio
        }
        chargeClass(cl, level, thePacketToPop);
    }

    printClass(cl);
    printLevel(levels[cl->level], cl->level);


    printInner(cl->parent);
    EV_INFO << "HTBScheduler: Bytes in queue at index " << retIndex << " = " << collections[cl->leaf.queueId]->getNumPackets() << endl;
    return retIndex;
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
    std::string printSet = "";
    for (auto & elem : level->waitingClasses){
        printSet.append(elem->name);
        printSet.append(" ;");
    }
    EV << "Wait queue for level " << level->levelId << ": " << printSet << endl;
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


inline long HTBScheduler::htb_lowater(htbClass *cl)
{
    if (htb_hysteresis)
        return cl->mode != cant_send ? -cl->cburstSize : 0;
    else
        return 0;
}

inline long HTBScheduler::htb_hiwater(htbClass *cl)
{
    if (htb_hysteresis)
        return cl->mode == can_send? -cl->burstSize : 0;
    else
        return 0;
}




int HTBScheduler::classMode(htbClass *cl, long long *diff) {

    long long toks;
    if ((toks = (cl->ctokens + *diff)) < htb_lowater(cl)) {
        *diff = -toks;
        EV << "Class mode diff 1 = " << *diff << endl;
        return cant_send;
    }

    if ((toks = (cl->tokens + *diff)) >= htb_hiwater(cl)) {
        EV << "Class mode diff 2 = " << *diff << endl;
        return can_send;
    }

    *diff = -toks;
    EV << "Class mode diff 3 = " << *diff << endl;
    return may_borrow;
}


void HTBScheduler::activateClassPrios(htbClass *cl) {
    EV << "activateClassPrios called for class " << cl->name << endl;
    htbClass *parent = cl->parent;

    bool newActivity[8];
    bool tempActivity[8];
    std::copy(std::begin(cl->activePriority), std::end(cl->activePriority), std::begin(newActivity));

    while (cl->mode == may_borrow && parent != NULL && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})) {
        std::copy(std::begin(newActivity), std::end(newActivity), std::begin(tempActivity));
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (tempActivity[i]) {
                if (parent->activePriority[i]) {
                    newActivity[i] = false;
                } else {
                    parent->activePriority[i] = true;
                }
                if (parent->inner.innerFeeds[i].size() < 1) {
                    parent->inner.nextToDequeue[i] = cl;
                }
                parent->inner.innerFeeds[i].insert(cl);
            }
        }
        cl = parent;
        parent = cl->parent;
    }

    printClass(cl);

    if (cl->mode == can_send && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})){
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (newActivity[i]) {
                if (levels[cl->level]->selfFeeds[i].size() < 1) {
                    levels[cl->level]->nextToDequeue[i] = cl;
                }
                levels[cl->level]->selfFeeds[i].insert(cl);
            }
        }
    }
}

void HTBScheduler::deactivateClassPrios(htbClass *cl) {
    EV << "deactivateClassPrios called for class " << cl->name << endl;
    htbClass *parent = cl->parent;

    bool newActivity[8];
    bool tempActivity[8];
    std::copy(std::begin(cl->activePriority), std::end(cl->activePriority), std::begin(newActivity));

    while (cl->mode == may_borrow && parent != NULL && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})) {
        std::copy(std::begin(newActivity), std::end(newActivity), std::begin(tempActivity));
        for (int i = 0; i < maxHtbNumPrio; i++) {
            newActivity[i] = false;
        }

        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (tempActivity[i]) {
                if (parent->inner.innerFeeds[i].find(cl) != parent->inner.innerFeeds[i].end()) {
                    if (parent->inner.innerFeeds[i].size() > 1) { //TODO: Probably need to take care of round robin!!
                        parent->inner.nextToDequeue[i] = *std::next(parent->inner.innerFeeds[i].find(cl));
                        if (parent->inner.nextToDequeue[i] == *parent->inner.innerFeeds[i].end()) {
                            parent->inner.nextToDequeue[i] = *parent->inner.innerFeeds[i].begin();
                        }
                    } else {
                        parent->inner.nextToDequeue[i] = NULL;
                    }
                    parent->inner.innerFeeds[i].erase(cl);
                }
                if (parent->inner.innerFeeds[i].empty()) {
                    parent->activePriority[i] = false;
                    newActivity[i] = true;
                }
            }
        }
        cl = parent;
        parent = cl->parent;
    }

    EV << "Working on class " << cl->name << " now with mode " << cl->mode << endl;

    if (cl->mode == can_send && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})){
        EV << "Class " << cl->name << " is with with mode " << cl->mode << " and is active on some priority!" << endl;
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            EV << "Testing priority " << i << endl;
            if (newActivity[i]) {
                EV << "Class is active on priority " << i << endl;
                if (levels[cl->level]->nextToDequeue[i] == cl) {
                    EV << "Class was considered next to dequeue on level  " << cl->level << " and priority " << i << endl;
                    EV << "Size " << cl->level << " = " << levels[cl->level]->selfFeeds[i].size() << endl;
                    if (levels[cl->level]->selfFeeds[i].size() > 1) { //TODO: Probably need to take care of round robin!!
                        EV << "There was more than one class in the self feed for level " << cl->level << " and priority " << i << endl;
                        EV << "Select next class as one to dequeue " << endl;
                        // Next to dequeue set to the next element in slef feed before removing the deactivated element
                        levels[cl->level]->nextToDequeue[i] = *std::next(levels[cl->level]->selfFeeds[i].find(cl));
                        if (levels[cl->level]->nextToDequeue[i] == *levels[cl->level]->selfFeeds[i].end()) {
                            levels[cl->level]->nextToDequeue[i] = *levels[cl->level]->selfFeeds[i].begin();
                        }
                        if (levels[cl->level]->nextToDequeue[i] == nullptr) {
                            throw cRuntimeError("Next to dequeue would be null even though it shouldn't be!");
                        }
                    } else {
                        EV << "There was one or fewer classe in the self feed for level " << cl->level << " and priority " << i << endl;
                        EV << "Set next to dequeue to NULL" << endl;
                        // There is nothing else other than the deactivated element in the self feed -> set next to dequeue to NULL (i.e. there is nothing to dequeue)
                        levels[cl->level]->nextToDequeue[i] = NULL;
                    }
                }
                EV << "Erase class " << cl->name << " with with mode " << cl->mode << " from self feed on level " << cl->level << " and priority " << i << endl;
                levels[cl->level]->selfFeeds[i].erase(cl);
            }
        }
    }
}

void HTBScheduler::updateClassMode(htbClass *cl, long long *diff) {

    int newMode = classMode(cl, diff);

    if (newMode == cl->mode)
            return;
    //Marija: this is just statistics I think
       /* if (new_mode == HTB_CANT_SEND) {

            cl->overlimits++;
            q->overlimits++;
            }*/

    // TODO: !!!!!!!!!!WE NEED THIS!!!!!!!!!!!
    printClass(cl);
    if (std::any_of(std::begin(cl->activePriority), std::end(cl->activePriority), [](bool b) {return b;})) {
        EV << "Deactivate then activate!" << endl;
        if (cl->mode != cant_send) {
            EV << "Deactivate priorities for class: " << cl->name << " in old mode " << cl->mode << endl;
            deactivateClassPrios(cl);
        }
        cl->mode = newMode;
        emit(cl->classMode, cl->mode);
        if (newMode != cant_send) {
            EV << "Activate priorities for class: " << cl->name << " in new mode " << cl->mode << endl;
            activateClassPrios(cl);
        }
    } else {
        cl->mode = newMode;
        emit(cl->classMode, cl->mode);
    }
}

void HTBScheduler::accountTokens(htbClass *cl, long long bytes, long long diff) {

    long long toks = diff + cl->tokens;
    if (toks > cl->burstSize)
        toks = cl->burstSize;

        //TODO:psched_l2t_ns(&cl->ceil, bytes), Length to Time (L2T) lookup in a qdisc_rate_table, to determine how
        //long it will take to send a packet given its size.
    /*LINUX psched:
     * static inline u64 psched_l2t_ns(const struct psched_ratecfg *r,
                                 unsigned int len)
 {
         len += r->overhead;

      if (unlikely(r->linklayer == TC_LINKLAYER_ATM))
                return ((u64)(DIV_ROUND_UP(len,48)*53) * r->mult) >> r->shift;

        return ((u64)len * r->mult) >> r->shift;
     */

        // toks -= (s64) psched_l2t_ns(&cl->ceil, bytes);
    toks-= bytes*8*1e9/cl->assignedRate;

    if (toks <= -cl->mbuffer)
        toks = 1 - cl->mbuffer;

    cl->tokens = toks;
    emit(cl->tokenBucket, cl->tokens);
    return;
}

void HTBScheduler::accountCTokens(htbClass *cl, long long bytes, long long diff) {

    long long toks = diff + cl->ctokens;
    if (toks > cl->cburstSize)
        toks = cl->cburstSize;
    //TODO:psched_l2t_ns(&cl->ceil, bytes)
    // toks -= (s64) psched_l2t_ns(&cl->ceil, bytes);
    toks-= bytes*8*1e9/cl->ceilingRate;
    if (toks <= -cl->mbuffer)
        toks = 1 - cl->mbuffer;

    cl->ctokens = toks;
    emit(cl->ctokenBucket, cl->ctokens);

   return;
}

void HTBScheduler::htbAddToWaitTree(htbClass *cl, long long delay) {
    cl->nextEventTime = simTime() + SimTime(delay, SIMTIME_NS);
    EV << "Expected class change at simtime " << cl->nextEventTime << endl;
    levels[cl->level]->waitingClasses.insert(cl);
}

void HTBScheduler::chargeClass(htbClass *leafCl, int borrowLevel, Packet *packetToDequeue) {
    long long bytes = (long long) packetToDequeue->getByteLength();
    int old_mode;
    long long diff;

    htbClass *cl;
    cl = leafCl;

    while (cl) {
        EV << "Before charge" << endl;
        printClass(cl);
        diff = (long long) std::min((simTime() - cl->checkpointTime).inUnit(SIMTIME_NS), (int64_t)cl->mbuffer);
        EV << "Diff is: " << diff << "; Packet Bytes: " << bytes << "Used toks:" << bytes*8*1e9/cl->assignedRate << "Used ctoks:" << bytes*8*1e9/cl->ceilingRate << endl;
        if (cl->level >= borrowLevel) {
//            if (cl->level == borrowLevel)
//                cl->xstats.lends++;
            accountTokens(cl, bytes, diff);
        } else {
//            cl->xstats.borrows++;
            cl->tokens += diff; /* we moved t_c; update tokens */
            emit(cl->tokenBucket, cl->tokens);
        }
        accountCTokens(cl, bytes, diff);

        EV << "Checkpoint Time before: " << cl->checkpointTime << endl;
        cl->checkpointTime = simTime();
        EV << "Checkpoint Time after: " << cl->checkpointTime << endl;

        old_mode = cl->mode;
        diff = 0;
        updateClassMode(cl, &diff);
        EV << "The diff = " << diff << endl;
        if (old_mode != cl->mode) {
            EV << "Mode changed from " << old_mode << " to " << cl->mode << endl;
            if (old_mode != can_send) {
                // TODO:Delete from wait queue
                levels[cl->level]->waitingClasses.erase(cl);
                EV << "Deleted " << cl->name << " from wait queue on level " << cl->level << endl;
//                htb_safe_rb_erase(&cl->pq_node, &q->hlevel[cl->level].wait_pq);
            }
            if (cl->mode != can_send) {
                // TODO:Add to wait queue
                htbAddToWaitTree(cl, diff);
                EV << "Added " << cl->name << " to wait queue on level " << cl->level << endl;
//                htb_add_to_wait_tree(q, cl, diff);
            }
        }

        /* update basic stats except for leaves which are already updated */
//        if (cl->level)
//            bstats_update(&cl->bstats, skb);
        EV << "After charge" << endl;
        printClass(cl);
        cl = cl->parent;
    }

    return;
}



} // namespace queueing
} // namespace inet





