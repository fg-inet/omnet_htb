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
// This implementation is heavily based on the implementation of Linux HTB qdisc by Martin Devera (https://github.com/torvalds/linux/blob/master/net/sched/sch_htb.c)
// Code base taken from the "PriorityScheduler"
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
    EV_WARN << "Class: " << cl->name << endl;
//    EV_WARN << "   - assigned rate: " << cl->assignedRate << endl;
//    EV_WARN << "   - ceiling rate: " << cl->ceilingRate << endl;
    EV_WARN << "   - burst size: " << cl->burstSize << endl;
    EV_WARN << "   - cburst size: " << cl->cburstSize << endl;
//    EV_WARN << "   - quantum: " << cl->quantum << endl;
//    EV_WARN << "   - mbuffer: " << cl->mbuffer << endl;
    EV_WARN << "   - chackpoint time: " << cl->checkpointTime << endl;
    EV_WARN << "   - level: " << cl->level << endl;
    EV_WARN << "   - number of children: " << cl->numChildren << endl;
    if (cl->parent != NULL) {
        EV_WARN << "   - parent class name: " << cl->parent->name << endl;
    }
    EV_WARN << "   - current tokens: " << cl->tokens << endl;
    EV_WARN << "   - current ctokens: " << cl->ctokens << endl;
    EV_WARN << "   - current class mode: " << cl->mode << endl;

    char actPrios[2*maxHtbNumPrio+1];

    for (int i = 0; i < maxHtbNumPrio; i++) {
        actPrios[2*i] = cl->activePriority[i] ? '1' : '0';
        actPrios[2*i+1] = ';';
    }
    actPrios[2*maxHtbNumPrio] = '\0';

    EV_WARN << "   - active priorities: " << actPrios << endl;

    if (strstr(cl->name,"leaf")) {
        EV_WARN << "   - leaf priority: " << cl->leaf.priority << endl;
        int leafNumPackets = collections[cl->leaf.queueId]->getNumPackets();
        EV_WARN << "   - current queue level: " << leafNumPackets << endl;
        EV_WARN << "   - queue num: " << cl->leaf.queueId << endl;
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
    long long cburstTemp = atoi(oneClass->getFirstChildWithTag("cburst")->getNodeValue());

    if (linkDatarate == -1) {
        throw cRuntimeError("Link datarate was -1!");
    }

    long long burst = burstTemp*8*1e+9/(long long)ceil;
    long long cburst = cburstTemp*8*1e+9/(long long)linkDatarate;
    
    newClass->burstSize = burst;
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
        newClass->leaf.queueId = atoi(oneClass->getFirstChildWithTag("queueNum")->getNodeValue());
        newClass->leaf.priority = atoi(oneClass->getFirstChildWithTag("priority")->getNodeValue());

        // Statistics collection for deficit
        for (int i = 0; i < maxHtbDepth; i++) {
            char deficitSignalName[50];
            sprintf(deficitSignalName, "class-%s-deficit%d", newClass->name, i);
            newClass->leaf.deficitSig[i] = registerSignal(deficitSignalName);
            char deficitStatisticName[50];
            sprintf(deficitStatisticName, "class-%s-deficit%d", newClass->name, i);
            cProperty *deficitStatisticTemplate = getProperties()->get("statisticTemplate", "deficit");
            getEnvir()->addResultRecorders(this, newClass->leaf.deficitSig[i], deficitStatisticName, deficitStatisticTemplate);
            emit(newClass->leaf.deficitSig[i], newClass->leaf.deficit[i]);
        }

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
    emit(newClass->tokenBucket, (long) newClass->tokens);

    // Statistics collection for ctoken levels
    char ctokenLevelSignalName[50];
    sprintf(ctokenLevelSignalName, "class-%s-ctokenLevel", newClass->name);
    newClass->ctokenBucket = registerSignal(ctokenLevelSignalName);
    char ctokenLevelStatisticName[50];
    sprintf(ctokenLevelStatisticName, "class-%s-ctokenLevel", newClass->name);
    cProperty *ctokenLevelStatisticTemplate = getProperties()->get("statisticTemplate", "ctokenLevel");
    getEnvir()->addResultRecorders(this, newClass->ctokenBucket, ctokenLevelStatisticName, ctokenLevelStatisticTemplate);
    emit(newClass->ctokenBucket, (long) newClass->ctokens);

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
        EV_WARN << "Get link datarate" << endl;
        int interfaceIndex = getParentModule()->getParentModule()->getParentModule()->getIndex();
        linkDatarate = getParentModule()->getParentModule()->getParentModule()->getParentModule()->gateByOrdinal(interfaceIndex)->getPreviousGate()->getChannel()->getNominalDatarate();
        EV_WARN << "SchedInit: Link datarate = " << linkDatarate << endl;
        //register signal for dequeue index
        dequeueIndexSignal = registerSignal("dequeueIndex");
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
        check_and_cast<Ppp*>(getParentModule()->getParentModule())->refreshOutGateConnection(true);
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

    // Iterate over all pending "events". Care for deletion in a for loop!
    for (auto it = levels[level]->waitingClasses.begin(); it != levels[level]->waitingClasses.end(); ) {
        htbClass *cl = *it; // Class to update

        it++;

        // If the event for a class is still in the future then return the event time
        if (cl->nextEventTime > simTime()) {
            EV_WARN << "doEvents: Considering class " << cl->name << " event lies in the future..." << endl;
            EV_WARN << "doEvents: Next event scheduled at " << cl->nextEventTime << " but current time is " << simTime() << endl;
            for (auto & elem : levels[cl->level]->waitingClasses){
                if (elem->nextEventTime <= simTime()) {
                    throw cRuntimeError("Class %s has an event that's not in the future but was not upadted!", elem->name);
                }
           }
            return cl->nextEventTime;
        }

        // Take the class out of waiting queue and update its mode
        htbRemoveFromWaitTree(cl);
        EV_WARN << "doEvents: Considering class " << cl->name << " in old mode " << std::to_string(cl->mode) << endl;
        long long diff = (long long) std::min((simTime() - cl->checkpointTime).inUnit(SIMTIME_NS), (int64_t)cl->mbuffer);
        updateClassMode(cl, &diff);
        EV_WARN << "doEvents: Considering class " << cl->name << " in new mode " << std::to_string(cl->mode) << endl;
        // If still not green (cen_send) readd to wait queue with new next event time.
        if (cl->mode != can_send) {
            htbAddToWaitTree(cl, diff);
        }
    }
    return simTime(); // If we managed all events, then just return current time.
}


