// This #include statement was automatically added by the Particle IDE.
#include "lifx.h"

#include "color.h"

// This #include statement was automatically added by the Particle IDE.
#include "neopixel.h"
/**
 * This is a minimal example, see extra-examples.cpp for a version
 * with more explantory documentation, example routines, how to
 * hook up your pixels and all of the pixel types that are supported.
 *
 * On Photon, Electron, P1, Core and Duo, any pin can be used for Neopixel.
 *
 * On the Argon, Boron and Xenon, only these pins can be used for Neopixel:
 * - D2, D3, A4, A5
 * - D4, D6, D7, D8
 * - A0, A1, A2, A3
 *
 * In addition on the Argon/Boron/Xenon, only one pin per group can be used at a time.
 * So it's OK to have one Adafruit_NeoPixel instance on pin D2 and another one on pin
 * A2, but it's not possible to have one on pin A0 and another one on pin A1.
 */

#include "Particle.h"
#include "neopixel.h"

const boolean DEBUG = 1;

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

uint16_t makeWord(uint16_t w);
uint16_t makeWord(byte h, byte l);
#define lowByte(w) ((w) & 0xff)
#define highByte(w) ((w) >> 8)
uint16_t makeWord(uint16_t w) { return w; }
uint16_t makeWord(uint8_t h, uint8_t l) { return (h << 8) | l; }
#define word(...) makeWord(__VA_ARGS__)

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN D2
#define PIXEL_COUNT 6
#define PIXEL_TYPE WS2812B

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

#define EEPROM_SIZE 256

unsigned int localPort = LifxPort;
UDP Udp;
TCPServer server = TCPServer(LifxPort);
TCPClient client;
bool wasConnected = false;
char message[128];
byte PacketBuffer[128]; //buffer to hold tcp incoming packet,
int rxError = 0;
byte mac[6];
byte site_mac[] = { 0x4c, 0x49, 0x46, 0x58, 0x56, 0x32 }; // spells out "LIFXV2" - version 2 of the app changes the site address to this...
/////////////////////////////////////////////
// Lifx Stuff
// label (name) for this bulb
char bulbLabel[LifxBulbLabelLength] = "CabinetRGB\0";
byte bulbGroup[16] = {
  1,3,5,7,8,1,2,3,0,0,0,0,0,0,0,0};
char bulbGroupLabel[32] = "MainGroup\0";
char bulbLocation[16] = {
  1,3,5,7,8,1,2,3,0,0,4,0,3,0,2,0};
char bulbLocationLabel[32] = "MyPlace\0";
// tags for this bulb
char bulbTags[LifxBulbTagsLength] = {
  0,0,0,0,0,0,0,0};
char bulbTagLabels[LifxBulbTagLabelsLength] = "";

// initial bulb values - warm white!
long power_status = 65535;
long hue = 0;
long sat = 0;
long bri = 65535;
long kel = 2000;
long dim = 0;

// end Lifx Stuff
/////////////////////////////////////////////


// Prototypes for local build, ok to leave in for Build IDE
void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);


/////////////////////////////////////////////
// setup
/////////////////////////////////////////////
void setup()
{
    
    Serial.begin();
    waitFor(Serial.isConnected, 30000);
    //Particle.publish("Starting");
    Serial1.begin(115200, SERIAL_8N1);
    Serial.println("Particle Started");
    Serial1.println("Particle Started");


    
      Serial.println("Wifi MAC: ");
      Serial1.println("Wifi MAC: ");
      WiFi.macAddress(mac);
      for (int i=0; i<6; i++) {
        Serial.printf("%02x%s", mac[i], i != 5 ? ":" : "");
        Serial1.printf("%02x%s", mac[i], i != 5 ? ":" : "");
      }
      Serial.printlnf("RSSI=%d", (int8_t) WiFi.RSSI());
      Serial1.printlnf("RSSI=%d", (int8_t) WiFi.RSSI());

      server.begin();

    doEEPROMStuff();
    
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
}


