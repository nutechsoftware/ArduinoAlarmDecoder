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

#include "config.h"

// AlarmDecoder arduino parsing state machine.
// https://github.com/nutechsoftware/ArduinoAlarmDecoder
#include <ArduinoAlarmDecoder.h>

/**
 * Include our secrets file. Must create this file to build the project.
 * 
 * Inside of the Arduino  IDE create a new TAB and call it secrets.h
 * Next add the next two lines to this file and save the file.
#define SECRET_WIFI_SSID "Your SSID Here"
#define SECRET_WIFI_PASS "Your PASSWORD here"
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
#define EN_MQTT_ASYNC_CLIENT
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
TimerHandle_t wifiReconnectTimer;
#define WIFI_CONNECT_INTERVAL 5000
#endif // EN_WIFI


/**
 * MQTT Async client support
 */
 /**
  * FIXME:
  * trying this out.
  *   https://github.com/me-no-dev/ESPAsyncTCP
  *   https://github.com/me-no-dev/AsyncTCP
  */
#ifdef EN_MQTT_ASYNC_CLIENT
// async-mqtt-client v0.8.1
// https://github.com/marvinroger/async-mqtt-client
// *requires https://github.com/me-no-dev/AsyncTCP
#include <AsyncMqttClient.h>

/// MQTT secrets.h defines.
// MQTT DEFINES
#define MQTT_CONNECT_RETRY_INTERVAL 5000   // every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL 60000   // every 60 seconds

// MQTT globals
#if defined(SECRET_MQTT_SERVER)
#else
#error select an MQTT server profile from secrets.h
#endif
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
String mqtt_clientId;
#endif // EN_MQTT_ASYNC_CLIENT


/** 
 * Test code defines
 */
#if defined(EN_ETH) + defined(EN_WIFI) == 0
#error Must define at least one network interface
#endif


/** 
 * Common global/static 
 */
#define VERBOSE

// used by both Ethernet and Wifi arduino drivers
const char* host_name   = "AD2ESP32";
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
  // Give the  ESP32 uart driver lots of space to avoid data loss when busy.
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
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(WIFI_CONNECT_INTERVAL), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  connectToWifi();
#endif

#if defined(EN_MQTT_ASYNC_CLIENT)
  // setup MQTT connection.
  mqttSetup();
#endif

  // ASYNC AlarmDecoder event wiring.
  AD2Parse.setCB_ON_MESSAGE(my_ON_MESSAGE_CB);
  AD2Parse.setCB_ON_LRR(my_ON_LRR_CB);
}

#ifdef EN_WIFI
void connectToWifi() {
  Serial.println("!DBG:AD2EMB,Wi-Fi connect starting.");
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
}
#endif

/**
 * Arduino Sketch loop()
 *   Called consecutively after setup().
 */
void loop()
{

  // UART AD2/HOST bridge and AD2* message processing
  uartLoop();
  
  // update currrent time
  time_now = millis();

  // small loop delay needed for time tracking
  delay(1);

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
#if defined(EN_MQTT_ASYNC_CLIENT)
      // disable mqttTimer connect timer if even active
      xTimerStop(mqttReconnectTimer, 0);
#endif
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
#if defined(EN_MQTT_ASYNC_CLIENT)
      // start MQTT ASYNC connection
      mqttConnect();
#endif
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
#if defined(EN_MQTT_ASYNC_CLIENT)
      // start MQTT ASYNC connection
      mqttConnect();
#endif
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("!DBG:AD2EMB,ETH Disconnected");
      eth_connected = false;
#if defined(EN_MQTT_ASYNC_CLIENT)
      // disable mqttTimer connect timer if even active
      xTimerStop(mqttReconnectTimer, 0);
#endif
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


#if defined(EN_MQTT_ASYNC_CLIENT)
/**
 * MQTT ASYNC Client reporting service
 * 
 *  1) Stay connected to a server.
 *     AsyncMqttClient.cpp sends network PING to ensure the
 *     client stays connected to the server.
 *
 *  2) Publish periodic messages to subscriber(s) to ensure
 *     this device publications are being monitored.
 *
 *  3) Optional set QOS level 2 to ensure data deliver.
 *
 */

/**
 * MQTT setup
 */
void mqttSetup() {

  // setup ASYNC callbackup functions
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(SECRET_MQTT_SERVER, SECRET_MQTT_PORT);

  // build our client ID
  mqtt_clientId = "AD2ESP32Client-";
  mqtt_clientId += String(random(0xffff), HEX);
  mqttClient.setClientId(mqtt_clientId.c_str());

  // keepalive
  mqttClient.setKeepAlive(20);

  // configure server connection and credentials.
  mqttClient.setServer(SECRET_MQTT_SERVER, SECRET_MQTT_PORT);
  mqttClient.setCredentials(SECRET_MQTT_USER,SECRET_MQTT_PASS);

#ifdef SECRET_MQTT_SERVER_CERT
  // enable secure
  //mqttClient.setSecure(true);

  //mqttClient.setSecure(MQTT_SECURE);
//  if (MQTT_SECURE) {
    mqttClient.addServerFingerprint((const uint8_t[])SECRET_MQTT_SERVER_FINGERPRINT);
//  }

  //mqttClient.addServerFingerprint();
#endif

  // create timer for mqtt reconnecting ever 5 seconds when disconnected
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(MQTT_CONNECT_RETRY_INTERVAL),
   pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(mqttConnect));
}

