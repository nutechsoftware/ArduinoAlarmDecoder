 /**
 *  @file    AD2EmbeddedIoT.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/15/2020
 *  @version 1.0
 *
 *  @brief AlarmDecoder IOT embedded network appliance
 *
 *  @copyright Copyright (C) 2020 Nu Tech Software Solutions, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

// AlarmDecoder arduino parsing state machine.
// https://github.com/nutechsoftware/ArduinoAlarmDecoder
#include <ArduinoAlarmDecoder.h>

/**
 * Include our secrets file. Must create this file to build the project.
 * 
 * Inside of the Arduino  IDE create a new TAB and call it secrets.h
 * Next add the next two lines to this file and save the file.
#define SECRET_SSID "Your SSID Here"
#define SECRET_PASS "Your PASSWORD here"
 */
#include "secrets.h"

/**
 * Hardware setup
 * FIXME: needs design work.
 */
#define HW_ESP32_EVB_EA
//#define HW_ESP32_TTGO_TBEAM10
//#define HW_ESP32_THING

// OLIMAX ESP32-EVB-EA HARDWARE SETTINGS
// Connect the AD2* TX PIN to ESP32 RX PIN and AD2* RX PIN to ESP32 TX PIN
#ifdef HW_ESP32_EVB_EA
 // Use UART2 on ESP32
 //#define USE_ESP32_UART2
 #ifndef USE_ESP32_UART2
  #warning Using ESP32 software uart on AD2TX->UEXT:RXD(4):GPIO4 UEXT:TXD(3):GPIO36->AD2RX
  // UEXT TXD(3)/RXD(4) are on not connected to the ESP32 hardware UART.
  #define AD2_TX 4  // UEXT PIN 4
  #define AD2_RX 36 // UEXT PIN 3
 #else
  #warning Using ESP32 hardware uart AD2TX->UEXT:SCL(5):GPIO16 UEXT:SSEL(10):GPIO17->AD2RX
  #define AD2_TX 16 // UEXT PIN  5
  #define AD2_RX 17 // UEXT PIN 10
 #endif
#endif


// Default AD2* BAUD is 115200 but can be adjusted by jumpers.
#define AD2_BAUD 115200

/**
 *  Enable device features/service
 *  
 *  It may not be possible to enable all at the same time.
 *  This will depend on the hardware code and ram space.
 */
#define EN_ETH
#define EN_WIFI
#define EN_MQTT_CLIENT
//#define EN_REST_HOST
//#define EN_SSDP

/**
 * Arduino/Espressif built in support for LAN87XX chip
 */
#ifdef EN_ETH
#include <ETH.h>
// ETH Constants
/// FIXME: Static IP/DHCP
#endif

/** 
 * Arduino/Espressif built in support for WiFi 
 */
#ifdef EN_WIFI
#include <WiFi.h>
/// WIFI Constants
const char* ssid        = SECRET_SSID;
const char* password    = SECRET_PASS;
#endif // EN_WIFI


/** 
 * Test code defines
 */
#if defined(EN_ETH) + defined(EN_WIFI) == 0
#error Must define at least one network interface
#endif


/** 
 * Common global/static 
 */
 
// used by both Ethernet and Wifi arduino drivers
const char* host_name   = "AD2ESP32";
WiFiClient espClient;
static bool eth_connected = false;
static bool wifi_connected = false;

// time/delay tracking
static uint32_t time_now = 0;

// raw mode allows direct access to the AD2* device and disables internal processing.
static bool raw_mode = false;

// MAC address must be unique on the local network.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };

// IP address in case DHCP fails
IPAddress dhcp_fail_ip(169,254,0,123);
IPAddress dhcp_fail_gw(169,254,0,1);
IPAddress dhcp_fail_subnet(255,255,0,0);

// Static IP or 0,0,0,0 for DHCP
IPAddress static_ip(0,0,0,0);
IPAddress static_gw(169,254,0,1);
IPAddress static_subnet(255,255,0,0);

// AlarmDecoder parser
AlarmDecoderParser AD2Parse;


/** 
 *  MQTT client support
 */
#ifdef EN_MQTT_CLIENT
// PubSubClient v2.6.0 by Nick O'Leary
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// MQTT DEFINES
#define MQTT_CONNECT_RETRY_INTERVAL 5000   // every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL 60000   // every 60 seconds

