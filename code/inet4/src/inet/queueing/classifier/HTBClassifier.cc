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

#include "inet/queueing/classifier/HTBClassifier.h"

namespace inet {
namespace queueing {

Define_Module(HTBClassifier);

HTBClassifier::~HTBClassifier()
{
    for (auto filter : filters)
        delete filter;
}

void HTBClassifier::initialize(int stage)
{
    EV_FATAL << "Testing..." << endl;
    PacketClassifierBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        EV_FATAL << "Classifier is in initstage 0, local :)" << endl;
        defaultGateIndex = par("defaultGateIndex");
        cStringTokenizer packetFilterTokenizer(par("packetFilters"), ";");
        cStringTokenizer packetDataFilterTokenizer(par("packetDataFilters"), ";");
        while (packetFilterTokenizer.hasMoreTokens() && packetDataFilterTokenizer.hasMoreTokens()) {
            auto filter = new PacketFilter();
            filter->setPattern(packetFilterTokenizer.nextToken(), packetDataFilterTokenizer.nextToken());
            filters.push_back(filter);
        }
    }
    if (stage == 1) {
        EV_FATAL << "Classifier is in initstage 1 :)" << endl;
        cModule *targetModule = getParentModule()->getSubmodule("scheduler");
        scheduler = check_and_cast<HTBScheduler *>(targetModule);
        scheduler->printTest();
    }
}

int HTBClassifier::classifyPacket(Packet *packet)
{
//    scheduler->printTest();
    for (int i = 0; i < (int)filters.size(); i++) {
        auto filter = filters[i];
//        EV_INFO << "Filter: " << filter << endl;
        if (filter->matches(packet)) {
            EV_FATAL << "Enqueue packet to " << i << endl;
            scheduler->htbEnqueue(i, packet);
            return i;
        }
    }
    EV_FATAL << "Enqueue packet to default gate " << defaultGateIndex << endl;
    scheduler->htbEnqueue(defaultGateIndex, packet);
    return defaultGateIndex;
}

} // namespace queueing
} // namespace inet