// Returns the number of packets available to dequeue. If there are packets in the queue,
// but none are available to dequeue will schedule an event to update the interface and
// "force it" to dequeue again.
int HTBScheduler::getNumPackets() const {
    const_cast<HTBScheduler *>(this)->cancelEvent(classModeChangeEvent);
    int fullSize = 0;  // Number of all packets in the queue
    int dequeueSize = 0; // Number of packets available for dequeueing
    int leafId = 0; // Id of the leaf/queue
    simtime_t changeTime = simTime() + SimTime(100000, SIMTIME_NS); // Time at which we expect the next change of class mode
    for (auto collection : collections) { // Iterate over all leaf queues
        int collectionNumPackets = collection->getNumPackets();
        if (leafClasses.at(leafId) == nullptr) { // Leafs need to exist. If they don't, we've done sth. wrong :)
            throw cRuntimeError("There is no leaf at index %i!", leafId);
        }
        long long diff = (long long) std::min((simTime() - leafClasses.at(leafId)->checkpointTime).inUnit(SIMTIME_NS), (int64_t)leafClasses.at(leafId)->mbuffer);
        int currClassMode = const_cast<HTBScheduler *>(this)->classMode(leafClasses.at(leafId), &diff);
        if (currClassMode != cant_send) { // The case when there are packets available at leaf for dequeuing
            bool parentOk = false;
            htbClass *parent = leafClasses.at(leafId)->parent;
            if (currClassMode != can_send) { //&& leafClasses.at(leafId)->nextEventTime <= simTime()
                while (parent != NULL) { //&& parent->nextEventTime <= simTime()
                    long long diff2 = (long long) std::min((simTime() - parent->checkpointTime).inUnit(SIMTIME_NS), (int64_t)parent->mbuffer);
                    int parentMode = const_cast<HTBScheduler *>(this)->classMode(parent, &diff2);
                    if (parentMode == can_send && parent->nextEventTime <= simTime()) {
                        parentOk = true;
                        break;
                    } else if (parentMode == cant_send) {
                        break;
                    } else if (parentMode == may_borrow && (parentMode == parent->mode || parent->nextEventTime <= simTime())) {
                        parent = parent->parent;
                    } else {
                        break;
                    }
                }
            } else {
                parentOk = true;
            }
            if (parentOk == true) {
                if (currClassMode == can_send && leafClasses.at(leafId)->nextEventTime <= simTime()) {
                    dequeueSize += collectionNumPackets;
                } else if (currClassMode == may_borrow && (currClassMode == leafClasses.at(leafId)->mode || leafClasses.at(leafId)->nextEventTime <= simTime())) {
                    dequeueSize += collectionNumPackets;
                } 
            } 
        }
        fullSize += collectionNumPackets; // Keep track of all packets in leaf queues
        leafId++;
    }
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

    nextEvent = simTime(); // TODO: Probably redundant. Remove???

    // Go over all levels until we find something to dequeue
    for (level = 0; level < maxHtbDepth; level++) {
        nextEvent = doEvents(level); // Do all events for level

        // Go through all priorities on level until we find something to dequeue
        for (int prio = 0; prio < maxHtbNumPrio; prio++) {
            if (levels[level]->nextToDequeue[prio] != NULL) { // Next to dequeue is always right. If it's not null, then we can dequeue something there. If it's null, we know there is nothing to dequeue.
                dequeueIndex = htbDequeue(prio, level); // Do the dequeue in the HTB tree. Actual dequeue is done by the interface (I think)
            }
            if (dequeueIndex != -1) { // We found the first valid thing, just break!
                break;
            }
        }
        if (dequeueIndex != -1) { // We found the first valid thing, just break!
            break;
        }
    }

    emit(dequeueIndexSignal, dequeueIndex);
    return dequeueIndex; //index returned to the interface

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
    int packetLen = packet->getByteLength() + 7; // The +7 is related to the PPP link layer header and trailer size
    htbClass *currLeaf = leafClasses.at(index);
    activateClass(currLeaf, currLeaf->leaf.priority);
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
        if (!cl) { // Class does not exist...
            return -1;
        }

        // Take care if the queue is empty, but was not deactivated. Which in this implementation should never happen, hence the "dead code" after the error!
        if (collections[cl->leaf.queueId]->isEmpty()) {
            throw cRuntimeError("The class %s was empty! This should not have happened!", cl->name, level);
            htbClass *next;
            deactivateClass(cl, priority); // Also takes care of DRR if we deleted the nextToDequeue. Only needs to be called once, as leafs can only be active for one prio
            delClass = cl;
            next = getLeaf(priority, level); // Get next leaf available
            cl = next;
            continue;
        }
        thePacketToPop = providers[cl->leaf.queueId]->canPopPacket();

        if (thePacketToPop != nullptr) {
            retIndex = cl->leaf.queueId;
            break;
        }

        cl = getLeaf(priority, level);

    } while (cl != start);

    // Handle the DRR and packet dequeue preparation here
    if (thePacketToPop != nullptr) {
        if (cl->leaf.deficit[level] < 0) {
            throw cRuntimeError("The class %s deficit on level %d is negative!", cl->name, level);
        }
        cl->leaf.deficit[level] -= (thePacketToPop->getByteLength() + 7);
        emit(cl->leaf.deficitSig[level], cl->leaf.deficit[level]);
        if (cl->leaf.deficit[level] < 0) {
            cl->leaf.deficit[level] += cl->quantum;
            emit(cl->leaf.deficitSig[level], cl->leaf.deficit[level]);
            htbClass *tempNode = cl;
            while (tempNode != rootClass) {
                if (tempNode->parent->inner.innerFeeds[priority].size() > 1 && tempNode->mode == may_borrow) { // Take care of round robin!!
                    if (tempNode->parent->inner.nextToDequeue[priority] == cl) {
                        tempNode->parent->inner.nextToDequeue[priority] = *std::next(tempNode->parent->inner.innerFeeds[priority].find(tempNode));
                        if (tempNode->parent->inner.nextToDequeue[priority] == *tempNode->parent->inner.innerFeeds[priority].end()) {
                            tempNode->parent->inner.nextToDequeue[priority] = *tempNode->parent->inner.innerFeeds[priority].begin();
                        }
                    
                        // if (tempNode->parent->inner.nextToDequeue[priority] != *tempNode->parent->inner.innerFeeds[priority].begin()) { // Unclear if necessary, hence commented out.
                        //     break;
                        // }
                    } else {
                        break;
                    }
                } else if (levels[tempNode->level]->selfFeeds[priority].size() > 1 && tempNode->mode == can_send) {
                    if (levels[tempNode->level]->nextToDequeue[priority] == cl) {
                        levels[tempNode->level]->nextToDequeue[priority] = *std::next(levels[tempNode->level]->selfFeeds[priority].find(tempNode));
                        if (levels[tempNode->level]->nextToDequeue[priority] == *levels[tempNode->level]->selfFeeds[priority].end()) {
                            levels[tempNode->level]->nextToDequeue[priority] = *levels[tempNode->level]->selfFeeds[priority].begin();
                        }
                        if (levels[tempNode->level]->nextToDequeue[priority] == nullptr) {
                            throw cRuntimeError("Next to dequeue would be null even though it shouldn't be!");
                        }
                    }
                    break;
                } else if (levels[tempNode->level]->selfFeeds[priority].size() == 1 && tempNode->mode == can_send) {
                    break;
                }
                tempNode = tempNode->parent;
            }
        }
        /* this used to be after charge_class but this constelation
         * gives us slightly better performance
         * --> Note from the original implementation of the Linux HTB!
         */

        if (collections[cl->leaf.queueId]->getNumPackets() <= 1) {
//            EV_WARN << "Deactivate class called from htbDequeue!" << endl;
            deactivateClass(cl, priority); // Called for leaf. Leaf can be active for only one prio
        }
        chargeClass(cl, level, thePacketToPop);
    }

    return retIndex;
}

