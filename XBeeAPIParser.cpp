#include <mbed.h>
#include "XBeeAPIParser.h"

/**
 * @brief Construct a new XBeeAPIParser::XBeeAPIParser object from pointer
 * 
 * @param modem pointer to a BufferedSerial object connected to the XBee
 */
XBeeAPIParser::XBeeAPIParser(BufferedSerial* modem){ 
  // Since BufferedSerial is non-copyable, change assignment of 
  // tx, rx, and baud rate from constructor assignment to passing a pointer 
  // and assigning it to the XBeeAPIParser private BufferedSerial pointer
  _modem = modem; 
  _init();
}

/**
 * @brief Construct a new XBeeAPIParser::XBeeAPIParser object from pin names
 * 
 * @param tx TX pin linked to XBee
 * @param rx RX pin linked to XBee
 * @param baud 
 */
XBeeAPIParser::XBeeAPIParser(PinName tx, PinName rx, int baud = 921600){ 
  // Create a pointer to a BufferedSerial from pins
  _modem = new BufferedSerial(tx, rx, baud); 
  _init();
}

// Move all of the stuff common to the constructor methods to one place
void XBeeAPIParser::_init() {
  for (int i = 0; i < MAX_INCOMING_FRAMES; i++) {
    _frameBuffer.frame[i].type = 0xFF; // Set frame type to generic 
    _frameBuffer.frame[i].id = 0x00;
    _frameBuffer.frame[i].length = 0;
  }
  _frameBuffer.length = 0;
  _partialFrame.status = 0x00; // Set status to "all good"
  _time_out = 1000ms; // Timing variable used throughout 
  _isAssociated = false; 
  _failedTransmits = 0;
  _maxFailedTransmits = 5;
  _frameAlertThreadId = NULL; // Set the frame alert thread id to null
  _updateBufferThread.start(callback(this, &XBeeAPIParser::_move_frame_to_buffer)); // Start the update buffer thread and attach it to the move_frame_to_buffer function
  _modem->attach(callback(this, &XBeeAPIParser::_pull_byte), SerialBase::RxIrq);
}

bool XBeeAPIParser::associated() {
  if (!_isAssociated) {
    _verify_association(); // Check if the device is associated 
  }
  return _isAssociated;
}

/** 
 * Returns frame with specified frame type and frame ID. The frame is also 
 * removed from the buffer.
 * 
 * @returns true if match was found
 */
bool XBeeAPIParser::find_frame(char frameType, char frameID, apiFrame_t* frame) {
  if (_frameBufferMutex.trylock_for(_time_out)) { // Try to lock mutex for time given. true if the mutex was acquired, false otherwise.
    if (_frameBuffer.length>0) {
      for (int i = 0; i < _frameBuffer.length; i++) { // For each frame in the frame buffer 
        if ((_frameBuffer.frame[i].type == frameType) && (_frameBuffer.frame[i].id == frameID)) { // If the frame matches the specifications requested 
          // Set frame parameters 
          frame->type = frameType;
          frame->id = frameID;
          frame->length = _frameBuffer.frame[i].length;
          // Copy over frame data 
          for (int j = 0; j < frame->length; j++)
            frame->data[j] = _frameBuffer.frame[i].data[j];
          _frameBufferMutex.unlock(); // Now that all data has been copied from the buffer, unlock the mutex 
          _remove_frame_by_index(i); // Remove the frame from the buffer 
          return true;
        }
      }
    }
    _frameBufferMutex.unlock(); // If the frame buffer is empty, unlock the mutex 
  }
  return false;
}

/** 
 * Returns generic frame with specified frame type. The frame is also 
 * removed from the buffer.
 * 
 * @returns true if match was found
 */
bool XBeeAPIParser::find_frame(char frameType, apiFrame_t* frame) {
  return find_frame(frameType, 0xFF, frame);
}

/** 
 * Clears all frames of a specified type and ID from the frame buffer 
 */
