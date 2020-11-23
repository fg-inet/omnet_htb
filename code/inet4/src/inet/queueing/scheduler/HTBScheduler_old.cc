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
//#include <algorithm>
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

    long long rate = atoi(oneClass->getFirstChildWithTag("rate")->getNodeValue())*1e3;
    newClass->assignedRate = rate;
    long long ceil = atoi(oneClass->getFirstChildWithTag("ceil")->getNodeValue())*1e3;
    newClass->ceilingRate = ceil;
    long long burstTemp = atoi(oneClass->getFirstChildWithTag("burst")->getNodeValue());
    long long burst = burstTemp*1e+12/linkDatarate;
    newClass->burstSize = burst;
    long long cburstTemp = atoi(oneClass->getFirstChildWithTag("cburst")->getNodeValue());
    long long cburst = cburstTemp*1e+12/linkDatarate;
    newClass->cburstSize = cburst;
    int level = atoi(oneClass->getFirstChildWithTag("level")->getNodeValue());
    newClass->level = level;
    int quantum = atoi(oneClass->getFirstChildWithTag("quantum")->getNodeValue());
    newClass->quantum = quantum;
    long long mbuffer = atoi(oneClass->getFirstChildWithTag("mbuffer")->getNodeValue())*1e9;
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
        int interfaceIndex = getParentModule()->getParentModule()->getParentModule()->getIndex();
        linkDatarate = getParentModule()->getParentModule()->getParentModule()->getParentModule()->gateByOrdinal(interfaceIndex)->getPreviousGate()->getChannel()->getNominalDatarate();
        EV << "SchedInit: " << linkDatarate << endl;
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
        activateClassPrios(cl);
//        htbLevel *lvl = levels[cl->level];
//        lvl->selfFeeds[priority].insert(cl);

    }
}


//TODO: Marija: this has to be extended to check all self feeds of all levels, not only the level we are in ?
void HTBScheduler::deactivateClass(htbClass *cl, int priority) {
    if (cl->activePriority[priority]) {
        deactivateClassPrios(cl);
        cl->activePriority[priority] = false;
        htbLevel *lvl =  levels[cl->level];     //TODO: Do we need this here???
//        lvl->selfFeeds[priority].erase(cl);
        lvl->waitingClasses.erase(cl);          //TODO: Do we need this here???
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
    int packetLen = thePacketToPop->getByteLength();
    EV_INFO << "HTBScheduler: htbDequeue " << index << "; Dequeue " << packetLen << " bytes." << endl;
    currLeaf->leaf.queueLevel -= packetLen;

    if (currLeaf->leaf.queueLevel == 0) {
        deactivateClass(currLeaf, currLeaf->leaf.priority);
    }
    printClass(currLeaf);
    printLevel(levels[currLeaf->level], currLeaf->level);

    chargeClass(currLeaf, 0, thePacketToPop);
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

    signed int toks;
    if ((toks = (cl->ctokens + *diff)) < htb_lowater(cl)) {
        *diff = -toks;
        return cant_send;
    }

    if ((toks = (cl->tokens + *diff)) >= htb_hiwater(cl))
        return can_send;

    *diff = -toks;
    return may_borrow;
}


void HTBScheduler::activateClassPrios(htbClass *cl) {
    htbClass *parent = cl->parent;

    bool newActivity[maxHtbNumPrio];
    bool tempActivity[maxHtbNumPrio];
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
                parent->inner.innerFeeds[i].insert(cl);
            }
        }
        cl = parent;
        parent = cl->parent;
    }

    if (cl->mode == can_send && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})){
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (newActivity[i]) {
                levels[cl->level]->selfFeeds[i].insert(cl);
            }
        }
    }
}

void HTBScheduler::deactivateClassPrios(htbClass *cl) {
    htbClass *parent = cl->parent;

    bool newActivity[maxHtbNumPrio];
    bool tempActivity[maxHtbNumPrio];
    std::copy(std::begin(cl->activePriority), std::end(cl->activePriority), std::begin(newActivity));

    while (cl->mode == may_borrow && parent != NULL && std::any_of(std::begin(newActivity), std::end(newActivity), [](bool b) {return b;})) {
        std::copy(std::begin(newActivity), std::end(newActivity), std::begin(tempActivity));
        for (int i = 0; i < maxHtbNumPrio; i++) {
            newActivity[i] = false;
        }

        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (tempActivity[i]) {
                if (parent->inner.innerFeeds[i].find(cl) != parent->inner.innerFeeds[i].end()) {
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
        for (int i = 0; i < maxHtbNumPrio; i++) { // i = priority level
            if (newActivity[i]) {
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
    if (std::any_of(std::begin(cl->activePriority), std::end(cl->activePriority), [](bool b) {return b;})) {
        if (cl->mode != cant_send) {
            deactivateClassPrios(cl);
        }
        cl->mode = newMode;
        if (newMode != cant_send) {
            activateClassPrios(cl);
        }
    } else {
        cl->mode = newMode;
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

   return;
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
        }
        accountCTokens(cl, bytes, diff);

        EV << "Checkpoint Time before: " << cl->checkpointTime << endl;
        cl->checkpointTime = simTime();
        EV << "Checkpoint Time after: " << cl->checkpointTime << endl;

        old_mode = cl->mode;
        diff = 0;
        updateClassMode(cl, &diff);
        if (old_mode != cl->mode) {
            if (old_mode != can_send) {
                // TODO:Delete from wait queue
//                htb_safe_rb_erase(&cl->pq_node, &q->hlevel[cl->level].wait_pq);
            }
            if (cl->mode != can_send) {
                // TODO:Add to wait queue
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





