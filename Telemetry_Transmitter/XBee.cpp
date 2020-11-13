/* File: XBee.cpp
 * Author: Andrew Riachi
 * Date: 2020-10-15
 * Description: Implementation of the XBee class
 */
#include "XBee.h"
#include <Arduino.h>

packet::packet()
{
  // Helps prevent heap fragmentation (even though it's very possible in the rest of my code :/)
  frameData = new char[MAX_BUFFER_SIZE];
}

packet::~packet()
{
  delete[] frameData;
}

packet& packet::operator=(const packet& other)
{
  recvd_start = other.recvd_start;
  bytes_recvd = other.bytes_recvd;
  length = other.length;
  frameType = other.frameType;
  frameID = other.frameID;
  for (unsigned i = 0; i < (length-2) && i < MAX_BUFFER_SIZE; i++)
  {
    frameData[i] = '\0'; // clear the frameData
  }
  memcpy(frameData, other.frameData, length-2); // copy the new frameData over
  recvd_checksum = other.recvd_checksum;
  checksum = other.checksum;
}

bool XBee::configure(const String& server)
{
  m_serial.write("+++");
  delay(1500);
  String ok = m_serial.readStringUntil('\r');
  if (ok != "OK")
    return false;
  m_serial.write("ATAP 1\r");
  delay(2000);
  ok = "";
  ok = m_serial.readStringUntil('\r');
  if (ok != "OK")
    return false;
  m_serial.write("CN\r");
  return true;
}
void XBee::sendFrame(byte frameType, byte frameID, const char frameData[], size_t frameDataLen)
{
  uint16_t checksum = frameType + frameID;
  uint16_t len = frameDataLen + 2; // length of frameData + length of frameType + length of frameID
  for (uint16_t i = 0; i < frameDataLen; i++)
    checksum += frameData[i];
  checksum = 0xFF - checksum;

  const uint8_t start = 0x7E;
  byte len_msb = (uint8_t) (len >> 8);
  byte len_lsb = (uint8_t) len;

  SerialUSB.println("Length of frameData: " + String(len));
  SerialUSB.println("MSB: " + String(len_msb));
  SerialUSB.println("LSB: " + String(len_lsb));
  SerialUSB.println("checksum: " + String(checksum));

  m_serial.write(start);
  m_serial.write(len_msb);
  m_serial.write(len_lsb);
  m_serial.write(frameType);
  m_serial.write(frameID);
  m_serial.write(frameData, frameDataLen);
  m_serial.write(checksum);


  // debug
  /*
  m_serial.print(start);
  m_serial.print((int) (len_msb << 8) + len_lsb);
  m_serial.print(frameType);
  m_serial.print(frameData.c_str());
  m_serial.print(checksum);
  */
}

void XBee::sendATCommand(const char command[], const char param[], size_t paramLen)
{
  SerialUSB.println("Length of command: 2");
  SerialUSB.println("Length of param: " + String(paramLen));
  SerialUSB.println("Length of frame type: 1");
  char* temp = new char[2 + paramLen];
  memcpy(temp, command, 2);
  memcpy(temp+2, param, paramLen);
  sendFrame(0x08, 0x01, temp, 2+paramLen);
  delete[] temp;
}

void XBee::shutdown()
{
  sendATCommand("SD", (char) 0x00, 1);
}

bool XBee::shutdownCommandMode()
{
  m_serial.write("+++");
  delay(1500);
  String ok = m_serial.readStringUntil('\r');
  if (ok != "OK")
    return false;
  m_serial.write("ATSD 0\r");
  m_serial.setTimeout(30000);
  ok = "";
  ok = m_serial.readStringUntil('\r');
  m_serial.setTimeout(1000);
  if (ok != "OK")
    return false;
  m_serial.write("CN\r");
  return true;
}

userPacket XBee::read()
{
  // To save memory, I would like userPacket to contain references to m_rxBuffer.
  // Therefore, once we get a non-empty userPacket, we need it to be valid at least until the next call
  // to read().
  // So here, we'll check if our last call to read() received a full packet and if so, clear m_rxBuffer
  // to make room for a brand new packet. (This means userPacket will no longer have your previous packet!)
  if (m_rxBuffer.recvd_checksum)
  {
    m_rxBuffer = {};
  }
  // find the first field that hasn't been received yet.
  // if we don't have the full packet yet, return ""
  for (int recvd = m_serial.read(); (recvd != -1 && !m_rxBuffer.recvd_checksum); recvd = m_serial.read())
  {
    if (m_rxBuffer.bytes_recvd == 0)
    {
      m_rxBuffer.recvd_start = (recvd == 0x7E);
    }
    else if (m_rxBuffer.bytes_recvd == 1)
    {
      m_rxBuffer.length = recvd << 8;
    }
    else if (m_rxBuffer.bytes_recvd == 2)
    {
      m_rxBuffer.length += recvd;
    }
    else if (m_rxBuffer.bytes_recvd == 3)
    {
      m_rxBuffer.frameType = recvd;
    }
    else if (m_rxBuffer.bytes_recvd == 4)
    {
      m_rxBuffer.frameID = recvd;
    }
    else if (m_rxBuffer.bytes_recvd - 3 < m_rxBuffer.length) // The first three bits are not in the length
    {
      m_rxBuffer.frameData[m_rxBuffer.bytes_recvd - 3] = (char) recvd; // use a char here because that is probably what append is
      // defined for
    }
    else
    {
      m_rxBuffer.recvd_checksum = true;
      m_rxBuffer.checksum = recvd;
    }
    if (m_rxBuffer.recvd_start)
      (m_rxBuffer.bytes_recvd)++;
  }
  // if we are done looping because we received the checksum
  if (m_rxBuffer.recvd_checksum)
  {
    // verify the checksum
    uint8_t verify = m_rxBuffer.frameType + m_rxBuffer.frameID;
    for (unsigned i = 0; i < (m_rxBuffer.length - 2) && i < m_rxBuffer.MAX_BUFFER_SIZE; i++)
    {
      verify += (uint8_t) m_rxBuffer.frameData[i];
    }
    verify += m_rxBuffer.checksum;
    if (verify == 0xFF)
      return userPacket{m_rxBuffer.frameType, m_rxBuffer.frameData};
    else // poo poo packet, clear the buffer and return nothing to our poor user :(
      m_rxBuffer = {};
  }
  return NULL_USER_PACKET;
}