// MQTT Constants
const char* mqtt_server = "test.mosquitto.org";
//const char* mqtt_server = "5.196.95.208";

// MQTT globals
PubSubClient mqttClient(espClient);
String mqtt_clientId;

#endif // EN_MQTT_CLIENT



/**
 * Arduino Sketch setup()
 *   Called once after hardware powers on.
 */
void setup()
{
  // small delay on startup
  delay(5000);
  
  // Open host UART
  Serial.begin(115200);
  Serial.println();
  Serial.println("!DBG:AD2EMB,Starring");

  // Open AlarmDecoder UART
  Serial2.begin(AD2_BAUD, SERIAL_8N1, AD2_TX, AD2_RX);
  // The ESP32 uart driver has its own interrupt and buffers for processing
  // rx bytes. Give it plenty of space. 1024 gave about 1 minute storage of
  // normal messages from AD2 on Vista 50PUL panel with one partition.
  // If any loop() method is busy too long alarm panel state data will be lost.
  Serial2.setRxBufferSize(2048);

#ifdef EN_ETH
  // Start ethernet
  ETH.begin();

  // Static IP or DHCP?
  if (static_ip != (uint32_t)0x00000000) {
      ETH.config(static_ip, static_gw, static_subnet);
  }  
#endif

#ifdef EN_WIFI
  // Start wifi
  WiFi.disconnect(true);
  WiFi.onEvent(networkEvent);
  WiFi.begin(ssid, password);
#endif

#ifdef EN_MQTT_CLIENT
  mqttSetup();
#endif

  // AlarmDecoder wiring.
  AD2Parse.setCB_ON_RAW_MESSAGE(my_ON_RAW_MESSAGE_CB);

}

/**
 * Arduino Sketch loop()
 *   Called consecutively after setup().
 */
void loop()
{

  // UART AD2/HOST bridge and AD2* message processing
  uartLoop();
  
  // Netorking ETH/WiFi persistent connection state machine cycles
  networkLoop();

  // update currrent time
  time_now = millis();

  // small loop delay needed for time tracking
  delay(1);

}

/**
 * Networking state machine
 *  1) Monitor network hardware.
 *  2) Keep connections persistent.
 *  3) Respond to connection activity.
 *  
 */
void networkLoop() {

  // calculate time since the end of the last loop.
  uint32_t tlaps = (millis() - time_now);

  // if we have an interface active process network service states
  if (eth_connected || wifi_connected) {

#ifdef EN_MQTT_CLIENT
    mqttLoop(tlaps);
#endif

#ifdef EN_REST_CLIENT
#error FIXME: not implemented.
#endif

#ifdef EN_SSDP
#error FIXME: not implemented.
#endif

  }
}


/**
 * WIFI / Ethernet state event handler
 */