/////////////////////////////////////////////
// loop
/////////////////////////////////////////////
void loop()
{
  if (WiFi.ready()) {
      if (!wasConnected) {
            Serial1.println(WiFi.localIP());
            Serial1.printf ("Starting UDP on  %d\n", LifxPort);
            Udp.begin(LifxPort);
            wasConnected = true;
        }
        else {
            int count = Udp.receivePacket((byte*)message, 127);
            if (count >= 0 && count < 128) {
              message[count] = 0;
              rxError = 0;
            } else if (count < -1) {
              rxError = count;
              // need to re-initialize on error
              Udp.begin(LifxPort);
            }
            if (!rxError) {
              if (count > 0) {

                LifxPacket request((byte*)message, count);
                handleRequest(request);
                
                // clear the message buffer
                memset(message, 0, 127);
              }
            }
        }
        client = server.available();
        if (client == true) {// read incoming data
            int packetSize = 0;
            while (client.available()) {
                  byte b = client.read();
                  PacketBuffer[packetSize] = b;
                  packetSize++;
            }
            Serial1.println ("\nTCP message: ");
            Serial.println ((char*)PacketBuffer);
            Serial1.println ((char*)PacketBuffer);
            Serial1.println ("END TCP message\n");
                
            // push the data into the LifxPacket structure
            LifxPacket request(PacketBuffer, packetSize);
            //processRequest(PacketBuffer, packetSize, request);
        
            //respond to the request
            handleRequest(request);
            memset(PacketBuffer, 0, 128);
        }
   }
   else {
        wasConnected = false;
    }
}


/////////////////////////////////////////////
// doEEPROMStuff
/////////////////////////////////////////////
void doEEPROMStuff() {
      // read in settings from EEPROM (if they exist) for bulb label and tags
  if(EEPROM.read(EEPROM_CONFIG_START) == EEPROM_CONFIG[0]
    && EEPROM.read(EEPROM_CONFIG_START+1) == EEPROM_CONFIG[1]
    && EEPROM.read(EEPROM_CONFIG_START+2) == EEPROM_CONFIG[2]) {
      if(DEBUG) {
        Serial1.println(F("Config exists in EEPROM, reading..."));
        Serial1.print(F("Bulb label: "));
      }
  
      for(int i = 0; i < LifxBulbLabelLength; i++) {
        bulbLabel[i] = EEPROM.read(EEPROM_BULB_LABEL_START+i);
        
        if(DEBUG) {
          Serial1.print(bulbLabel[i]);
        }
      }
      
      if(DEBUG) {
        Serial1.println();
        Serial1.print(F("Bulb tags: "));
      }
      
      for(int i = 0; i < LifxBulbTagsLength; i++) {
        bulbTags[i] = EEPROM.read(EEPROM_BULB_TAGS_START+i);
        
        if(DEBUG) {
          Serial1.print(bulbTags[i]);
        }
      }
      
      if(DEBUG) {
        Serial1.println();
        Serial1.print(F("Bulb tag labels: "));
      }
      
      for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
        bulbTagLabels[i] = EEPROM.read(EEPROM_BULB_TAG_LABELS_START+i);
        
        if(DEBUG) {
          Serial1.print(bulbTagLabels[i]);
        }
      }
      
      if(DEBUG) {
        Serial1.println();
        Serial1.println(F("Done reading EEPROM config."));
      }
  } else {
    // first time sketch has been run, set defaults into EEPROM
    if(DEBUG) {
      Serial1.println(F("Config does not exist in EEPROM, writing..."));
    }
    
    EEPROM.write(EEPROM_CONFIG_START, EEPROM_CONFIG[0]);
    EEPROM.write(EEPROM_CONFIG_START+1, EEPROM_CONFIG[1]);
    EEPROM.write(EEPROM_CONFIG_START+2, EEPROM_CONFIG[2]);
    
    for(int i = 0; i < LifxBulbLabelLength; i++) {
       EEPROM.write(EEPROM_BULB_LABEL_START+i, bulbLabel[i]);
    }
    
    for(int i = 0; i < LifxBulbTagsLength; i++) {
      EEPROM.write(EEPROM_BULB_TAGS_START+i, bulbTags[i]);
    }
    
    for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
      EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i, bulbTagLabels[i]);
    }
      
    if(DEBUG) {
      Serial1.println(F("Done writing EEPROM config."));
    }
  }
  
  if(DEBUG) {
    Serial.println(F("EEPROM dump:"));
    for(int i = 0; i < 256; i++) {
      Serial1.print(EEPROM.read(i));
      Serial1.print(SPACE);
    }
    Serial1.println();
  }
}



