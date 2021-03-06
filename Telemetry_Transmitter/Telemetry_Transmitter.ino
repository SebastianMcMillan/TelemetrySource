#include "DueCANLayer.h"
#include "XBee.h"
#include "MonitoredSerial.h"
#include "Stats.h"

const byte BYTE_MIN = -128;
const unsigned long DELAY = 5000;
byte maxTemp;
unsigned long nextTimeWeSendFrame;
MonitoredSerial mySerial(Serial1, Serial);
XBee xbee(mySerial);

const size_t REQUEST_BUFFER_SIZE = 488;
char requestBuffer[REQUEST_BUFFER_SIZE];

void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);
  
  delay(5000);
  if(xbee.configure())
    Serial.println("Configuration successful");
  else
    Serial.println("Configuration failed");
  maxTemp = -128;
  nextTimeWeSendFrame = 0;
  // Set the serial interface baud rate
    // Initialize both CAN controllers
  Serial.println("Test");  
  if(canInit(0, CAN_BPS_250K) == CAN_OK)
    Serial.print("CAN0: Initialized Successfully.\n\r");
  else
    Serial.print("CAN0: Initialization Failed.\n\r");


}

void loop()
{
  // sendMaxTempEveryFiveSeconds();
  printReceivedFrame();
  // sendStats();
  shutdownOnCommand();
}

void sendMaxTempEveryFiveSeconds()
{
  // Check for received message
  long lMsgID;
  bool bExtendedFormat;
  byte cRxData[8];
  byte cDataLen;
  if(canRx(0, &lMsgID, &bExtendedFormat, &cRxData[0], &cDataLen) == CAN_OK)
  {
    if (lMsgID == 0x6B1) {
      if (cRxData[4] > maxTemp)
        maxTemp = cRxData[4];
    }
  } // end if
  if (millis() >= nextTimeWeSendFrame)
  {
    nextTimeWeSendFrame += DELAY;
    printTemperature(maxTemp);
    maxTemp = BYTE_MIN;
  }
}

void printReceivedFrame()
{
  // xbee.read();
}

void shutdownOnCommand()
{
  if (Serial.read() == 's')
  {
    Serial.println("Shutting down, please wait about 30 seconds...");
    if (Serial.read() != 'c')
      xbee.shutdown(30000);
    else
    {
      if (xbee.shutdownCommandMode())
        Serial.println("Shutdown successful");
      else
        Serial.println("Shutdown failed");
    }
  }
}

void printTemperature(byte temp)
{
  Serial.print("High Temperature: ");
  Serial.print(temp);
  Serial.print("\n\r");
}

void sendStats(Stats stats)
{
  strcpy(requestBuffer, "POST /car HTTP/1.1\r\nContent-Length: 000\r\nHost: ku-solar-car-b87af.appspot.com\r\nContent-Type: application/json\r\nAuthentication: eiw932FekWERiajEFIAjej94302Fajde\r\n\r\n");
  strcat(requestBuffer, "{");
  int bodyLength = 2;
  for (int i = 0; i < StatData::_LAST; i++)
  {
    if (stats[i].present)
    {
      bodyLength += toKeyValuePair(requestBuffer + strlen(requestBuffer), stats[i].value); // append the key-value pair
      strcat(requestBuffer, ",");
    }
    strcat(requestBuffer, "}");
  }
  setContentLengthHeader(requestBuffer, bodyLength);

  xbee.sendTCP("216.58.192.212", 5000, 
}

void setContentLegnthHeader(char* dest, int len)
{
  char* contentLength = strstr(dest, "Content-Length: ") + 16;
  sprintf(contentLength, "%03u", len);
}

int toKeyValuePair(char* dest, StatData data)
{
  switch(data.name)
  {
    case StatData::BATT_VOLTAGE: return sprintf(dest, "\"battery_voltage\":%6f", data.doubleVal); break;
    case StatData::BATT_CURRENT: return sprintf(dest, "\"battery_current\":%6f", data.doubleVal); break;
    case StatData::BATT_TEMP: return sprintf(dest, "\"battery_temperature\":%6f", data.doubleVal); break;
    case StatData::BMS_FAULT: return sprintf(dest, "\"bms_fault\":%d", data.boolVal); break;
    case StatData::GPS_TIME: return sprintf(dest, "\"gps_time\":%6f", data.uIntVal); break;
    case StatData::GPS_LAT: return sprintf(dest, "\"gps_lat\":%6f", data.doubleVal); break;
    case StatData::GPS_LON: return sprintf(dest, "\"gps_lon\":%6f", data.doubleVal); break;
    case StatData::GPS_VEL_EAST: return sprintf(dest, "\"gps_velocity_east\":%6f", data.doubleVal); break;
    case StatData::GPS_VEL_NOR: return sprintf(dest, "\"gps_velocity_north\":%6f", data.doubleVal); break;
    case StatData::GPS_VEL_UP: return sprintf(dest, "\"gps_velocity_up\":%6f", data.doubleVal); break;
    case StatData::GPS_SPD: return sprintf(dest, "\"gps_speed\":%6f", data.doubleVal); break;
    case StatData::SOLAR_VOLTAGE: return sprintf(dest, "\"solar_voltage\":%6f", data.doubleVal); break;
    case StatData::SOLAR_CURRENT: return sprintf(dest, "\"solar_current\":%6f", data.doubleVal); break;
    case StatData::MOTOR_SPD: return sprintf(dest, "\"motor_speed\":%6f", data.doubleVal); break;
  }
}