void networkEvent(WiFiEvent_t event)
{
  switch (event) {

#ifdef EN_WIFI
    case SYSTEM_EVENT_WIFI_READY: 
      Serial.println("!DBG:AD2EMB,WiFi interface ready");
      WiFi.setHostname(host_name);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("!DBG:AD2EMB,STA Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("!DBG:AD2EMB,STA Connected");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("!DBG:AD2EMB,WIFI MAC: ");
      Serial.print(WiFi.macAddress());
      Serial.print(", IPv4: ");
      Serial.println(WiFi.localIP());
      wifi_connected = true;
      break;
    case SYSTEM_EVENT_STA_STOP:
      Serial.println("!DBG:AD2EMB,STA Stopped");
      wifi_connected = false;
      break;
#endif

#ifdef EN_ETH
    case SYSTEM_EVENT_ETH_START:
      Serial.println("!DBG:AD2EMB,ETH Started");
      //set eth hostname here
      ETH.setHostname(host_name);
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("!DBG:AD2EMB,ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("!DBG:AD2EMB,ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("!DBG:AD2EMB,ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("!DBG:AD2EMB,ETH Stopped");
      eth_connected = false;
      break;
#endif  
      
    default:
      break;
  }
}


#ifdef EN_MQTT_CLIENT
/**
 * MQTT Client reporting service
 * 
 *  1) Stay connected to a server.
 *     PubSubClient.cpp sends network PING to ensure the 
 *     client stays connected to the server.
 *
 *  2) Publish periodic messages to subscriber(s) to ensure
 *     this device publications are being monitored.
 *
 */

/**
 * MQTT setup
 */
void mqttSetup() {
  mqtt_clientId = "AD2ESP32Client-";
  mqtt_clientId += String(random(0xffff), HEX);
  mqttClient.setServer(mqtt_server, 1883);
}

/**
 * Attempt to connect to MQTT server(blocking)
 */
bool mqttConnect() {
  bool res = true;
  
  if(!mqttClient.connected()) {
    Serial.print("!DBG:AD2EMB,MQTT connection starting...");
    // Attempt to connect
    if (mqttClient.connect(mqtt_clientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe("AD2LRR");
      mqttClient.publish("AD2LRR", "!LRR:008,1,CID_3123,ff");
    } else {
      Serial.print(" failed, client.connect() rc=");
      Serial.println(mqttClient.state());
      res = false;
    }
  }
  return res;
}

/**
 * Process MQTT state
 */
void mqttLoop(uint32_t tlaps) {
      /// delay: counters must be signed to easily detect overflow by going negative
    static int32_t mqtt_reconnect_delay = 0;
    static int32_t mqtt_ping_delay = 0;
  
    /// state: fire off a message on first connect.
    static uint8_t mqtt_signon_sent = 0;

    if (!mqttClient.connected()) {
      if (!mqtt_reconnect_delay) {
        mqtt_reconnect_delay = MQTT_CONNECT_RETRY_INTERVAL;
      } else {
        mqtt_reconnect_delay -= tlaps;
        if (mqtt_reconnect_delay<=0){
          mqtt_reconnect_delay=0;
          if (!mqttConnect()) {
            mqtt_reconnect_delay = MQTT_CONNECT_RETRY_INTERVAL;         
          }
        }
        if (mqttClient.connected()) {
          mqtt_ping_delay = MQTT_CONNECT_PING_INTERVAL;
        }
      }
    } else {
      if (!mqtt_signon_sent) {
        Serial.println("!DBG:AD2EMB,MQTT publish AD2LRR:TEST");
        mqttClient.publish("AD2LRR", "!LRR:008,1,CID_1123,ff");
        mqtt_signon_sent = 1;
      }

      // See if we have a host subscribing to AD2LRR watching this device.
      // FIXME: If the host goes away set an alarm state
      mqtt_ping_delay -= tlaps;      
      if (mqtt_ping_delay<=0) {
        Serial.println("!DBG:AD2EMB,MQTT publish AD2LRR:AD2ESP32_PING");
        mqttClient.publish("AD2LRR", "!INF:AD2ESP32_PING");
        mqtt_ping_delay = MQTT_CONNECT_PING_INTERVAL;
      }
      
    }

    // give the MQTT client class state machine cycles
    mqttClient.loop();    

}
#endif // EN_MQTT_CLIENT

/**
 * 1) read from AD2* uart and send to host uart.
 * 2) read from host uart and send to AD2* uart.
 * 3) process message from AD2* uart and update AD2* state machine.
 */
void uartLoop() {
  int len;
  static uint8_t buff[100];
  
  // Read any data from the AD2* device echo to the HOST uart and parse it.
  while ((len = Serial2.available())>0) {
    
    // avoid consuming more than our storage.
    if (len > sizeof(buff)) {
      len = sizeof(buff);
    }
    
    int res = Serial2.readBytes(buff, len);
    if (res > 0) {
      if (raw_mode) {
        // Raw mode just echo data to the host.
        Serial.write(buff, len);
      } else {
        // Parse data from AD2* and report back to host.
        AD2Parse.put(buff, len);        
      }
    }
  }
  
  // Send any host data to the AD2*
  // WARNING! AVOID multiple systems sending data at the same time to the panel.
  while (Serial.available()>0) {
    int res = Serial.read();
    if (res > -1) {
      Serial2.write((uint8_t)res);
    }
  }
}

/**
 * AlarmDecoder callbacks.
 */
 
/**
 * ON_RAW_MESSAGE
 * When a full message is received before it is parsed.
 * WARNING: It may be invalid.
 */
void my_ON_RAW_MESSAGE_CB(void *msg) {
  String* sp = (String *)msg;
  Serial.println(*sp);
}
