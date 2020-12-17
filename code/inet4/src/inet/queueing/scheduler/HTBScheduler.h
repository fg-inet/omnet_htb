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

#ifndef __INET_HTBSCHEDULER_H
#define __INET_HTBSCHEDULER_H

#include "inet/queueing/base/PacketSchedulerBase.h"
#include "inet/queueing/contract/IPacketCollection.h"
#include "inet/common/XMLUtils.h"

//#include "inet/common/INETDefs.h"

#include <string>
#include <set>
#include <vector>
#include <array>
#include <cstdlib>
#include <algorithm>

namespace inet {
namespace queueing {

class INET_API HTBScheduler : public PacketSchedulerBase, public IPacketCollection
{
  protected:
    std::vector<IPacketCollection *> collections; // The actual queues

    static const int maxHtbDepth = 8; // The maximal amount of levels of htb tree
    static const int maxHtbNumPrio = 8; // The maximal number of priorities
    static const int can_send = 0; // The class is in can send mode (green)
    static const int may_borrow = 1; // The class is in may borrow mode (yellow)
    static const int cant_send = 2; // The class is in can't send mode (red)

    bool htb_hysteresis; // Use hysteresis determine class mode

    double linkDatarate; // The datarate of connected link

    // Structure for the class
    struct htbClass {
        const char *name = "";
        long long assignedRate = 0; // assured rate
        long long ceilingRate = 0; // ceiling rate
        long long burstSize = 0;
        long long cburstSize = 0;

        int quantum = 0;
        long long mbuffer = 0;

        simtime_t checkpointTime = 0; // Time of last token update
        int level = 0; // Level within the tree. !!!!!! ALWAYS 0 = LEAF !!!!!!
        int numChildren = 0; // Number of connected children

        htbClass *parent = NULL; // Pointer to parent node. NULL for root
        // We are only keeping information about the parent. There is usually no need to traverse the tree from the root.
        // We only need to traverse it from the top when there are classes in the inner feeds, which then store the pointers to children

        // The token buckets
        long tokens = 0; // For assured rate
        long ctokens = 0; // For ceiling rate

        // The mode of the class
        int mode = can_send;

        // Specifies for which priorities the class is now active and in green/yellow mode
        bool activePriority[maxHtbNumPrio] = { false };

        // If class not in green mode, it specifies when the next mode change is expected
        simtime_t nextEventTime = 0;

        struct htbClassLeaf { // Special class information for leaf
            int priority;
            int deficit[maxHtbDepth]; // Used for deficit round robin (DRR)
            simsignal_t deficitSig[maxHtbDepth]; // Signal for queue level statistics collection
            int queueId; // Id of the corresponding queue
        } leaf;
        struct htbClassInner { // Special class information for inner/root
            std::set<htbClass*> innerFeeds[maxHtbNumPrio]; // Inner feeds of inner/root class
            htbClass* nextToDequeue[maxHtbNumPrio]; // DRR -> which class to dequeue next from inner feed
        } inner;

        // Statistics collection signals
        simsignal_t tokenBucket;
        simsignal_t ctokenBucket;
        simsignal_t classMode;
    };

    struct waitComp { // Comparator to sort the waiting classes according to their expected mode change time
        bool operator()(htbClass* const & a, htbClass* const & b) {
            return a->nextEventTime < b->nextEventTime;
        }
    };

    // Struct for the tree level
    struct htbLevel {
        unsigned int levelId; // Level number. 0 = Leaf
        std::set<htbClass*> selfFeeds[maxHtbNumPrio]; // Self feeds for each priority. Contain active green classes
        htbClass* nextToDequeue[maxHtbNumPrio]; // Next green class to dequeue on level
        std::set<htbClass*, waitComp> waitingClasses; // Red classes waiting to become non-red.
    };

    htbLevel* levels[maxHtbDepth]; // Levels saved here

    cXMLElement *htbConfig = nullptr; // XML config for the htb tree

    htbClass* rootClass; // Root class saved here
    std::vector<htbClass*> innerClasses; // Inner classes saved here for ease of access
    std::vector<htbClass*> leafClasses; // Leaf classes saved here for ease of access

    // TODO: WIP: solve the problem that omnet does not periodically tries to dequeue.
    cMessage *classModeChangeEvent = nullptr;


  protected:
    virtual void initialize(int stage) override;
    virtual int schedulePacket() override;
    virtual void handleMessage(cMessage *msg) override;

  public:
    int classMode(htbClass *cl, long long *diff);
    virtual int getMaxNumPackets() const override { return -1; }
    virtual int getNumPackets() const override;

    virtual b getMaxTotalLength() const override { return b(-1); }
    virtual b getTotalLength() const override;

    virtual bool isEmpty() const override { return getNumPackets() == 0; }
    virtual Packet *getPacket(int index) const override;
    virtual void removePacket(Packet *packet) override;

    void htbEnqueue(int index, Packet *packet);
    int htbDequeue(int priority, int level);
    void printClass(htbClass *cl);
    void printLevel(htbLevel *level, int index);
    void printInner(htbClass *cl);
    htbClass *createAndAddNewClass(cXMLElement* oneClass, int queueId);
    void activateClass(htbClass *cl, int priority);
    void deactivateClass(htbClass *cl, int priority);

    void activateClassPrios(htbClass *cl);
    void deactivateClassPrios(htbClass *cl);

    inline long htb_hiwater(htbClass *cl);
    inline long htb_lowater(htbClass *cl);

    void updateClassMode(htbClass *cl, long long *diff);
    void accountTokens(htbClass *cl, long long bytes, long long diff);
    void accountCTokens(htbClass *cl, long long bytes, long long diff);
    void chargeClass(htbClass *leafCl, int borrowLevel, Packet *packetToDequeue);

    void htbAddToWaitTree(htbClass *cl, long long delay);
    htbClass *getLeaf(int priority, int level);
    simtime_t doEvents(int level);

    virtual void scheduleAt(simtime_t t, cMessage *msg) override;
    virtual cMessage *cancelEvent(cMessage *msg) override;

};

} // namespace queueing
} // namespace inet

#endif // ifndef __INET_HTBSCHEDULER_H

