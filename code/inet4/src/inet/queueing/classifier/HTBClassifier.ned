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
// This implementation is an extended version of the "ContentBasedClassifier"!
//

package inet.queueing.classifier;

import inet.queueing.base.PacketClassifierBase;
import inet.queueing.contract.IPacketClassifier;

//
// This module connects one packet producer to multiple packet consumers. It
// can be pushed with packets from the connected packet producer. When this
// happens, the classifier pushes the packet to one of its connected packet
// consumers based on the configured packet filters. The first matching expression
// determines the index of the output gate.
//
simple HTBClassifier extends PacketClassifierBase like IPacketClassifier
{
    parameters:
        int defaultGateIndex = default(0); // default gate index if no matching labels are found
        string packetFilters; // semicolon separated list of packet filter expressions (e.g. ping-*;udp-*), matches all packets by default
        string packetDataFilters; // semicolon separated list of packet data filter expressions (e.g. inet::Ipv4Header and srcAddress = "10.0.0.1"), matches all packets by default
        xml htbTreeConfig; // The htb tree structure
        @class(HTBClassifier);
}
