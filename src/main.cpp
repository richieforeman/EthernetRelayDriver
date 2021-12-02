#include <Arduino.h>
#include <SPI.h>         //Standard Library
#include <Ethernet.h>    //Standard Library
#include <EthernetUdp.h> //Standard Library
#include <avr/wdt.h>

// ref: https://github.com/forkineye/ESPAsyncE131/blob/master/ESPAsyncE131.h
// ref: https://tsp.esta.org/tsp/documents/docs/E1-31-2016.pdf
#define E131_UNIVERSE 0
#define E131_PORT 5568
#define E131_START_ADDRESS 126
#define E131_FRAME_UNIVERSE 113
#define E131_DMP_DATA 125
#define E131_DMP_COUNT 123
const uint8_t _E131_ACN_PACKET_IDENTIFIER[] = {0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00};

#define ETHERNET_BUFFER_MAX 640

#define SDCARD_CONTROL 4

#define RELAY_COUNT 8
#define OFF HIGH
#define ON LOW

#define RELAY_THRESHOLD_VALUE 127

int Relay[] = {
    42,
    40,
    38,
    36,
    34,
    32,
    30,
    28,
};

//Timer Setup
long packetsError = 0;
long packetsReceived = 0;

//Ethernet Configuration
//A8:61:0A:AE:81:80
byte mac[] = {0xA8, 0x61, 0x0A, 0xAE, 0x81, 0x80};
IPAddress ip(192, 168, 86, 5); //IP address of ethernet shield


// buffer to hold E1.31 data
unsigned char packetBuffer[ETHERNET_BUFFER_MAX];
EthernetUDP suUDP;

EthernetServer server(80);

void setup()
{
  delay(3000);
  Serial.begin(9600);
  wdt_enable(WDTO_4S);
  pinMode(SDCARD_CONTROL, OUTPUT);
  digitalWrite(SDCARD_CONTROL, HIGH);
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    pinMode(Relay[i], OUTPUT);
    digitalWrite(Relay[i], OFF);
    delay(200);
    digitalWrite(Relay[i], ON);
    delay(200);
    digitalWrite(Relay[i], OFF);
    wdt_reset();
  }
  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true)
    {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }

  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected.");
    while (true)
    {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }

  suUDP.begin(E131_PORT);

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  wdt_reset();
}

void sacnDMXReceived(unsigned char *frame, int count)
{
  if (frame[E131_FRAME_UNIVERSE] == E131_UNIVERSE)
  {
    if (frame[E131_DMP_DATA] == 0)
    {
      for (int i = 0; i < RELAY_COUNT; i++)
      {
        int value = frame[E131_START_ADDRESS + i];
        digitalWrite(Relay[i], value > RELAY_THRESHOLD_VALUE ? ON : OFF);
        wdt_reset();
      }
    }
  }
  else
  {
  }
}

//checks to see if packet is E1.31 data
int checkACNHeaders(unsigned char *frame, int messagelength)
{
  // TODO(richieforeman): We should really be comparing the whole ACN header
  if (frame[1] == 0x10 && frame[4] == _E131_ACN_PACKET_IDENTIFIER[0] && frame[12] == _E131_ACN_PACKET_IDENTIFIER[8])
  {
    int addresscount = frame[E131_DMP_COUNT] * 256 + frame[124];
    return addresscount - 1;
  }
  return 0;
}

// Loop for UDP packets, do fun things when one of them is E.131
void udpLoop()
{
  int packetSize = suUDP.parsePacket();
  if (packetSize)
  {
    packetsReceived++;
    suUDP.read(packetBuffer, ETHERNET_BUFFER_MAX);
    int count = checkACNHeaders(packetBuffer, packetSize);
    if (count == RELAY_COUNT)
    {
      sacnDMXReceived(packetBuffer, count);
    }
    else
    {
      packetsError++;
    }
  }
  wdt_reset();
}

void webStatus() {
  EthernetClient client = server.available();
  if (client)
  {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        if (c == '\n' && currentLineIsBlank)
        {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close"); // the connection will be closed after completion of the response
          client.println("Refresh: 5");        // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println(packetsReceived);
          client.println("/");
          client.println(packetsError);
          client.println("</html>");
          break;
        }
        if (c == '\n')
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void loop()
{
  udpLoop(); // Process UDP data before webserver stuff.
  webStatus();
}
