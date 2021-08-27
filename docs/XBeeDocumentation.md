# To Bee or not to Bee: a crash course in XBee communication 
Authored and maintained by Lydia Calderon-Aceituno <lcalderon-aceituno@my.whitworth.edu>. References and images used in this guide are taken from [Digi's official XBee documentation](https://www.digi.com/resources/documentation/Digidocs/90001456-13/Default.htm#concepts) and accompanying documents. 

## Introduction 
XBee modules are radio frequency (RF) devices that transmit and receive data over the air using radio signals. XBees are configurable and support multiple protocols. In order to transmit data from one XBee to another, both modules must be on the same frequency and in the same network. The frequency and network of an XBee are determined through two parameters:

* Channel (CH) tells the XBee the frequency at which to communicate 
* Personal Area Network identifier (ID) establishes a common unique network ID. All XBees with this ID can communicate with one another. 

XBees can only receive data from and sent data to other XBees with the same CH and ID numbers. 

In order to talk to each other, XBees also have three different types of adresses. 
| Type | Example | Unique |
|------|-------|----------|
| 64-bit | 0013A20012345678 | Always |
| 16-bit | 1234 | Yes, but only within a network |
| Node identifier | Karen | Not necessarily unique | 

Every XBee's 64-bit address can be found on the back of the module and can also be read from the Serial Number High (SH) and Serial Number Low (SL) parameters on the device. The value of the 16-bit address can be read or set from the Source Address (MY) parameter. The node identifier is useless as far as uniqueness, but does give XBee modules a nice human-friendly name. 

## Point to multi-point communication 
When you have multiple XBee devices, a module can communicate with one or multiple devices on a network. This type of communication generally involves a central XBee called the coordinator with multiple remote modules called end nodes or end devices which are connected back to the central coordinator. 

![star_comm](img/star_topology.jpg)

A few more specifics on the roles:
* The **coordinator** is the central node of the network. It initializes the network, can set the frequency of communication, and is the gatekeeper in charge of allowing other devices to joing the network. To crown a XBee device coordinator, you must set the Coordinator Enable (CE) parameter to Coordinator [1]. There can only ever be ONE coordinator in a network.
* An **end device** is a remote node of the network. End devices can communicate with both the coordinator and other end devices. End devices can be put in low power modes to conserve battery life. To set an XBee device as an end device, change the CE parameter to End Device [0].

## Operating modes 
XBee devices have different modes of communication which dictate how the host device communicates with an XBee through the serial interface: transparent mode and API mode.

### Application transparent mode 
Information is passed along exactly as it is received. All serial data received by the radio module is sent wirelessly to a remote XBee with no additional information or structure. 

### API mode 
Application Programming Interface operating mode is an alternative to transparent mode. API mode provides a structured interface where data is communicated through the serial interface in organized packets and in a determined order. The structured data packets in API mode are called frames. They are sent and received through the serial interface of the device and contain the wireless message itself as well as some extra information such as the destination/source of the data or the signal quality. 

This particular library focuses on interfacing with XBee 802.15.4 using API mode, so this guide will not cover the specifics of transparent mode. 

## API Operation  
The frame structure for API mode is as follows:
| Frame fields | Byte | Description |
|---------|---------|-----------|
| Start delimiter | 1 | 0x7E |
| Length | 2-3 | MSB, LSB |
| Frame data | 4 - n | API-specific structure |
| Checksum | n + 1 | 1 byte | 

Any data received before the start delimiter is discarded and if the frame is not received corrrectly or the checksum fails, the XBee responds with a radio status frame indicating the nature of the failure.

### API frames 
Below is a comprehensive list of the API frames. For more information, please visit [this link](). Note that both the table and link provide information on frames specific to **XBee 3 modules ONLY**. There are other pages you can visit for information on Legacy S1 or XBee S2C Pro modules. Frame descriptions are copied verbatim from Digi's official XBee 3 guide for the sake of accuracy. 

| Name | Hex value | Description |
|--------|---------|-------------| 
| Local AT command request | 0x08 | Used to query or set command parameters on the local device. | 
| Queue local AT command request | 0x09 | Used to query or set queued command parameters on the local device. |
| Transmit request | 0x10 | Used to send payload data as an RF packet to a specific destination. |
| Explicit adressing command request | 0x11 | Used to send payload datqa as an RF packet to a specific destination using application-layer addressing fields. This frame is similar to 0x10 but with additional fields avaliable for user defined endpoints, cluster ID, and profile ID. |
| Remote AT command request | 0x17 | Used to query or set AT command parameters on a remote device. |
| Create source route | 0x21 | Used to create an entry in the source route table of a local device. A source route specifies the complete route a packet traverses to get from source to destination. |
| Register joining device | 0x24 | Used to securely register a joining device to a trust center. |
| BLE unlock request | 0x2C | Used to authenticate a connection on the BT interface and unlock the processing of AT command frames across this interface. |
| User data relay input | 0x2D | Used to relay user data between local interfaces. | 
| Secure session control | 0x2E | Used to control a secure session between a client and a server. |
| Local AT command response | 0x88 | This frame type is emitted in response to a local AT command request. |
| Modem status | 0x8A | This frame type is emitted in response to specific conditions. The status field of this frame indicates the device behavior. |
|Extended transmit status | 0x8B | This frame type is emitted when a network transmission request completes. The status field of this frame indicates whether the request succeeded or failed and the reason. |
| Transmit status | 0x89 | This frame type is emitted when a transmit request completes. |
| Receive packet | 0x90 | This frame type is emitted when a device configured with standard API output receives an RF data packet.|
| Explicit receive indicator | 0x91| This frame type is emitted when a device configured with explicit API output receives an RF data packet. |
| I/O sample indicator | 0x92 | This frame type is emitted when a device configured with standard API output receives an I/O sample from a remote device.|
| Node identification indicator | 0x95 | This frame type is emitted when a node identification broadcast is received. |
| Remote AT command response | 0x97 | This frame is emitted in response to a 0x17 (remote AT command) request. |
| Extended modem status | 0x98 | This frame type can be used to troubleshoot Zigbee network association |
| Route record indicator | 0xA1 | Contains the routing information for a remote device on the network |
| Registration status | 0xA4 | Emitted in response to registering a device to a trust center using the 0x24 (register joining device) frame. Indicates whether the registration attempt succeeded or not. |
| Many-to-one route request indicator | 0xA3 | Emitted on devices that receive a many-to-one route request from a network concentrator. |
| BLE unlock response | 0xAC | Emitted in response to a 0x2C (BLE unlock request)frame. |
| User data relay output | 0xAD | Emitted when user data is relayed to the serial prot from a local interface. |
| Secure session response |0xAE | This frame type is a response to a 0x2E (secure session control) frame. Indicates whether the secure session operation was successful or not. |

## How the library works 
XBeeAPIParser provides a clean interface for users to access and communicate with XBee devices in a network. The library accomplishes this through the use of multiple structures and key class variables, as outlined below. 

### Structures outside of the class 
* `apiFrame_t` represents a frame complete with frame type, length, and data buffer charactersitics. `apiFrame_t` objects also come with a library-defined (not in official Digi documentation) hexidecimal ID created by summing the ascii values of the two chars which represent the AT command being sent.  
* `frameBuffer_t` represents a frame buffer consisting of an array of `apiFrame_t` objects and a unsigned 16-bit integer length characteristic. 
* `partialFrame_t` contains a `apiFrame_t` as well as a char status and a received indicator. 

### Within the class 
* `_modem` is a BufferedSerial pointer used for serial data transfers.
* `_partialFrame` 
* `_frameBuffer`
* `__time_out` is an `int` representing a time quantitiy in ms, and is used throughout the code to measure the amount of time that the program is willing to sit around to wait for things to happen.
* `_failedTransmits` is an `int` initialized as zero and which is incremented with each case of a failed transmission 
* `_maxFailedTransmits` is an `int` initialized as 5, but is alterable. This value defines the maximum number of allowed failed transmissions. If this threshold is exceeded...
* `_isAssociated` is a `volatile bool` indicating if a device is joining the network (is associated)  
* `_frameBufferMutex` is a mutex protecting the shared frame buffer
* `_modemTxMutex` 
* `_updateBufferThread` is a thread in charge of updating the frame buffer 
* `_updateBufferThreadId` is the thread ID of the local thread (`_updateBufferThread`) in charge of updating the frame buffer 
* `_frameAlertThreadId` is the thread ID of the thread outside of the XBeeAPIParser which is controlling the XBee