void XBeeAPIParser::flush_old_frames(char frameType, char frameID) {
  apiFrame_t frame;
  while (find_frame(frameType, frameID, &frame)); // Find all frames with the specified ID and type and clear them from the frame buffer 
}


/** 
 * @returns 0 if response frame not found | 1 if response frame's status code is 1 (ERROR), 2 (INVALID CMD), or 3 (INVALID PARAMETER) 
 * | 64-bit address if successful
 */
uint64_t XBeeAPIParser::get_address(string ni) {
  uint64_t address;
  Timer t; // Initialize timer 
  char frameID;
  char status;
  apiFrame_t frame;
  int len;
  bool foundFrame;
  _make_AT_frame("DN", ni, &frame); // Make local AT command frame and set command to destination node 
  flush_old_frames(frame.type, frame.id); // Clear old DN frames 
  frameID = frame.id; // Collect frame id (0x92 for DN)
  send(&frame); // Send the DN frame 
  ThisThread::sleep_for(5ms);
  t.start(); // Start timer 
  foundFrame = false;
  // For 10 times longer than the single-step timeout or until one is found, search for local at command response frames in the frame buffer 
  while ((t.elapsed_time() < 10*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame); // Search for local AT command response frames (0x88)
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (!foundFrame) printf("Timed out after DN!\r\n"); // If not successful in finding the response frame
  if ((!foundFrame) || (frame.length != 3)) return 0; // If the frame was not found or has insufficient data, return 0 
  if (!((frame.data[0] = 'D') && (frame.data[1] == 'N') && (frame.data[2]==0))) return 1; // If the response frame command is incorrect or the status code is not 0 (OKAY), return 1
  // If everything is okay with the response frame, move on to collecting the 64-bit destination address 
  // Local AT command frames can be used to collect the address by combining the DH and DL AT commands 

  // Begin with DH, which is used to read the upper 32 bits of the 64-bit adress 
  // make local AT command frame and set command to Desitnation Address High
  _make_AT_frame("DH", &frame); 
  flush_old_frames(frame.type, frame.id); // Clear old DH frames 
  frameID = frame.id; // Collect frame id (0x8C for DH)
  send(&frame); // Send DH frame 
  t.reset(); // Reset timer 
  foundFrame = false;
  // For 2 times the single-step timeout or until it is found, attempt to find the response frame (0x88)
  while ((t.elapsed_time() < 2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (!foundFrame) printf("Timed out after DH!\r\n"); // If not successful in finding the response frame in time
  if ((!foundFrame) || (frame.length != 7)) return 0; // If the frame was not found or has insufficient data, return 0 
  // If nothing went wrong with getting the response frame 
  address = 0; // Clear address to prep for loading 
  // Collect data from response frame 
  for (int i = 0; i < 4; i++) {
    address = (address << 8) | frame.data[3+i];
  }
  // To get the second half of the address, send a DL command frame 
  _make_AT_frame("DL", &frame);
  flush_old_frames(frame.type, frame.id); // Clear old DL frames 
  frameID = frame.id; // Collect frame id (0x90 for DL)
  send(&frame); // Send DL frame 
  t.reset(); // Reset timer 
  foundFrame = false;
  // For 2 times the single-step timeout or until it is found, attempt to find the response frame (0x88)
  while ((t.elapsed_time() <2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (!foundFrame) printf("Timed out after DL!\r\n"); // If not successful in finding the response frame in time 
  if ((!foundFrame) || (frame.length != 7)) return 0; // If the frame was not found or has insufficient data, return 0 
  // If nothing went wrong with getting the response frame, collect the lower 32 bits of the address 
  for (int i = 0; i < 4; i++) {
    address = (address << 8) | frame.data[3+i];
  }
  return address; // Return full 64-bit address 
}

/** 
 * @returns 
 * RSSI (Received Signal Strength Indicator) in -dBm of the last received FR data packet 
 * | 0 if the XBee has been restarted and has not yet received a packet | 0xFF if the response 
 * does not come through 
 */
char XBeeAPIParser::last_RSSI() {
  Timer t;
  char frameID;
  char rssi = 0xFF;
  apiFrame_t frame;
  bool foundFrame;
  _make_AT_frame("DB", &frame); // Make DB frame 
  frameID = frame.id; // Collect id 
  send(&frame); // Send DB frame 
  t.start(); // Start timer 
  foundFrame = false;
  // For 2 times the single-step timeout or until it is found (whichever is shorter), attempt to find the response frame  
  while ((t.elapsed_time() <2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (!foundFrame) return 0xFF; // If the frame is not found, return 0xFF
  if (frame.length == 6) { // If the frame has sufficient data and the command status is good, collect the RSSI
    if ((frame.data[2]=='D') && (frame.data[3]=='B')&& (frame.data[4]==0)) {
      rssi = frame.data[5];
    }
  }
  return rssi; // Return RSSI value 
}

/** 
 * Checks if there are any frames in the frame buffer 
 * 
 * @returns true if there are frames in the frame buffer, false otherwise
 */
bool XBeeAPIParser::readable() {
  bool hasFrames = false;
  if (_frameBufferMutex.trylock_for(_time_out)) {
    hasFrames = _frameBuffer.length > 0;
    _frameBufferMutex.unlock();
  }
  return hasFrames;
}

/** 
 * Returns the frame in the first index of the frame buffer 
 * 
 * @returns true if successful
 */
bool XBeeAPIParser::get_oldest_frame(apiFrame_t* frame) {
  // Try to lock the frame buffer mutex for single-step timeout
  if (_frameBufferMutex.trylock_for(_time_out)) {
    if (_frameBuffer.length > 0) { // If the frame buffer has data
      // Set frame parameters
      frame->type = _frameBuffer.frame[0].type;
      frame->id = _frameBuffer.frame[0].id;
      frame->length = _frameBuffer.frame[0].length;
      // Copy frame data 
      for (int i = 0; i < frame->length; i++)
        frame->data[i] = _frameBuffer.frame[0].data[i];
      _frameBufferMutex.unlock(); // Free the mutex 
      _remove_frame_by_index(0); // Remove the frame from the buffer 
      return true; // Since successful, return true 
    } 
    _frameBufferMutex.unlock(); //  If the frame buffer has no data, free the mutex 
  }
  return false; // Return flase if mutex does not lock within the given window of time 
}

/** 
 * Checks if the receive packet frame has been emitted. 
 * Note that 0x90 (receive packet frame type) is emitted when 
 * a device in standard API mode receives an RF data packet 
 * 
 * @returns unknown 
 */
int XBeeAPIParser::rxPacket(char* payload, uint64_t* address) {
  apiFrame_t frame; // Create a blank frames 
  bool foundFrame; 
  uint64_t who = 0;
  foundFrame = find_frame(0x90, &frame); // Find a receive packet (0x90) frame in the frame buffer 
  if (foundFrame) { // If the receive packet frame is found 
    for (int i = 0; i < 8; i++) {
      who = (who << 8) | frame.data[i]; // Copy over the 64-bit source address (the sender's address)
    }
    *address = who; // Set address 
    for (int i = 0; i < (frame.length-9); i++) {
      payload[i] = frame.data[i+11]; // Copy over the received data 
    }
    return (frame.length-11); // Not sure why we are pulling from this particular index 
  } else return 0;
}

/** 
 * Writes out a given frame 
 * 
 * @returns true if successful, else false 
 */
bool XBeeAPIParser::send(apiFrame_t* frame) {
  bool success = true;
  char c;

  // To calculate the checksum of an API frame 
  // 1) Add all bytes of the packet except for the start delimiter 0x7E and the length
  // 2) Keep only the lowest 8 bits from the result 
  // 3) Subtract from 0xFF
  uint32_t checksum = frame->type; // Start with frame type
  checksum = checksum + frame->id; // Add the first two data bytes
  for (int i = 0; i < frame->length; i++) { // Sum all other bytes 
    checksum = checksum + frame->data[i];
  }
  checksum = checksum & 0xFF; // Keep only 8 lowest bits 
  checksum = 0xFF - checksum; // Subtract from 0xFF

  Timer t;
  t.start(); // Start timer; entire send must complete before _time_out
  if (_modemTxMutex.trylock_for(_time_out)) { 
    while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {}
    if (_modem->writable() && success) { // write the start delimiter (0x7E)
      c = 0x7E;
      _modem->write(&c, 1);
    } else success = false;

    while (t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {} 
    if (_modem->writable() && success) {
      c = (frame->length+2) >> 8; // Write length MSB
      _modem->write(&c, 1);
    } else success = false;
    
    while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {} 
    if (_modem->writable() && success) {
      c = (frame->length+2) & 0xFF; // Write length LSB 
      _modem->write(&c, 1);
    } else success = false;
    
    while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {}
    if (_modem->writable() && success) {
      c = frame->type; // Write frame type 
      _modem->write(&c, 1);
    } else success = false;

    while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {} 
    if (_modem->writable() && success) {
      c = frame->id; // Write frame ID 
      _modem->write(&c, 1);
    } else success = false;

    for (int i = 0; i < frame->length; i++) {
      while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {}  
      if (_modem->writable() && success) {
        c = frame->data[i]; // Write each byte of the data, one at a time 
        _modem->write(&c, 1);
      } else success = false;
    }

    while ((t.elapsed_time() < _time_out) && (!_modem->writable()) && success) {}
    if (_modem->writable() && success) {
      c = checksum; // Write the final byte (checksum) 
      _modem->write(&c, 1);
    } else success = false;
    _modemTxMutex.unlock(); 
  }
  return success; // Return success boolean 
}

void XBeeAPIParser::set_frame_alert_thread_id(osThreadId_t threadID) {
  _frameAlertThreadId = threadID;
}

void XBeeAPIParser::set_max_failed_transmits(int maxFails) {
  if ((maxFails>0) && (maxFails<20)) _maxFailedTransmits = maxFails;
}

void XBeeAPIParser::set_timeout(std::chrono::milliseconds t) {
  if ((t >= 1ms) && (t < 5s)) _time_out = t;
}

// ???
int XBeeAPIParser::txAddressed(uint64_t address, char* payload, int len) {
  if (len>(MAX_FRAME_LENGTH)) return -1;
  apiFrame_t frame;
  bool foundFrame;
  char frameID = 0x00;
  for (int i = 0; i < len; i++) {
    frameID = frameID + payload[i];
  }
  frame.type = 0x00; // Tx Request Frame Type
  frame.id = frameID;
  for (int i = 0; i < 8; i++) {
    frame.data[i] = (address >> ((7-i)*8)) & 0xFF;
  }
  frame.data[8] = 0x00;  // No options
  for (int i = 0; i < len; i++) {
    frame.data[9+i] = payload[i];
  }
  frame.length = len + 9;
  send(&frame);
  Timer t;
  t.start();
  foundFrame = false;
  ThisThread::sleep_for(7ms);
  while ((t.elapsed_time() < 2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x89, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(7ms);
  }
  if (foundFrame) {
    if (frame.data[0] == 0x00) {
      _failedTransmits = 0;
      return 0;
    } else {
      _failedTransmits++;
      if (_failedTransmits>= _maxFailedTransmits) {
        _disassociate();
        _failedTransmits = 0;
        return -2;
      }
    }
  }
  return -3;
}

int XBeeAPIParser::txBroadcast(char* payload, int len) {
  return txAddressed(0xFFFF, payload, len);
}

void XBeeAPIParser::_disassociate() {
  Timer t;
  apiFrame_t frame;
  bool foundFrame;
  _make_AT_frame("DA", &frame);
  char frameID = frame.id;
  send(&frame);
  t.start();
  foundFrame = false;
  while ((t.elapsed_time() < 2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (foundFrame) {
    if ((frame.data[0]=='D') && (frame.data[1]=='A') && (frame.data[2]==0)) {
      _isAssociated = false;
    }
  }
}

void XBeeAPIParser::_make_AT_frame(string cmd, apiFrame_t* frame) {
  _make_AT_frame(cmd, "", frame);
}

/** 
 * Makes a local AT command request frame (0x08)
 * This frame type is used to query or set command parameters on the local device 
 */
void XBeeAPIParser::_make_AT_frame(string cmd, string param, apiFrame_t* frame) {
  frame->type = 0x08; // Local AT command frame
  if (cmd.length()==2) { 
    // Set AT command
    frame->data[0] = cmd[0]; 
    frame->data[1] = cmd[1]; 
  }
  frame->id = cmd[0] + cmd[1]; // Set the frame id to the sum of the first two characters in the given string
  // The frame id identifies the data frame for the host to correlate with a subsequent response 
  if (param.length()>0) { // Only enter this if a parameter is explicitly given
    for (int i = 0; i < param.length(); i++)
      frame->data[2+i] = param[i];
  }
  frame->length = 2 + param.length(); // Set the length of the data 
}

/** The NEW way forward
while (true) {
  while (_modem->readable() and other stuff) {
    existing stuff happens;
  }
  if (frame ready to move) {
    do the frame moving stuff
  }
  ThisThread::sleep_for(10ms);
}
*/


void XBeeAPIParser::_pull_byte() {
  char buff;
  uint16_t len;
  uint32_t checksum = 0;
  while (_modem->readable() && (_partialFrame.status < 0x06)) {
    buff = _modem->getc();
    switch (_partialFrame.status) {
      case 0x00:  // Waiting for start of new frame
        if (buff == 0x7E) {  // Frame start byte should be 0x7E
          _partialFrame.status = 0x01;
        }
        break;
      case 0x01:  // First frame length byte
        _partialFrame.frame.length = buff << 8;
        _partialFrame.status = 0x02;
        break;
      case 0x02: // Second frame length byte
        _partialFrame.frame.length = (_partialFrame.frame.length | buff)-2;
        _partialFrame.rcvd = 0;
        _partialFrame.status = 0x03;
        break;
      case 0x03: // Frame type
        _partialFrame.frame.type = buff;
        switch (buff) { // frame types with FrameID
          case 0x00: _partialFrame.status = 0x04; break;
          case 0x08: _partialFrame.status = 0x04; break;
          case 0x17: _partialFrame.status = 0x04; break;
          case 0x88: _partialFrame.status = 0x04; break;
          case 0x89: _partialFrame.status = 0x04; break;
          case 0x97: _partialFrame.status = 0x04; break;
          default: // No Frame ID for this type
            _partialFrame.frame.id = 0xFF;
            _partialFrame.frame.length++;
            _partialFrame.status = 0x05;
        }
        break;
      case 0x04: // Frame ID
        _partialFrame.frame.id = buff;
        _partialFrame.status = 0x05;
        break;
      case 0x05:
        len = _partialFrame.frame.length;
        if (len > MAX_FRAME_LENGTH) { // Incoming frame won't fit!
          _partialFrame.status = 0x00;
        } else if (_partialFrame.rcvd < len) {  // Waiting for rest of frame
          _partialFrame.frame.data[_partialFrame.rcvd] = buff;
          _partialFrame.rcvd++;
        } else { // This should be the checksum
          switch (_partialFrame.frame.type) { // frame types with FrameID
            case 0x00: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            case 0x08: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            case 0x17: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            case 0x88: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            case 0x89: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            case 0x97: checksum = _partialFrame.frame.type + _partialFrame.frame.id; break;
            default: // No Frame ID for this type
              checksum = _partialFrame.frame.type;
          }
          for (int i = 0; i < len; i++)
            checksum = checksum + _partialFrame.frame.data[i];
          checksum = (checksum + buff) & 0xFF;
          if (checksum == 0xFF) { // Frame is good!  Save to buffer.
            if (_partialFrame.frame.type == 0x8A) { // Intercept modem status frames
              switch (_partialFrame.frame.data[0]) {
                case 0x02:
                  _isAssociated = true;
                  _failedTransmits = 0;
                  break;
                case 0x06:
                  _isAssociated = true;
                  _failedTransmits = 0;
                  break;
                default:
                  _isAssociated = false;
              }
              _partialFrame.status = 0x00;
            } else {
              _modem->attach(NULL); // Stop interrupts on serial input
              _partialFrame.status = 0x06;
              _updateBufferThread.flags_set(0x01); // Trigger copy to frame buffer outside of ISR
            }
          } else { // Checksum doesn't match.  Bad frame!
            _partialFrame.status = 0x00; // There should be some error signaling
          }
        }
        break;
      case 0x06:
        break;
    }
  }
}

void XBeeAPIParser::_move_frame_to_buffer() {
  while (true) {
    ThisThread::flags_wait_all(0x01);
    if (_frameBufferMutex.trylock_for(5*_time_out)) {
      if (_frameBuffer.length == MAX_INCOMING_FRAMES) {  // Buffer full, drop oldest frame
        _remove_frame_by_index(0);
      }
      int n = _frameBuffer.length; // Save current length for ease of copying to buffer
      _frameBuffer.frame[n].id = _partialFrame.frame.id;
      _frameBuffer.frame[n].type = _partialFrame.frame.type;
      _frameBuffer.frame[n].length = _partialFrame.frame.length;
      for (int i = 0; i < _partialFrame.frame.length; i++)
        _frameBuffer.frame[n].data[i] = _partialFrame.frame.data[i];
      _frameBuffer.length++;
      _frameBufferMutex.unlock();
      _partialFrame.status = 0x00;
      if (_frameAlertThreadId) osSignalSet(_frameAlertThreadId, 0x01); 
      _modem->attach(callback(this,&XBeeAPIParser::_pull_byte),SerialBase::RxIrq);
    }
  }
}


/** 
 * Remove the frame at the given index from the frame buffer 
 */
void XBeeAPIParser::_remove_frame_by_index(int n) {
  if (n< _frameBuffer.length) { // Only proceed if the given index is valid 
    for (int i = n + 1; i < _frameBuffer.length; i++) {
      for (int j = 0; j < _frameBuffer.frame[i].length; j++) {
        _frameBuffer.frame[i-1].data[j] = _frameBuffer.frame[i].data[j];
      }
      _frameBuffer.frame[i-1].type = _frameBuffer.frame[i].type;
      _frameBuffer.frame[i-1].id = _frameBuffer.frame[i].id;
      _frameBuffer.frame[i-1].length = _frameBuffer.frame[i].length;
    }
  }
  if (_frameBuffer.length>0) _frameBuffer.length--;
}

void XBeeAPIParser::_verify_association() {
  Timer t; // Create timer object 
  apiFrame_t frame; // Create frame object 
  bool foundFrame;
  char status = 0xFE; // Set the status
  _make_AT_frame("AI", &frame);  // Make local AT command frame and set command to association indication  
  char frameID = frame.id; // Collect the frame id (0x8A for AI)
  _isAssociated = false; // Set default value to false 
  foundFrame = false; // Set default value to false 
  send(&frame); // Write out the frame 
  t.start(); // Start the timer 
  while ((t.elapsed_time() < 2*_time_out) && (!foundFrame)) {
    foundFrame = find_frame(0x88, frameID, &frame);
    if (!foundFrame) ThisThread::sleep_for(5ms);
  }
  if (foundFrame) { // If the frame is found 
    if ((frame.data[0]=='A') && (frame.data[1]=='I') && (frame.data[2]==0)) {
      status = frame.data[3]; // Collect the modem status from the fourth index of the frame
    }
  }
  if (status == 0x00) _isAssociated = true; // If status is 0x00 (end device successfully associated), return true 
}



