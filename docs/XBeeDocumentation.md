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
Below is a comprehensive list of the API frames. For more information, please visit [this link](). Note that the link provided gives information on frames specific to **XBee 3 modules ONLY**. There are other pages you can visit for information on Legacy S1 or XBee S2C Pro modules. 

| Name | Hex value | Description |
|--------|---------|-------------| 
| Local AT command request | 0x08 | 
| Queue local AT command request | 0x09
| Transmit request | 0x10
| Explicit adressing command request | 0x11
| Remote AT command request | 0x17 |
| Create source route | 0x21 |
| Register joining device | 0x24 |
| BLE unlock request | 0x2C |
| User data relay input | 0x2D
| Secure session control | 0x2E
| Local AT command response | 0x97 
| Extended modem status | 0x98 
| Route record indicator | 0xA1
| Registration status | 0xA4
| Many-to-one route request indicator | 0xA3
| BLE unlock response | 0xAC
| User data relay output | 0xAD
| Secure session response |0xAE

## How the library works 
XBeeAPIParser provides a clean interface for users to access and communicate with XBee devices in a network. The library accomplishes this through the use of multiple structures and key class variables, as outlined below. 

### Structures outside of the class 
* `apiFrame_t` represents a frame complete with frame type, length, and data buffer charactersitics. `apiFrame_t` objects also come with a library-defined (not in official Digi documentation) hexidecimal ID created by summing the ascii values of the two chars which represent the AT command being sent.  
* `frameBuffer_t` represents a frame buffer consisting of an array of `apiFrame_t` objects and a unsigned 16-bit integer length characteristic. 
* `partialFrame_t` contains a `apiFrame_t` as well as a char status and a received indicator. 

### Within the class 
* `_modem`
* `_partialFrame`
* `_frameBuffer`
* `__time_out`
* `_failedTransmits`
* `_maxFailedTransmits`
* `_isAssociated`
* `_frameBufferMutex`
* `_modemTxMutex`
* `_updateBufferThread`
* `_updateBufferThreadId`
* `_frameAlertThreadId`





