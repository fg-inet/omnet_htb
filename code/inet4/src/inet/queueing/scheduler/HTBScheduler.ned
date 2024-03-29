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

package inet.queueing.scheduler;

import inet.queueing.base.PacketSchedulerBase;
import inet.queueing.contract.IPacketScheduler;

simple HTBScheduler extends PacketSchedulerBase like IPacketScheduler
{
    parameters:
        xml htbTreeConfig; // The htb tree structure
        
        bool htbHysterisis = default(false);

		int mtu @unit(B) = default(1500B);
		bool checkHTBTreeValuesForCorectness = default(true); // Check if some important (burst, cburst, quantum) inputted values in the XML Tree are correct. Will throw errors on incorrect values!
		bool adjustHTBTreeValuesForCorectness = default(true); // Adjust some important (burst, cburst, quantum) inputted values in the XML Tree if they are incorrect. Will just adjust incorrect values if the above check is disabled
        
        @class(HTBScheduler);
	
		@signal[dequeueIndex](type=long);
		@statistic[dequeueIndex](title="Index of a queue that just dequeued"; record=vector);
		
	
		@signal[class-*-tokenLevel](type=long);
		@statisticTemplate[tokenLevel](record=vector);
		@signal[class-*-ctokenLevel](type=long);
		@statisticTemplate[ctokenLevel](record=vector);
		
		@signal[class-*-deficit*](type=long);
		@statisticTemplate[deficit](record=vector);
		
		@signal[class-*-mode](type=long);
		@statisticTemplate[classMode](record=vector);
}