/**
 * Start ASYNC connect to MQTT server
 */
void mqttConnect() {
  Serial.println("!DBG:AD2EMB,MQTT connection starting...");
  mqttClient.connect();
}

/**
 * MQTT ASYNC Callback on connect
 */
void onMqttConnect(bool sessionPresent) {
  Serial.println("!DBG:AD2EMB,MQTT connected.");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT Session present: ");
  Serial.println(sessionPresent);
#endif

  uint16_t packetIdSub = mqttClient.subscribe(SECRET_MQTT_TOPIC, SECRET_MQTT_QOS);

#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
#endif

  uint16_t packetIdPub1 = mqttClient.publish(SECRET_MQTT_TOPIC, SECRET_MQTT_QOS, true, "!LRR:008,1,CID_3123,ff");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT Publishing.");
  Serial.println(packetIdPub1);
#endif

}

/**
 * MQTT ASYNC Callback on disconnect
 */
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.print("!DBG:AD2EMB,MQTT Disconnected. ");
  if (eth_connected || wifi_connected) {
    Serial.print(" Starting reconnect timer.");
    xTimerStart(mqttReconnectTimer, 0);
  } else {
    Serial.print(" Waiting for network to return.");
  }
}

/**
 * MQTT ASYNC Callback on subscribe
 */
void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("!DBG:AD2EMB,MQTT Subscribe acknowledged.");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT packetId: ");
  Serial.println(packetId);
  Serial.print("!DBG:AD2EMB,MQTT qos: ");
  Serial.println(qos);
#endif
}

/**
 * MQTT ASYNC Callback on unsubscribe
 */
void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("!DBG:AD2EMB,MQTT Unsubscribe acknowledged.");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT packetId: ");
  Serial.println(packetId);
#endif
}

/**
 * MQTT ASYNC Callback on message
 */
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("!DBG:AD2EMB,MQTT Publish received.");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT topic: ");
  Serial.println(topic);
  Serial.print("!DBG:AD2EMB,MQTT qos: ");
  Serial.println(properties.qos);
  Serial.print("!DBG:AD2EMB,MQTT dup: ");
  Serial.println(properties.dup);
  Serial.print("!DBG:AD2EMB,MQTT retain: ");
  Serial.println(properties.retain);
  Serial.print("!DBG:AD2EMB,MQTT len: ");
  Serial.println(len);
  Serial.print("!DBG:AD2EMB,MQTT index: ");
  Serial.println(index);
  Serial.print("!DBG:AD2EMB,MQTT total: ");
  Serial.println(total);
#endif
}

/**
 * MQTT ASYNC Callback on publish
 */
void onMqttPublish(uint16_t packetId) {
  Serial.println("!DBG:AD2EMB,MQTT Publish acknowledged.");
#if defined(VERBOSE)
  Serial.print("!DBG:AD2EMB,MQTT packetId: ");
  Serial.println(packetId);
#endif
}
#endif //EN_MQTT_ASYNC_CLIENT

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
 * ASYNC AlarmDecoder callbacks.
 */
 
/**
 * ON_MESSAGE
 * When a full standard alarm state message is received before it is parsed.
 * WARNING: It may be invalid.
 */
void my_ON_MESSAGE_CB(void *msg) {
  String* sp = (String *)msg;
  Serial.println(*sp);
}

/**
 * ON_LRR
 * When a LRR message is received.
 * WARNING: It may be invalid.
 */
void my_ON_LRR_CB(void *msg) {
  String* sp = (String *)msg;
  Serial.println(*sp);
}