// A method for printing the level information in log
void HTBScheduler::printLevel(htbLevel *level, int index) {
    EV_WARN << "Self feeds for Level:  " << level->levelId << endl;
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
        EV_WARN << "   - Self feed for priority " << i  << ": " << printSet << endl;
    }
    EV_WARN << "Wait queue for level " << level->levelId << " contains " << level->waitingClasses.size() << " elements" << endl;
}

// A method for printing the inner class in log
void HTBScheduler::printInner(htbClass *cl) {
    EV_WARN << "Inner feeds for Inner Class:  " << cl->name << endl;
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
        EV_WARN << "   - Inner feed for priority " << i  << ": " << printSet << endl;
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



// Check in what mode the class is currently
int HTBScheduler::classMode(htbClass *cl, long long *diff) {
    long long toks;
    if ((toks = (cl->ctokens + *diff)) < htb_lowater(cl)) {
        *diff = -toks;
        return cant_send;
    }

    if ((toks = (cl->tokens + *diff)) >= htb_hiwater(cl)) {
        return can_send;
    }

    *diff = -toks;
    return may_borrow;
}

// Activate the class for priorities
void HTBScheduler::activateClassPrios(htbClass *cl) {
    EV_WARN << "activateClassPrios called for class " << cl->name << endl;
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

// Deactivate the class for priorities
void HTBScheduler::deactivateClassPrios(htbClass *cl) {
    EV_WARN << "deactivateClassPrios called for class " << cl->name << endl;
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
                    if (parent->inner.innerFeeds[i].size() > 1) { //Take care of round robin!!
                        if (parent->inner.nextToDequeue[i] == cl) {
                            parent->inner.nextToDequeue[i] = *std::next(parent->inner.innerFeeds[i].find(cl));
                            if (parent->inner.nextToDequeue[i] == *parent->inner.innerFeeds[i].end()) {
                                parent->inner.nextToDequeue[i] = *parent->inner.innerFeeds[i].begin();
                            }
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

    if (cl->mode == can_send && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})){
        EV_WARN << "Class " << cl->name << " is with with mode " << cl->mode << " and is active on some priority!" << endl;
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (newActivity[i]) {
                if (levels[cl->level]->nextToDequeue[i] == cl) {
                    if (levels[cl->level]->selfFeeds[i].size() > 1) { // Take care of round robin!!
                        // Next to dequeue set to the next element in self feed before removing the deactivated element
                        levels[cl->level]->nextToDequeue[i] = *std::next(levels[cl->level]->selfFeeds[i].find(cl));
                        if (levels[cl->level]->nextToDequeue[i] == *levels[cl->level]->selfFeeds[i].end()) {
                            levels[cl->level]->nextToDequeue[i] = *levels[cl->level]->selfFeeds[i].begin();
                        }
                        if (levels[cl->level]->nextToDequeue[i] == nullptr) {
                            throw cRuntimeError("Next to dequeue would be null even though it shouldn't be!");
                        }
                    } else {
                        // There is nothing else other than the deactivated element in the self feed -> set next to dequeue to NULL (i.e. there is nothing to dequeue)
                        levels[cl->level]->nextToDequeue[i] = NULL;
                    }
                }
                levels[cl->level]->selfFeeds[i].erase(cl);
            }
        }
    }
}

// Update the mode saved in class
void HTBScheduler::updateClassMode(htbClass *cl, long long *diff) {
    int newMode = classMode(cl, diff);
    EV_WARN << "updateClassMode - oldMode = " << cl->mode << "; newMode = " << newMode << endl;
    if (newMode == cl->mode)
            return;

    if (std::any_of(std::begin(cl->activePriority), std::end(cl->activePriority), [](bool b) {return b;})) {
       EV_WARN << "Deactivate then activate!" << endl;
        if (cl->mode != cant_send) {
           EV_WARN << "updateClassMode - Deactivate priorities for class: " << cl->name << " in old mode " << cl->mode << endl;
            deactivateClassPrios(cl);
        }
        cl->mode = newMode;
        emit(cl->classMode, cl->mode);
        if (newMode != cant_send) {
           EV_WARN << "updateClassMode - Activate priorities for class: " << cl->name << " in new mode " << cl->mode << endl;
            activateClassPrios(cl);
        }
    } else {
        cl->mode = newMode;
        emit(cl->classMode, cl->mode);
    }
}

// Update tokens used for dequeue
void HTBScheduler::accountTokens(htbClass *cl, long long bytes, long long diff) {
    long long toks = diff + cl->tokens;
    if (toks > cl->burstSize)
        toks = cl->burstSize;
    toks-= bytes*8*1e9/cl->assignedRate;
    if (toks <= -cl->mbuffer)
        toks = 1 - cl->mbuffer;

    cl->tokens = toks;
    emit(cl->tokenBucket, (long) cl->tokens);
    return;
}

// Update tokens used for dequeue
void HTBScheduler::accountCTokens(htbClass *cl, long long bytes, long long diff) {
    long long toks = diff + cl->ctokens;
    if (toks > cl->cburstSize)
        toks = cl->cburstSize;
    toks-= bytes*8*1e9/cl->ceilingRate;
    if (toks <= -cl->mbuffer)
        toks = 1 - cl->mbuffer;

    cl->ctokens = toks;
    emit(cl->ctokenBucket, (long) cl->ctokens);
   return;
}

// Add class to waiting tree when orange
void HTBScheduler::htbAddToWaitTree(htbClass *cl, long long delay) {
    cl->nextEventTime = simTime() + SimTime(delay, SIMTIME_NS);
    levels[cl->level]->waitingClasses.insert(cl);
    printLevel(levels[cl->level], cl->level);

}

void HTBScheduler::htbRemoveFromWaitTree(htbClass *cl) {
    std::multiset<htbClass*, waitComp>::iterator hit(levels[cl->level]->waitingClasses.find(cl));
    while (hit != levels[cl->level]->waitingClasses.end()) {
        if (*hit == cl) {
            levels[cl->level]->waitingClasses.erase(hit);
            break;
        } else {
            hit++;
        }
    }
}

// Charge all tokens/ctokens to class and update everything
void HTBScheduler::chargeClass(htbClass *leafCl, int borrowLevel, Packet *packetToDequeue) {
    long long bytes = (long long) packetToDequeue->getByteLength() + 7;
    int old_mode;
    long long diff;

    htbClass *cl;
    cl = leafCl;

    while (cl) {
        diff = (long long) std::min((simTime() - cl->checkpointTime).inUnit(SIMTIME_NS), (int64_t)cl->mbuffer);
        if (cl->level >= borrowLevel) {
            accountTokens(cl, bytes, diff);
        } else {
            cl->tokens += diff;
            emit(cl->tokenBucket, (long) cl->tokens);
        }
        accountCTokens(cl, bytes, diff);
        cl->checkpointTime = simTime();
        old_mode = cl->mode;
        diff = 0;
        updateClassMode(cl, &diff);
        if (old_mode != cl->mode) {
            if (old_mode != can_send) {
                htbRemoveFromWaitTree(cl);
            }
            if (cl->mode != can_send) {
                htbAddToWaitTree(cl, diff);
            }
        }
        cl = cl->parent;
    }
    return;
}



} // namespace queueing
} // namespace inet





