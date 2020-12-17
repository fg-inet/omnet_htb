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

package inet.queueing.scheduler;

import inet.queueing.base.PacketSchedulerBase;
import inet.queueing.contract.IPacketScheduler;

//
// This classifier pops packets from the first non-empty among its connected
// packet providers.
//
simple HTBScheduler extends PacketSchedulerBase like IPacketScheduler
{
    parameters:
        xml htbTreeConfig; // The htb tree structure
        
        bool htbHysterisis = default(false); // Use hysterisis?
        
        @class(HTBScheduler);
        
//        @signal[class-*-queueLevel](type=long);
//		@statisticTemplate[queueLevel](record=vector);
	
		@signal[class-*-tokenLevel](type=long);
		@statisticTemplate[tokenLevel](record=vector);
		@signal[class-*-ctokenLevel](type=long);
		@statisticTemplate[ctokenLevel](record=vector);
		
		@signal[class-*-deficit*](type=long);
		@statisticTemplate[deficit](record=vector);
		
		@signal[class-*-mode](type=long);
		@statisticTemplate[classMode](record=vector);
}
