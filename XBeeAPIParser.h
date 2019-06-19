/** XBee API buffered frame parser
 * 
 *  @author John M. Larkin (jlarkin@whitworth.edu)
 *  @version 0.1
 *  @date 2019
 *  @copyright MIT License
 */

#ifndef XBEE_API_PARSER_H
#define XBEE_API_PARSER_H

#include "mbed.h"
#include "rtos.h"
#include <string> 
using namespace std;

#define MAX_INCOMING_FRAMES 10
#define MAX_FRAME_LENGTH 150

typedef struct {
    char type;
    char id;
    uint16_t length;
    char data[MAX_FRAME_LENGTH];
} apiFrame_t;

typedef struct {
    uint16_t length;
    apiFrame_t frame[MAX_INCOMING_FRAMES];
} frameBuffer_t;

typedef struct {
    apiFrame_t frame;
    char status;
    int rcvd;
} partialFrame_t;

class XBeeAPIParser
{
private:
    RawSerial _modem;
    volatile partialFrame_t _partialFrame;
    volatile frameBuffer_t _frameBuffer;
    int _time_out;
    int _failedTransmits;
    int _maxFailedTransmits;

    volatile bool _isAssociated;

    // RTOS management
    // Mutex _partialFrameMutex;
    Mutex _frameBufferMutex;
    Mutex _modemTxMutex;
    // Thread _rxThread;

    void _pull_byte();
    void _verify_association();
    void _disassociate();
    void _remove_frame_by_index(int n);
    void _make_AT_frame(string cmd, apiFrame_t* frame);
    void _make_AT_frame(string cmd, string param, apiFrame_t* frame);

public:
    XBeeAPIParser(PinName tx, PinName rx, int baud = 921600);
    bool readable();
    bool associated();
    bool send(apiFrame_t* frame);
    bool get_oldest_frame(apiFrame_t* frame);
    bool find_frame(char frameType, char frameID, apiFrame_t* frame);
    bool find_frame(char frameType, apiFrame_t* frame);
    int txAddressed(uint64_t address, char* payload, int len);
    int txBroadcast(char* payload, int len);
    int rxPacket(char* payload, uint64_t* address);
    void set_timeout(int time_ms);
    void set_max_failed_transmits(int maxFails);
    char last_RSSI();
    uint64_t get_address(string ni);
};

#endif