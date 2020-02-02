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

/**
 * Base configuration settings
 */
#include "config.h"

/**
 * Include secrets file to contain keys, passwords that may be private
 * to a given install.
 */
#include "secrets.h"

/**
 * AlarmDecoder Arduino library.
 * https://github.com/nutechsoftware/ArduinoAlarmDecoder
 */
#include <ArduinoAlarmDecoder.h>

/**
 * Arduino/Espressif built in support for LAN87XX chip
 */
#ifdef EN_ETH
#include <ETH.h>
#endif

/** 
 * Arduino/Espressif built in support for WiFi 
 */
#ifdef EN_WIFI
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif // EN_WIFI

/**
 *  MQTT client support
 */
#if defined(EN_MQTT_CLIENT)
// PubSubClient v2.6.0 by Nick O'Leary
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
#endif // EN_MQTT_CLIENT

/** 
 * Base settings tests.
 */
#if defined(EN_ETH) + defined(EN_WIFI) == 0
#error Must define at least one network interface
#endif

/** 
 * Global/Static/constant state variables
 */
// Network state variables.
static bool eth_connected = false;
static bool wifi_connected = false;

// time/delay tracking
static uint32_t time_now = 0;

// raw mode allows direct access to the AD2* device and disables internal processing.
static bool raw_mode = false;

// AlarmDecoder parser
AlarmDecoderParser AD2Parse;

// MQTT client
#if defined(EN_MQTT_CLIENT)
#if defined(SECRET_MQTT_SERVER) && defined(SECRET_MQTT_SERVER_CERT)
WiFiClientSecure mqttnetClient;
#elif defined(SECRET_MQTT_SERVER)
WiFiClient mqttnetClient;
#else
#error select an MQTT server profile from secrets.h
#endif
PubSubClient mqttClient(mqttnetClient);
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
//#ifdef DEBUG
  //Serial.setDebugOutput(true);
  esp_log_level_set("*", ESP_LOG_VERBOSE);
//#endif
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
      ETH.config(static_ip, static_gw, static_subnet, static_dns1, static_dns2);
  }  
#endif

#ifdef EN_WIFI
  // Start wifi
  WiFi.disconnect(true);
  WiFi.onEvent(networkEvent);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
#endif

#ifdef EN_MQTT_CLIENT
#ifdef SECRET_MQTT_SERVER_CERT
#endif
  mqttSetup();
#endif

  // AlarmDecoder wiring.
  AD2Parse.setCB_ON_MESSAGE(my_ON_MESSAGE_CB);
  AD2Parse.setCB_ON_LRR(my_ON_LRR_CB);
}

/**
 * Arduino Sketch loop()
 *   Called consecutively after setup().
 */
void loop()
{

  // UART AD2/HOST bridge and AD2* message processing
  uartLoop();
  
  // Networking ETH/WiFi persistent connection state machine cycles
  networkLoop();

  // update current time
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
 * UART processing loop.
 *  1) read from AD2* uart and send to host uart.
 *  2) read from host uart and send to AD2* uart.
 *  3) process message from AD2* uart and update AD2* state machine.
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
 * WIFI / Ethernet state event handler
 */
void networkEvent(WiFiEvent_t event)
{
  switch (event) {

#ifdef EN_WIFI
    case SYSTEM_EVENT_WIFI_READY: 
      Serial.println("!DBG:AD2EMB,WiFi interface ready");
      WiFi.setHostname(BASE_HOST_NAME);
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
      ETH.setHostname(BASE_HOST_NAME);
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
  mqttClient.setServer(SECRET_MQTT_SERVER, SECRET_MQTT_PORT);
#ifdef SECRET_MQTT_SERVER_CERT
  /* set SSL/TLS certificate */
  mqttnetClient.setCACert(SECRET_MQTT_SERVER_CERT);
  // FIXME: client certificates.
  /// client.setCertificate
  /// client.setPrivateKey
#endif
}

/**
 * Attempt to connect to MQTT server(blocking)
 */
bool mqttConnect() {
  bool res = true;
  
  if(!mqttClient.connected()) {
    Serial.print("!DBG:AD2EMB,MQTT connection starting...");
    // Attempt to connect
    if (mqttClient.connect(mqtt_clientId.c_str(), SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
      Serial.println("connected");
      mqttClient.subscribe("AD2LRR");
      mqttClient.publish(SECRET_MQTT_ROOT_TOPIC "AD2LRR", "!LRR:008,1,CID_3123,ff");
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
        mqttClient.publish(SECRET_MQTT_ROOT_TOPIC "AD2LRR", "!LRR:008,1,CID_1123,ff");

        mqtt_signon_sent = 1;
      }

      // See if we have a host subscribing to AD2LRR watching this device.
      // FIXME: If the host goes away set an alarm state
      mqtt_ping_delay -= tlaps;      
      if (mqtt_ping_delay<=0) {
        Serial.println("!DBG:AD2EMB,MQTT publish AD2LRR:AD2ESP32_PING");
        mqttClient.publish(SECRET_MQTT_ROOT_TOPIC "AD2LRR", "!INF:AD2ESP32_PING");
        mqtt_ping_delay = MQTT_CONNECT_PING_INTERVAL;
      }
      
    }

    // give the MQTT client class state machine cycles
    mqttClient.loop();    

}
#endif // EN_MQTT_CLIENT


/**
 * AlarmDecoder callbacks.
 * As the AlarmDecoder receives data via put() data is validated.
 * When a complete messages is received or a specific stream of
 * bytes is received event(s) will be called.
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
