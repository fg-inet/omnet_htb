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


namespace inet {
namespace queueing {

#define HTB_HYSRERESIS 0;

#define HTB_MAX_NUM_PRIO 8;

class INET_API HTBScheduler : public PacketSchedulerBase, public IPacketCollection
{
  protected:
    std::vector<IPacketCollection *> collections;

    static const int maxHtbDepth = 8;
    static const int maxHtbNumPrio = 8;
    static const int can_send = 0;
    static const int may_borrow = 1;
    static const int cant_send = 2;
    static const int htb_hysteresis=0;

    double linkDatarate;

    struct htbClass {
        const char* name = "";
        long long assignedRate = 0;
        long long ceilingRate = 0;
        long long burstSize = 0;
        long long cburstSize = 0;

        int quantum = 0;
        long long mbuffer = 0;

        simtime_t checkpointTime = 0;
        int level = 0;
        int numChildren = 0;

        htbClass *parent = NULL;

        long tokens = 0;
        long ctokens = 0;

        int mode = can_send;

        bool activePriority[maxHtbNumPrio] = { false };

        struct htbClassLeaf {
            int priority;
            int deficit[maxHtbDepth];
            long queueLevel;
            int queueId;
        } leaf;
        struct htbClassInner {
            std::set<htbClass*> innerFeeds[maxHtbNumPrio];
            htbClass* nextToDequeue[maxHtbNumPrio];
        } inner;
    };

    struct htbLevel {
        unsigned int levelId;
        std::set<htbClass*> selfFeeds[maxHtbNumPrio];
        htbClass* nextToDequeue[maxHtbNumPrio];
        std::set<htbClass*> waitingClasses;
    };



    htbLevel* levels[maxHtbDepth];
  // std::vector<htbLevel*> levels;

    cXMLElement *htbConfig = nullptr;

//    std::vector<htbClass*> classes;

    htbClass* rootClass;
    std::vector<htbClass*> innerClasses;
    std::vector<htbClass*> leafClasses;


  protected:
    virtual void initialize(int stage) override;
    virtual int schedulePacket() override;

  public:
    virtual int getMaxNumPackets() const override { return -1; }
    virtual int getNumPackets() const override;

    virtual b getMaxTotalLength() const override { return b(-1); }
    virtual b getTotalLength() const override;

    virtual bool isEmpty() const override { return getNumPackets() == 0; }
    virtual Packet *getPacket(int index) const override;
    virtual void removePacket(Packet *packet) override;

//    void addToClass(Packet *packet, int gateIndex);
    void printTest();

    void htbEnqueue(int index, Packet *packet);
    void htbDequeue(int index);
//    htbClass *htbInitializeNewClass();
    void printClass(htbClass *cl);
    void printLevel(htbLevel *level, int index);
    void printInner(htbClass *cl);
    htbClass *createAndAddNewClass(cXMLElement* oneClass, int queueId);
    void activateClass(htbClass *cl, int priority);
    void deactivateClass(htbClass *cl, int priority);

    void activateClassPrios(htbClass *cl);
    void deactivateClassPrios(htbClass *cl);

    // TODO: Next methods for accounting!
    inline long htb_hiwater(htbClass *cl);
    inline long htb_lowater(htbClass *cl);
    int classMode(htbClass *cl, long long *diff);
    void updateClassMode(htbClass *cl, long long *diff); //TODO: Marija
    void accountTokens(htbClass *cl, long long bytes, long long diff); //TODO: Marija
    void accountCTokens(htbClass *cl, long long bytes, long long diff); //TODO: Marija
    void chargeClass(htbClass *leafCl, int borrowLevel, Packet *packetToDequeue); //TODO: Marcin

};

} // namespace queueing
} // namespace inet

#endif // ifndef __INET_HTBSCHEDULER_H

