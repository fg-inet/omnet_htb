# OMNeT++/INET Implementation of the Hierarchical Token Bucket Queuing Module

Repository including the implementation of Hierarchical Token Bucket (HTB) for OMNeT++ with INET Framework. The repository is structured as follows:
1. The code folder contains the implementation files of the HTB
2. The examples folder contains one project that includes all files necessary to run 3 exemplary scenarios using HTB

Corresponding publication containing an in-depth explanation of the Hierarchical Token Bucket as well as its implementation in OMNeT++/INET can be found [here](https://summit.omnetpp.org/2021/assets/pdf/OMNeT_2021_paper_8.pdf).

## Installation

In order to run HTB you need OMNeT++ in version 5.5.1 (available [here](https://omnetpp.org/download/old)) and INET Framework in version 4.2.0 (available [here](https://github.com/inet-framework/inet/releases/tag/v4.2.0)).

### Steps
1. Download and Compile Omnet++ - follow the guide https://doc.omnetpp.org/omnetpp/InstallGuide.pdf
2. Download, unzip and prepare the INET Framework for compilation
3. Merge the [code/inet4](code/inet4) folder into the inet4 folder that you prepared in step 2
4. Compile the INET framework.

## User guide - WIP
The HTBQueue Compound Module can be used in place of any queuing module currently present within the INET Framework. Currently, only the PPP Interface is supported.
Examplary simulations can be found [here](examples/simulations).

Two configuration elements are required for the use of the HTBQueue Module within your simulation:
1. An XML configuration containing the HTB tree structure
2. The INI file configuration of the Queuing Module for your interface.

### The XML file
There are a few special requirements that the XML file needs to fulfil:
- The order of the classes in the XML file matters!!!
- The first class to be specified needs to be the root
- If you are configuring the inner classes, they need to directly follow the root
- The leafs need to be specified at the end of the XML. The leafs need to be sorted in ascending order based on the configured queueNum, i.e. lowest queueNum first.

There are some limitations to the XML configuration and the HTB configuration in general:
- You can configure up to 8 levels within the HTB tree (TODO: Insert a tree)
- You can have up to 8 various priorities. Priority 0 is the highest. Priority 7 is the lowest.
- Burst, cburst, and quantum cannot be lower than the maximum transmission unit. The queue will throw an error if so configured.
- queueNum starts at 0. The number of queues (configured in INI) needs to correspond to the number of leafs in HTB.
- The level needs to correspond to the level within the tree, with 0 being reserved for the leaves. Parent of a class cannot be on the same or lower level than the child!
- The parent of root needs to be "NULL" and root class id needs to be "root"
- Sum of children's assured rates cannot exceed the assured rate of the parent!

Furthermore, some values can be configured automatically for the classes. These values include each class's burst, cburst, and quantum. The user can also configure those values themselves.

An exemplary XML configuration with one inner class and one leaf class is shown below:
```xml
<config>
	<class id="root"> <!--The id is the name of the class. The first class always needs to be called root.-->
		<parentId>NULL</parentId> <!--Parent of class in HTB tree structure. For root always needs to be NULL.-->
		<rate type="int">50000</rate> <!--The assured rate in kbit/s. Here = 50 Mbit/s-->
		<ceil type="int">50000</ceil> <!--The ceiling rate in kbit/s. Here = 50 Mbit/s-->
		<burst type="int">6250</burst> <!--The burst bytes at assured rate. Must not be smaller than MTU. Should not be smaller than rate/8000 to allow for 1ms of transmit time-->
		<cburst type="int">6250</cburst> <!--The burst bytes at ceiling rate. Must not be smaller than MTU. Should not be smaller than ceil/8000 to allow for 1ms of transmit time-->
		<level type="int">2</level> <!--The level of the class in HTB tree hierarchy. For root it has to be above all other classes. In this case = 2-->
		<quantum type="int">1500</quantum> <!--The number of bytes that can be transmitted by a class before the next class is selected for transmission with the deficit round robin algorithm. Used to avoid class starvation.-->
		<mbuffer type="int">60</mbuffer> <!--The time for which big burst events are remembered in seconds-->
	</class>
	<class id="innerClass0"> <!--The id is the name of the class. The inner classes must always be specified directly after root and before leaves. Inner class needs to have "inner" in it's name!-->
		<parentId>root</parentId> <!--Parent of class in HTB tree structure. For inner can be root or other inner classes that are on a higher level-->
		<rate type="int">20000</rate> <!--The assured rate in kbit/s. Here = 20 Mbit/s-->
		<ceil type="int">40000</ceil> <!--The ceiling rate in kbit/s. Here = 40 Mbit/s-->
		<burst type="int">2500</burst> <!--The burst bytes at assured rate. Must not be smaller than MTU. Should not be smaller than rate/8000 to allow for 1ms of transmit time-->
		<cburst type="int">5000</cburst> <!--The burst bytes at ceiling rate. Must not be smaller than MTU. Should not be smaller than ceil/8000 to allow for 1ms of transmit time-->
		<level type="int">1</level> <!--The level of the class in HTB tree hierarchy. For inner it has to be > 0 and smaller than that of root. In this case = 1-->
		<quantum type="int">1500</quantum> <!--The number of bytes that can be transmitted by a class before the next class is selected for transmission with the deficit round robin algorithm. Used to avoid class starvation.-->
		<mbuffer type="int">60</mbuffer> <!--The time for which big burst events are remembered in seconds-->
	</class>
	<class id="leafClass0"> <!--The id is the name of the class. The leaf classes must always be specified after all inner classes. Leaf class needs to have "leaf" in it's name!-->
		<parentId>innerClass0</parentId> <!--Parent of class in HTB tree structure. For leaf can be root or any inner class-->
		<rate type="int">3000</rate> <!--The assured rate in kbit/s. Here = 3 Mbit/s-->
		<ceil type="int">20000</ceil> <!--The ceiling rate in kbit/s. Here = 20 Mbit/s-->
		<burst type="int">1500</burst> <!--The burst bytes at assured rate. Must not be smaller than MTU. Should not be smaller than rate/8000 to allow for 1ms of transmit time-->
		<cburst type="int">2500</cburst> <!--The burst bytes at ceiling rate. Must not be smaller than MTU. Should not be smaller than ceil/8000 to allow for 1ms of transmit time-->
		<level type="int">0</level> <!--The level of the class in HTB tree hierarchy. For leaf it must be = 0. In this case = 0-->
		<quantum type="int">1500</quantum> <!--The number of bytes that can be transmitted by a class before the next class is selected for transmission with the deficit round robin algorithm. Used to avoid class starvation.-->
		<mbuffer type="int">60</mbuffer> <!--The time for which big burst events are remembered in seconds-->
		<priority>0</priority> <!--The priority of class (0 = highest, 7 = lowest). Can only be specified for leaves and gets inhereted in the hierarhy. Classes with higher prioriy will always have their assured rate satisfied first. Same applies to the borrowed rate.-->
		<queueNum type="int">0</queueNum> <!--The number of PacketQueue configured for interface. Relevant for the classifier and for the HTB to know from which queue packets should be dequeued for the leaf.-->
	</class>
</config>
```

## Verification results

Plots of simple verification results using UDP and TCP applications coming soon.