/////////////////////////////////////////////
// rainbow
/////////////////////////////////////////////
void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

/////////////////////////////////////////////
// Wheel
/////////////////////////////////////////////
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}


/////////////////////////////////////////////
// sendPacket
/////////////////////////////////////////////
void sendPacket(LifxPacket &pkt) {
  if(DEBUG) {
    //Serial1.println("SendPacket ENTER");
  }
  sendUDPPacket(pkt);

  if(client.connected()) {
    sendTCPPacket(pkt);
  }
}


/////////////////////////////////////////////
// handleRequest
/////////////////////////////////////////////
void handleRequest(LifxPacket &request) {
//   if(DEBUG) {
//     Serial1.print(F("  Received packet type "));
//     Serial1.println(request.packet_type, HEX);
//   }

  uint32_t replySource = request.getHeaderSource();
  uint8_t replySequence = request.getHeaderSequence();


  switch(request.getPacketType()) {

  case GET_PAN_GATEWAY: 
    {
        Serial1.print(F("   Processing GET_PAN_GATEWAY\r\n"));
      // we are a gateway, so respond to this

      byte mac[6];
      WiFi.macAddress(mac);
      
      // respond with the UDP port
      byte UDPdata[] = { 
        SERVICE_UDP, //UDP
        lowByte(LifxPort), 
        highByte(LifxPort), 
        0x00, 
        0x00 
      };
      LifxPacket response1(PAN_GATEWAY, UDPdata, sizeof(UDPdata), replySource, replySequence);
      response1.setTarget(mac);
      sendPacket(response1);

      // respond with the TCP port details
      byte TCPdata[] = { 
        SERVICE_TCP, //TCP
        lowByte(LifxPort), 
        highByte(LifxPort), 
        0x00, 
        0x00 
      };
      LifxPacket response2(PAN_GATEWAY, TCPdata, sizeof(TCPdata), replySource, replySequence);
      response2.setTarget(mac);
      sendPacket(response2);
    } 
    break;


  case SET_LIGHT_STATE: 
    {
      Serial1.print(F("   Processing SET_LIGHT_STATE\r\n"));
    
      byte* data = request.getPayload();
      
    //   // set the light colors
      hue = word(data[2], data[1]);
      sat = word(data[4], data[3]);
      bri = word(data[6], data[5]);
      kel = word(data[8], data[7]);
// TODO SMR
      setLight();
    } 
    break;


  case GET_LIGHT_STATE: 
    {
      Serial1.print(F("   Processing GET_LIGHT_STATE\r\n"));
      // send the light's state
      byte StateData[] = { 
        lowByte(hue),  //hue
        highByte(hue), //hue
        lowByte(sat),  //sat
        highByte(sat), //sat
        lowByte(bri),  //bri
        highByte(bri), //bri
        lowByte(kel),  //kel
        highByte(kel), //kel
        lowByte(dim),  //dim
        highByte(dim), //dim
        lowByte(power_status),  //power status
        highByte(power_status), //power status
        // label
        lowByte(bulbLabel[0]),
        lowByte(bulbLabel[1]),
        lowByte(bulbLabel[2]),
        lowByte(bulbLabel[3]),
        lowByte(bulbLabel[4]),
        lowByte(bulbLabel[5]),
        lowByte(bulbLabel[6]),
        lowByte(bulbLabel[7]),
        lowByte(bulbLabel[8]),
        lowByte(bulbLabel[9]),
        lowByte(bulbLabel[10]),
        lowByte(bulbLabel[11]),
        lowByte(bulbLabel[12]),
        lowByte(bulbLabel[13]),
        lowByte(bulbLabel[14]),
        lowByte(bulbLabel[15]),
        lowByte(bulbLabel[16]),
        lowByte(bulbLabel[17]),
        lowByte(bulbLabel[18]),
        lowByte(bulbLabel[19]),
        lowByte(bulbLabel[20]),
        lowByte(bulbLabel[21]),
        lowByte(bulbLabel[22]),
        lowByte(bulbLabel[23]),
        lowByte(bulbLabel[24]),
        lowByte(bulbLabel[25]),
        lowByte(bulbLabel[26]),
        lowByte(bulbLabel[27]),
        lowByte(bulbLabel[28]),
        lowByte(bulbLabel[29]),
        lowByte(bulbLabel[30]),
        lowByte(bulbLabel[31]),
        //tags
        lowByte(bulbTags[0]),
        lowByte(bulbTags[1]),
        lowByte(bulbTags[2]),
        lowByte(bulbTags[3]),
        lowByte(bulbTags[4]),
        lowByte(bulbTags[5]),
        lowByte(bulbTags[6]),
        lowByte(bulbTags[7])
        };

      LifxPacket response(LIGHT_STATUS, StateData, sizeof(StateData), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case SET_POWER_STATE:
  case GET_POWER_STATE: 
    {
      // set if we are setting
      if(request.getPacketType() == SET_POWER_STATE) {
        Serial1.print(F("   Processing SET_POWER_STATE\r\n"));
        byte* data = request.getPayload();
        power_status = word(data[0], data[1]);
        setLight();
      }
      else {
          Serial1.print(F("   Processing GET_POWER_STATE\r\n"));
      }

      // respond to both get and set commands
      byte PowerData[] = { 
        lowByte(power_status),
        highByte(power_status)
        };

      LifxPacket response(POWER_STATE, PowerData, sizeof(PowerData), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case SET_BULB_LABEL: 
  case GET_BULB_LABEL: 
    {
      // set if we are setting
      if(request.getPacketType() == SET_BULB_LABEL) {
        Serial1.print(F("   Processing SET_BULB_LABEL\r\n"));
        byte* data = request.getPayload();
        for(int i = 0; i < LifxBulbLabelLength; i++) {
          if(bulbLabel[i] != data[i]) {
            bulbLabel[i] = data[i];
            EEPROM.write(EEPROM_BULB_LABEL_START+i, data[i]);
          }
        }
      }
      else {
          Serial1.print(F("   Processing GET_BULB_LABEL\r\n"));
      }

      LifxPacket response(BULB_LABEL, (byte*)bulbLabel, sizeof(bulbLabel), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case SET_BULB_TAGS: 
  case GET_BULB_TAGS: 
    {
      // set if we are setting
      if(request.getPacketType() == SET_BULB_TAGS) {
        Serial1.print(F("   Processing SET_BULB_TAGS\r\n"));
        byte* data = request.getPayload();
        for(int i = 0; i < LifxBulbTagsLength; i++) {
          if(bulbTags[i] != data[i]) {
            bulbTags[i] = lowByte(data[i]);
            EEPROM.write(EEPROM_BULB_TAGS_START+i, data[i]);
          }
        }
      }
      else {
          Serial1.print(F("   Processing GET_BULB_TAGS\r\n"));
      }

      LifxPacket response(BULB_TAGS, (byte*)bulbTags, sizeof(bulbTags), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case SET_BULB_TAG_LABELS: 
  case GET_BULB_TAG_LABELS: 
    {
      // set if we are setting
      if(request.getPacketType() == SET_BULB_TAG_LABELS) {
        Serial1.print(F("   Processing SET_BULB_TAG_LABELS\r\n"));
        byte* data = request.getPayload();
        for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
          if(bulbTagLabels[i] != data[i]) {
            bulbTagLabels[i] = data[i];
            EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i, data[i]);
          }
        }
      }
      else {
          Serial1.print(F("   Processing GET_BULB_TAG_LABELS\r\n"));
      }

      LifxPacket response(BULB_TAG_LABELS, (byte*)bulbTagLabels, sizeof(bulbTagLabels), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case GET_VERSION_STATE: 
    {
      Serial1.print(F("   Processing GET_VERSION_STATE\r\n"));
    //   // respond to get command
      byte VersionData[] = { 
        lowByte(LifxBulbVendor),
        highByte(LifxBulbVendor),
        0x00,
        0x00,
        lowByte(LifxBulbProduct),
        highByte(LifxBulbProduct),
        0x00,
        0x00,
        lowByte(LifxBulbVersion),
        highByte(LifxBulbVersion),
        0x00,
        0x00
        };


      LifxPacket response(VERSION_STATE, VersionData, sizeof(VersionData), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
      
      // SMR
      // respond again to get command (real bulbs respond twice, slightly diff data (see below)
    //   response.packet_type = VERSION_STATE;
    //   response.protocol = LifxProtocol_AllBulbsResponse;
    //   byte VersionData2[] = { 
    //     lowByte(LifxVersionVendor), //vendor stays the same
    //     highByte(LifxVersionVendor),
    //     0x00,
    //     0x00,
    //     lowByte(LifxVersionProduct*2), //product is 2, rather than 1
    //     highByte(LifxVersionProduct*2),
    //     0x00,
    //     0x00,
    //     0x00, //version is 0, rather than 1
    //     0x00,
    //     0x00,
    //     0x00
    //     };
    //   memcpy(response.data, VersionData2, sizeof(VersionData2));
    //   response.data_size = sizeof(VersionData2);
    //   sendPacket(response);
      
    } 
    break;


  case GET_MESH_FIRMWARE_STATE: 
    {
       Serial1.print(F("   Processing GET_MESH_FIRMWARE_STATE\r\n"));
    //   // respond to get command
 
      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte MeshVersionData[] = { 
        0x00, 0x2e, 0xc3, 0x8b, 0xef, 0x30, 0x86, 0x13, //build timestamp
        0xe0, 0x25, 0x76, 0x45, 0x69, 0x81, 0x8b, 0x13, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      LifxPacket response(MESH_FIRMWARE_STATE, MeshVersionData, sizeof(MeshVersionData), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;


  case GET_WIFI_FIRMWARE_STATE: 
    {
       Serial1.print(F("   Processing GET_WIFI_FIRMWARE_STATE\r\n"));
    //   // respond to get command

      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte WifiVersionData[] = { 
        0x00, 0xc8, 0x5e, 0x31, 0x99, 0x51, 0x86, 0x13, //build timestamp
        0xc0, 0x0c, 0x07, 0x00, 0x48, 0x46, 0xd9, 0x43, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      LifxPacket response(WIFI_FIRMWARE_STATE, WifiVersionData, sizeof(WifiVersionData), replySource, replySequence);
      response.setTarget(mac);
      sendPacket(response);
    } 
    break;
    
    
  case DEVICE_GET_LOCATION: 
    {
        Serial1.print(F("   Processing DEVICE_GET_LOCATION\r\n"));
        
        long timeNow_sec = Time.now() - 3600; // subtract an hour to avoid "this just happened right this second"

          byte locationData[56];
          uint8_t pktPtr = 0;
          for (int i=0;i<16;++i) {
              locationData[pktPtr] = bulbLocation[i];
              pktPtr++;
          }
          for (int i=0;i<32;++i) {
              locationData[pktPtr] = bulbLocationLabel[i];
              pktPtr++;
          }
          locationData[pktPtr] = (timeNow_sec & 0xff); pktPtr++;
          locationData[pktPtr] = ((timeNow_sec >> 2) & 0xff); pktPtr++;
          locationData[pktPtr] = ((timeNow_sec >> 4) & 0xff); pktPtr++;
          locationData[pktPtr] = ((timeNow_sec >> 8) & 0xff); pktPtr++;
          locationData[pktPtr] = 0; pktPtr++;
          locationData[pktPtr] = 0; pktPtr++;
          locationData[pktPtr] = 0; pktPtr++;
          locationData[pktPtr] = 0; pktPtr++;
    
          LifxPacket response(DEVICE_STATE_LOCATION, locationData, sizeof(locationData), replySource, replySequence);
          response.setTarget(mac);
          sendPacket(response);

        break;
    }
  case DEVICE_GET_GROUP: 
    {
        Serial1.print(F("   Processing DEVICE_GET_GROUP\r\n"));
        
        long timeNow_sec = Time.now() - 3600; // subtract an hour to avoid "this just happened right this second"

          byte groupData[56];
          uint8_t pktPtr = 0;
          for (int i=0;i<16;++i) {
              groupData[pktPtr] = bulbGroup[i];
              pktPtr++;
          }
          for (int i=0;i<32;++i) {
              groupData[pktPtr] = bulbGroupLabel[i];
              pktPtr++;
          }
          groupData[pktPtr] = (timeNow_sec & 0xff); pktPtr++;
          groupData[pktPtr] = ((timeNow_sec >> 2) & 0xff); pktPtr++;
          groupData[pktPtr] = ((timeNow_sec >> 4) & 0xff); pktPtr++;
          groupData[pktPtr] = ((timeNow_sec >> 8) & 0xff); pktPtr++;
          groupData[pktPtr] = 0; pktPtr++;
          groupData[pktPtr] = 0; pktPtr++;
          groupData[pktPtr] = 0; pktPtr++;
          groupData[pktPtr] = 0; pktPtr++;
         
    
          LifxPacket response(DEVICE_STATE_GROUP, groupData, sizeof(groupData), replySource, replySequence);
          response.setTarget(mac);
          sendPacket(response);

        break;
        
    }


  default: 
    {
      if(DEBUG) {
        Serial1.println(F("  Unknown packet type, ignoring\r\n"));
      }
    } 
    break;
  }
}


/////////////////////////////////////////////
// setLight
/////////////////////////////////////////////
void setLight() {
  if(DEBUG) {
    Serial1.print(F("Set light - "));
    Serial1.print(F("hue: "));
    Serial1.print(hue);
    Serial1.print(F(", sat: "));
    Serial1.print(sat);
    Serial1.print(F(", bri: "));
    Serial1.print(bri);
    Serial1.print(F(", kel: "));
    Serial1.print(kel);
    Serial1.print(F(", power: "));
    Serial1.print(power_status);
    Serial1.println(power_status ? " (on)" : "(off)");
  }

  if(power_status) {
    double this_hue = mapd(hue, 0, 65535, 0, 359);
    double this_sat = mapd(sat, 0, 65535, 0, 100);
    double this_bri = mapd(bri, 0, 65535, 0, 100);

    // if we are setting a "white" colour (kelvin temp)
    if(kel > 0 && this_sat < 1) {
      // convert kelvin to RGB
      rgb kelvin_rgb;
      kelvin_rgb = kelvinToRGB(kel);

      // convert the RGB into HSV
     // hsv kelvin_hsv;
      //kelvin_hsv = rgb2hsv(kelvin_rgb);

      // set the new values ready to go to the bulb (brightness does not change, just hue and saturation)
      //this_hue = kelvin_hsv.h;
      //this_sat = map(kelvin_hsv.s*1000, 0, 1000, 0, 255); //multiply the sat by 1000 so we can map the percentage value returned by rgb2hsv
      //this_sat = (kelvin_hsv.s*1000.0)*(100.0)/(1000.0);
      
      setStripColorRGB(this_hue, this_sat, this_bri);
    }
    else {
        setStripColor(this_hue, this_sat, this_bri);
    }

    
  } 
  else {
    setStripColor(0,0,0);
  }
}

double mapd(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


/////////////////////////////////////////////
// setStripColor
/////////////////////////////////////////////
void setStripColor(double h, double s, double v) {
    hsv hsvIn; hsvIn.h = h; hsvIn.s = s/100.0; hsvIn.v = v/100.0;
    rgb outColor = hsv2rgb(hsvIn);
    Serial1.printf("Setting LED Strip to RGB: %f %f %f\r\n",  outColor.r, outColor.g, outColor.b);
    
    byte r = floor(outColor.r*255);
    byte g = floor(outColor.g*255);
    byte b = floor(outColor.b*255);
    Serial1.printf("Setting LED Strip to RGB: %d %d %d\r\n", r, g, b);
    
    for(int i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}


/////////////////////////////////////////////
// setStripColor
/////////////////////////////////////////////
void setStripColorRGB(double rIn, double gIn, double bIn) {
    byte r = floor(rIn*255);
    byte g = floor(gIn*255);
    byte b = floor(bIn*255);
    Serial1.printf("Setting LED Strip to RGB: %d %d %d\r\n", r, g, b);
    
    for(int i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}


/////////////////////////////////////////////
// sendUDPPacket
/////////////////////////////////////////////
unsigned int sendUDPPacket(LifxPacket &pkt) { 
  // broadcast packet on local subnet
  IPAddress remote_addr(Udp.remoteIP());
  IPAddress broadcast_addr(remote_addr[0], remote_addr[1], remote_addr[2], 255);
  
  uint8_t tmpBuf;

  if (pkt.getHeaderSource() > 0) {
      Udp.beginPacket(remote_addr, Udp.remotePort());
  }
  else {
      Udp.beginPacket(broadcast_addr, Udp.remotePort());
  }
  

  Serial1.printf("\r\nSending Reply Packet\r\n");

  LifxPacketHeader hdr = pkt.getHeader();
  Udp.write((uint8_t*)&hdr,sizeof(LifxPacketHeader));
  
  byte* dataPtr = pkt.getPayload();
  for(int i = 0; i < pkt.getPayloadSize(); i++) {
    tmpBuf = dataPtr[i]; 
    Udp.write(&tmpBuf,1);
  }

  pkt.print();

  Udp.endPacket();

  return LifxPacketSize + pkt.getPayloadSize();
}

/////////////////////////////////////////////
// sendTCPPacket
/////////////////////////////////////////////
unsigned int sendTCPPacket(LifxPacket &pkt) { 

  byte TCPBuffer[128]; //buffer to hold outgoing packet,
  int byteCount = 0;

  LifxPacketHeader hdr = pkt.getHeader();
  byte* hdrPtr = (byte*)&hdr;
  for(int i=0; i<sizeof(LifxPacketHeader); ++i) {
      TCPBuffer[byteCount++] = *(hdrPtr+i);
  }
  byte* data = pkt.getPayload();
  for(int i=0; i<pkt.getPayloadSize(); ++i) {
      TCPBuffer[byteCount++] = *(data+i);
  }

  client.write(TCPBuffer, byteCount);

  return LifxPacketSize + pkt.getPayloadSize();
}


/////////////////////////////////////////////
// HSVtoRGB
/////////////////////////////////////////////
void HSVtoRGB(int H, double S, double V, int output[3]) {
	double C = S * V;
	double X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
	double m = V - C;
	double Rs, Gs, Bs;

	if(H >= 0 && H < 60) {
		Rs = C;
		Gs = X;
		Bs = 0;	
	}
	else if(H >= 60 && H < 120) {	
		Rs = X;
		Gs = C;
		Bs = 0;	
	}
	else if(H >= 120 && H < 180) {
		Rs = 0;
		Gs = C;
		Bs = X;	
	}
	else if(H >= 180 && H < 240) {
		Rs = 0;
		Gs = X;
		Bs = C;	
	}
	else if(H >= 240 && H < 300) {
		Rs = X;
		Gs = 0;
		Bs = C;	
	}
	else {
		Rs = C;
		Gs = 0;
		Bs = X;	
	}
	
	output[0] = (Rs + m) * 255;
	output[1] = (Gs + m) * 255;
	output[2] = (Bs + m) * 255;
}


void hsvToRgb2(double h, double s, double v, int output[3]) {
  double r, g, b;

  int i = floor(h * 6);
  double f = h * 6 - i;
  double p = v * (1 - s);
  double q = v * (1 - f * s);
  double t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
  }

   output[0] = r *255;
   output[1] = g *255;
   output[2] = b *255;

  return;
}
