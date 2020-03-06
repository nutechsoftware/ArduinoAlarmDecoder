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
 * Include SPIFFS & FS library for static content in program memeory.
 */
#include <SPIFFS.h>
#include <FS.h>

/**
 * TinyTemplateEngine v1.1.0 by full-stack-ex
 * Library manager 'TinyTemplateEngine'
 * https://github.com/full-stack-ex/tiny-template-engine-arduino
 * 20200223SM: custom SPIFFS reader in contrib/tiny-template-arduino/
 * place the .cpp and .h file into {arduino_home}/library/tiny-template-arduino/
 * TODO: Send to repo mgr for consideration.
 */
#include "TinyTemplateEngine.h"
#include "TinyTemplateEngineMemoryReader.h"
#include "TinyTemplateEngineSPIFFSReader.h" // FIXME: 20200223SM custom mod
/**
 * AlarmDecoder Arduino library.
 * https://github.com/nutechsoftware/ArduinoAlarmDecoder
 */
#include <ArduinoAlarmDecoder.h>

/**
 * Arduino/Espressif built in support for LAN87XX chip
 */
#if defined(EN_ETH)
#include <ETH.h>
#endif

/** 
 * Arduino/Espressif built in support for WiFi
 * FIXME: 20200103SM reconnect after wifi drop not working.
 *   * (maybe bound it inside of event handler for wifi)
 */
#if defined(EN_WIFI)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif // EN_WIFI

/**
 *  MQTT client support
 */
#if defined(EN_MQTT_CLIENT)
// PubSubClient v2.6.0 by Nick O'Leary
// Library manager 'PubSubClient'
// https://github.com/knolleary/pubsubclient
// NOTE: Adjust timeout PubSubClient/PubSubClient.h
//       MQTT_SOCKET_TIMEOUT from default of 15 to 2
#include <PubSubClient.h>
#endif // EN_MQTT_CLIENT

/**
 * SSDP Services.
 *
 * Not in library manager.
 * cd ${home}/Arduino/libraries
 * git clone https://github.com/f34rdotcom/ESP32SSDP.git
 */
#if defined(EN_SSDP)
#include <ESP32SSDP.h>
#endif


/**
 * HTTPServer/HTTPServer server  v0.3.? by Frank Hessel
 * Library manager 'esp32_https_server'
 * https://github.com/fhessel/esp32_https_server
 * must use github master branch for now.
 * I am working with project dev to add features. It was merged
 * into master and should get a release version soon.
 */
#if defined(EN_HTTP)
#include <HTTPServer.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#endif
#if defined(EN_HTTPS)
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#endif

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
#if defined(EN_WIFI)
static bool wifi_ready = false;
#endif

// time/delay tracking
static unsigned long time_now = 0;
static unsigned long time_laps = 0;

// raw mode allows direct access to the AD2* device and disables internal processing.
static bool raw_mode = false;

// AlarmDecoder parser
AlarmDecoderParser AD2Parse;
#if defined(AD2_SOCK)
WiFiClient AD2Sock;
#endif

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
String mqtt_root;
#endif // EN_MQTT_CLIENT

// REST service
#if defined(EN_REST)
typedef struct {
  unsigned long expire_time;
  String *host;
  String *callback;
  String *uuid;
} rest_subscriber_item_t;
rest_subscriber_item_t *rest_subscribers[REST_MAX_SUBSCRIBERS] = {};
std::map<String, uint32_t> TIME_MULTIPLIER = {{"SECONDS", 1 * 1000},{"MINUTES", 60 * 1000},{"HOURS", 3600 * 1000},{"DAYS", 86400 * 1000}};
#endif // EN_REST

// HTTP/HTTPS server
#if defined(EN_HTTP) || defined(EN_HTTPS)
using namespace httpsserver;
#endif
#if defined(EN_HTTPS)
// The HTTPS Server comes in a separate namespace. For easier use, include it here.
// Create an SSL certificate object from the files included above
SSLCert cert = SSLCert(
  example_crt_DER, example_crt_DER_len,
  example_key_DER, example_key_DER_len
);
HTTPSServer secureServer = HTTPSServer(&cert, HTTPS_PORT);
#endif // EN_HTTPS
#if defined(EN_HTTP)
HTTPServer insecureServer = HTTPServer(HTTP_PORT);
#endif
#if defined(EN_HTTP) || defined(EN_HTTPS)
// Declare some handler functions for the various URLs on the server
void handleCatchAll(HTTPRequest * req, HTTPResponse * res);
#if defined(EN_REST)
void handleEventSUBSCRIBE(HTTPRequest * req, HTTPResponse * res);
void handleEventUNSUBSCRIBE(HTTPRequest * req, HTTPResponse * res);
#endif // EN_REST
#endif // EN_HTTP || EN_HTTPS

/**
 * generate uptime string
 */
void uptimeString(String &tstring)
{
  // 64bit milliseconds since boot
  int64_t ms = esp_timer_get_time() / 1000;
  // seconds
  int32_t s = ms / 1000;
  // days
  int32_t d = s / 86400; s %= 86400;
  // hours
  uint8_t h = s / 3600; s %= 3600;
  // minutes
  uint8_t m = s / 60; s %= 60;
  char fbuff[255];
  snprintf(fbuff, sizeof(fbuff), "%04dd:%02dh:%02dm:%02ds", d, h, m, s);
  tstring = fbuff;
}

/**
 * generate AD2* uuid
 */
void genUUID(uint32_t n, String &ret) {
  uint32_t chipId = ((uint16_t) (ESP.getEfuseMac() >> 32));
  char _uuid[37];
  snprintf(_uuid, sizeof(_uuid), SECRET_SSDP_UUID_FORMAT,
  (uint16_t) ((chipId >> 16) & 0xff),
  (uint16_t) ((chipId >>  8) & 0xff),
  (uint16_t) ((chipId      ) & 0xff),
  (uint16_t) ((n      >> 24) & 0xff),
  (uint16_t) ((n      >> 16) & 0xff),
  (uint16_t) ((n      >>  8) & 0xff),
  (uint16_t) ((n           ) & 0xff));
  ret = _uuid;
}

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
  Serial.println("!DBG:AD2EMB,Starting");
#if defined(DEBUG)
  //Serial.setDebugOutput(true);
  esp_log_level_set("*", ESP_LOG_VERBOSE);
#endif

#if defined(AD2_UART)
  // Open AlarmDecoder UART
  Serial2.begin(AD2_BAUD, SERIAL_8N1, AD2_TX, AD2_RX);
  // The ESP32 uart driver has its own interrupt and buffers for processing
  // rx bytes. Give it plenty of space. 1024 gave about 1 minute storage of
  // normal messages from AD2 on Vista 50PUL panel with one partition.
  // If any loop() method is busy too long alarm panel state data will be lost.
  Serial2.setRxBufferSize(2048);
#endif // AD2_UART

  // start SPIFFS flash file system driver
  Serial.print("!DBG:AD2EMB,SPIFFS Start..");
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed.");
  } else {
    Serial.println("Success..");
  }

#if defined(EN_ETH) || defined(EN_WIFI)
  WiFi.onEvent(networkEvent);
#endif

#if defined(EN_ETH)
  // Start ethernet
  Serial.println("!DBG:AD2EMB,ETH Start wait for interface ready");
  ETH.begin();
  ETH.setHostname(BASE_HOST_NAME);  
  // Static IP or DHCP?
  if (static_ip != (uint32_t)0x00000000) {
      Serial.println("!DBG:AD2EMB,ETH setting static IP");
      ETH.config(static_ip, static_gw, static_subnet, static_dns1, static_dns2);
  }  
#endif // EN_ETH

#if defined(EN_WIFI)
  // Start wifi
  Serial.println("!DBG:AD2EMB,WiFi Start. Wait for interface");
  WiFi.setHostname(BASE_HOST_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
#endif // EN_WIFI

#if defined(EN_MQTT_CLIENT)
#if defined(SECRET_MQTT_SERVER_CERT)
#endif // SECRET_MQTT_SERVER_CERT
  mqttSetup();
#endif // EN_MQTT_CLIENT

#if defined(EN_SSDP)
  String szUUID; genUUID(0, szUUID);
  SSDP.setUUID(szUUID.c_str(), false);
  SSDP.setInterval(1200);
  SSDP.setServerName(BASE_HOST_NAME "/" BASE_HOST_VERSION);
  SSDP.setSchemaURL("device_description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setModelName(BASE_HOST_NAME);
  SSDP.setModelNumber("2.0");
  SSDP.setDeviceType("urn:schemas-upnp-org:device:AlarmDecoder:1");
#endif // EN_SSDP

#if defined(EN_HTTP) || defined(EN_HTTPS)
  ResourceNode * nodeCatchAll = new ResourceNode("", "", &handleCatchAll);
#if defined(EN_REST)
  ResourceNode * nodeEventSUBSCRIBE = new ResourceNode(HTTP_API_BASE "/event", "SUBSCRIBE", &handleEventSUBSCRIBE);
  ResourceNode * nodeEventUNSUBSCRIBE = new ResourceNode(HTTP_API_BASE "/event", "UNSUBSCRIBE", &handleEventUNSUBSCRIBE);
#endif // EN_REST
#if defined(EN_HTTP)
  insecureServer.setDefaultHeader("Server", BASE_HOST_NAME "/" BASE_HOST_VERSION);
  insecureServer.setDefaultHeader("Cache-Control", "no-cache, no-store, must-revalidate, private");
  insecureServer.setDefaultHeader("Pragma", "no-cache");
  insecureServer.setDefaultHeader("X-XSS-Protection", "1; mode=block");
  insecureServer.setDefaultHeader("X-Frame-Options", "SAMEORIGIN");
  insecureServer.setDefaultNode(nodeCatchAll);
#if defined(EN_REST)
  insecureServer.registerNode(nodeEventSUBSCRIBE);
  insecureServer.registerNode(nodeEventUNSUBSCRIBE);
#endif // EN_REST
#endif // EN_HTTP
#if defined(EN_HTTPS)
  secureServer.setDefaultHeader("Server", BASE_HOST_NAME "/" BASE_HOST_VERSION);
  secureServer.setDefaultHeader("Cache-Control", "no-cache, no-store, must-revalidate, private");
  secureServer.setDefaultHeader("Pragma", "no-cache");
  secureServer.setDefaultHeader("X-XSS-Protection", "1; mode=block");
  secureServer.setDefaultHeader("X-Frame-Options", "SAMEORIGIN");
  secureServer.setDefaultHeader("Connection", "close");
#if defined(EN_REST)
  secureServer.registerNode(nodeDeviceDescription);
  secureServer.registerNode(nodeServiceDescription);
#endif // EN_REST
#endif // EN_HTTPS
#endif // EN_HTTP || EN_HTTPS

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
  // calculate time since the end of the last loop.
  time_laps = (micros() - time_now);
  // update current time
  time_now = micros();
  // delay exception tracking
  if (time_laps > 100000) {
     Serial.printf("!DBG:AD2EMB,TLAPS EXCEPTION A: %lu\n", time_laps);
  }

  // AD2* message processing
  ad2Loop();
  
  // Networking ETH/WiFi persistent connection state machine cycles
  networkLoop();

  // small loop delay needed for time tracking > 1ms
  delayMicroseconds(10);
}

/**
 * Networking state machine
 *  1) Monitor network hardware.
 *  2) Keep connections persistent.
 *  3) Respond to connection activity.
 *  
 */
void networkLoop() {

#if defined(EN_WIFI)
  static long wifi_reconnect_delay = 0;
  if (wifi_ready && !wifi_connected) {
    if (!wifi_reconnect_delay) {
      wifi_reconnect_delay = WIFI_CONNECT_RETRY_INTERVAL;
    } else {
      wifi_reconnect_delay -= time_laps;
      if (wifi_reconnect_delay<=0){
        wifi_reconnect_delay = 0;
        Serial.println("!DBG:AD2EMB,STA Connecting");
        WiFi.reconnect();
      }
    }
  }
#endif

  // if we have an interface active process network service states
  if (eth_connected || wifi_connected) {

#if defined(EN_MQTT_CLIENT)
    mqttLoop();
#endif

#if defined(EN_REST_CLIENT)
#error FIXME: not implemented.
#endif

#if defined(EN_SSDP)
    static bool ssdp_started = false;
    if (!ssdp_started) {
      Serial.println("!DBG:SSDP starting.");
      SSDP.begin();
      ssdp_started = true;
    }
#endif

#if defined(EN_REST)
    // remove expired subscriptions
    for (int n = 0; n < REST_MAX_SUBSCRIBERS; n++) {
      if (rest_subscribers[n]) {
        if (millis() > rest_subscribers[n]->expire_time) {
           freeSubscriberLOC(n);
        }
      }
    }
#endif // EN_REST

#if defined(EN_HTTP)
    static bool http_started = false;
    if (!http_started) {
      Serial.print("!DBG:HTTP server starting.");
      http_started = true;
      insecureServer.start();
      if (insecureServer.isRunning()) {
        Serial.println("..running.");
      } else {
        Serial.println("..failed to start.");
      }
    } else {
      insecureServer.loop();
    }
#endif

#if defined(EN_HTTPS)
    static bool https_started = false;
    if (!https_started) {
      Serial.println("!DBG:HTTPS server starting.");
      https_started = true;
      secureServer.start();
      if (secureServer.isRunning()) {
        Serial.println("..running.");
      } else {
        Serial.println("..failed to start.");
      }
    } else {
      secureServer.loop();
    }
#endif

  }
}

/**
 * AlarmDecoder processing loop.
 *  1) read from AD2* uart/sock and send to host uart.
 *  2) read from host uart/sock and send to AD2* uart.
 *  3) process message from AD2* uart and update AD2* state machine.
 */
void ad2Loop() {
  int len;
  static uint8_t buff[100];

#if defined(AD2_SOCK)
  // if we have an interface active process network service states
  if (eth_connected || wifi_connected) {
      if (!AD2Sock.connected()) {
        AD2Sock.connect(AD2_SOCKIP,AD2_SOCKPORT);
      } else {
        int8_t maxread = 100;
        while (AD2Sock.available() && (maxread--)>0) {
          // Parse data from AD2* and report back to host.
          int8_t rx = buff[0] = AD2Sock.read();
          if (rx>0) {
            if (raw_mode) {
              // Raw mode just echo data to the host.
              Serial.write(buff, len);
            } else {
              AD2Parse.put(buff, 1);
            }
          }
        }
      }
    }
#endif
#if defined(AD2_UART)
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
#endif
  // Send any host data to the AD2*
  // WARNING! AVOID multiple systems sending data at the same time to the panel.
  while (Serial.available()>0) {
    int res = Serial.read();
    if (res > -1) {
#if defined(AD2_UART)
      Serial2.write((uint8_t)res);
#endif
#if defined(AD2_SOCK)
#endif
    }
  }
}

/**
 * WIFI / Ethernet state event handler
 */
void networkEvent(WiFiEvent_t event)
{
  switch (event) {

#if defined(EN_WIFI)
    case SYSTEM_EVENT_WIFI_READY: 
      Serial.println("!DBG:AD2EMB,WiFi interface ready");
      wifi_ready = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("!DBG:AD2EMB,STA Disconnected");
      wifi_connected = false;
      closeConnections();
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
#endif // EN_WIFI

#if defined(EN_ETH)
    case SYSTEM_EVENT_ETH_START:
      Serial.println("!DBG:AD2EMB,ETH Started");
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
#endif // EN_ETH
      
    default:
      break;
  }
}

/**
 * If wifi or network drop close all outbound connections.
 */
void closeConnections() {
Serial.println("!DBG:AD2EMB,Network reset close all connections");
#if defined(EN_MQTT_CLIENT)
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
#endif
}

#if defined(EN_MQTT_CLIENT)
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
 * generate client id
 */
void mqttMakeClientID(uint32_t n, String &ret) {
  uint32_t chipId = ((uint16_t) (ESP.getEfuseMac() >> 32));
  char _clid[24];
  snprintf(_clid, sizeof(_clid), SECRET_MQTT_CLIENTID_FORMAT,
  (uint16_t) ((chipId >> 16) & 0xff),
  (uint16_t) ((chipId >>  8) & 0xff),
  (uint16_t) ((chipId      ) & 0xff),
  (uint16_t) ((n      >> 24) & 0xff),
  (uint16_t) ((n      >> 16) & 0xff),
  (uint16_t) ((n      >>  8) & 0xff),
  (uint16_t) ((n           ) & 0xff));
  ret = _clid;
}

/**
 * MQTT setup
 */
void mqttSetup() {
  // generate unique mqtt client id
  mqttMakeClientID(0, mqtt_clientId);

  // build root path
  mqtt_root = SECRET_MQTT_ROOT;
  mqtt_root += MQTT_AD2EMB_PATH;
  mqtt_root += mqtt_clientId;
  mqtt_root += "/";

  mqttClient.setServer(SECRET_MQTT_SERVER, SECRET_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
#if defined(SECRET_MQTT_SERVER_CERT)
  /* set SSL/TLS certificate */
  mqttnetClient.setCACert(SECRET_MQTT_SERVER_CERT);
  // FIXME: client certificates.
  /// client.setCertificate
  /// client.setPrivateKey
#endif
}

/**
 * MQTT message callback
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("!DBG:AD2EMB,MQTT RX topic: '%s' payload: '%.*s'\n", topic, length, payload);
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
      // Subscribe to command input topic
      String subtopic = mqtt_root + MQTT_CMD_SUB_TOPIC;
      if (!mqttClient.subscribe(subtopic.c_str())) {
        Serial.printf("!DBG:AD2EMB,MQTT subscribe to CMD topic failed rc(%i)\n", mqttClient.state());
      }
    } else {
      Serial.print(" failed, client.connect() rc=");
      Serial.println(mqttClient.state());
      res = false;
    }
  } else {
    Serial.println("!DBG:AD2EMB,Warning reconnect when already connected");
  }
  return res;
}

/**
 * Process MQTT state
 */
void mqttLoop() {
    /// delay: counters must be signed to easily detect overflow by going negative
    static long mqtt_reconnect_delay = 0;
    static long mqtt_ping_delay = 0;
  
    /// state: fire off a message on first connect.
    static uint8_t mqtt_signon_sent = 0;

    /// give the mqtt library some time to process.
    mqttClient.loop();

    if (!mqttClient.connected()) {
      mqtt_signon_sent = 0;
      if (!mqtt_reconnect_delay) {
        Serial.printf("!DBG:AD2EMB,MQTT connection closed rc(%i) reconnect delay starting\n", mqttClient.state());
        mqtt_reconnect_delay = MQTT_CONNECT_RETRY_INTERVAL;
      } else {
        mqtt_reconnect_delay -= time_laps;
        if (mqtt_reconnect_delay<=0){
          mqtt_reconnect_delay=0;
          if (!mqttConnect()) {
            mqtt_reconnect_delay = MQTT_CONNECT_RETRY_INTERVAL;         
          }
        }
        if (mqttClient.connected()) {
          // force PING to notify subscriber(s) of this client id.
          mqtt_ping_delay = 0;
        }
      }
    } else {
      if (!mqtt_signon_sent) {
        Serial.println("!DBG:AD2EMB,MQTT publish AD2LRR:TEST");
        String pubtopic = mqtt_root + MQTT_LRR_PUB_TOPIC;
        if (!mqttClient.publish(pubtopic.c_str(), "!LRR:008,1,CID_3998,ff")) {
          Serial.printf("!DBG:AD2EMB,MQTT publish TEST fail rc(%i)\n", mqttClient.state());
        }
        mqtt_signon_sent = 1;
      }

      // See if we have a host subscribing to AD2LRR watching this device.
      // FIXME: If the host goes away set an alarm state
      mqtt_ping_delay -= time_laps;
      if (mqtt_ping_delay<=0) {
        Serial.println("!DBG:AD2EMB,MQTT publish AD2EMB-PING:PING");
        String pubtopic = mqtt_root + MQTT_PING_PUB_TOPIC;
        if (!mqttClient.publish(pubtopic.c_str(), mqtt_clientId.c_str())) {
          Serial.printf("!DBG:AD2EMB,MQTT publish PING fail rc(%i)\n", mqttClient.state());
        }
        mqtt_ping_delay = MQTT_CONNECT_PING_INTERVAL;
      }
    }
    if (mqtt_ping_delay > MQTT_CONNECT_PING_INTERVAL || mqtt_ping_delay < 0) {
      Serial.printf("!DBG:AD2EMB,TLAPS EXCEPTION B: %lu\n", mqtt_ping_delay);
    }
}
#endif // EN_MQTT_CLIENT

#if defined(EN_HTTP) || defined(EN_HTTPS)

/**
 * Given a file name get the extension and return a mime type.
 */
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".xml")) return "application/xml";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".jpg")) return "image/jpg";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

/**
 * HTTP catch all.
 *  1) Complete paths with Directory Index
 *  2) Test for SPIFFS file
 *  3) Apply templates if file with name+".tpl" exists.
 *  4) Send 404 if not found
 */
void handleCatchAll(HTTPRequest *req, HTTPResponse *res) {

  // send raw file or process as template
  bool apply_template = false;
  bool apply_gzip = false;

  // GET requests look for static template files
  if (req->getMethod() == "GET") {
    // Redirect / to /index.html
    String reqFile = (req->getRequestString()=="/") ? ("/" HTTP_DIR_INDEX) : req->getRequestString().c_str();

    // Set file path based upon request.
    String filename = String(FS_PUBLIC_PATH) + reqFile;

    // Check if same file with gz exist
    if (SPIFFS.exists(filename+".gz")) {
      Serial.printf("!DBG:AD2EMB,SPIFFS '%s.gz' found\n", filename.c_str());
      // set content type
      res->setHeader("Content-Type", getContentType(filename).c_str());

      filename += ".gz";
      apply_gzip = true;
    } else
    // Check if _NOT_ exists swap for 404.html
    if (!SPIFFS.exists(filename.c_str())) {
      Serial.printf("!DBG:AD2EMB,SPIFFS file not found '%s'\n", filename.c_str());
      filename = FS_PUBLIC_PATH "/404.html";
      res->setStatusCode(404);
      res->setStatusText("Not found");
      // set content type
      res->setHeader("Content-Type", getContentType(filename).c_str());
    }

    // Check if a flag file with the same name with .tpl extension exist.
    String tpl = filename+".tpl";
    if (!apply_gzip && SPIFFS.exists(tpl)) {
      Serial.printf("!DBG:AD2EMB,SPIFFS '%s' found\n", tpl.c_str());
      // set content type
      res->setHeader("Content-Type", getContentType(filename).c_str());
      apply_template = true;
    } else {
      Serial.printf("!DBG:AD2EMB,SPIFFS '%s' not found\n", tpl.c_str());
    }

    // Open the file
    File file = SPIFFS.open(filename.c_str());

    if (apply_gzip)
      res->setHeader("Content-Encoding", "gzip");

    // FIXME: add function will use it more than 1 time.
    // apply template if set
    if (apply_template) {
      Serial.printf("!DBG:AD2EMB,SPIFFS applying template to file '%s'\n", filename.c_str());
      
      // build standard template values FIXME: function dynamic.      
      String szVersion = "1.0";
      String szTime;
      uptimeString(szTime);
      String szLocalIP = WiFi.localIP().toString();
      String szClientIP = req->getClientIP().toString();
      String szProt = String(req->isSecure() ? "HTTPS" : "HTTP");
      String szUUID; genUUID(0, szUUID);
      
      const char* values[] = {
        szVersion.c_str(), // match ${0}
        szTime.c_str(),    // match ${1}
        szLocalIP.c_str(), // match ${2}
        szClientIP.c_str(),// match ${3}
        szProt.c_str(),    // match ${4}
        szUUID.c_str(),    // match ${5}
        0 // guard
      };

      // init template engine
      TinyTemplateEngineSPIFFSReader reader(&file);
      reader.keepLineEnds(true);
      TinyTemplateEngine engine(reader);

      // process and send
      engine.start(values);

      // Send content
      while (const char* line = engine.nextLine()) {
        res->print(line);
      }
      engine.end();
    } else {
      Serial.printf("!DBG:AD2EMB,SPIFFS spool raw file '%s'\n", filename.c_str());
      // set content type
      res->setHeader("Content-Type", getContentType(filename).c_str());
      // Set length if not a template but actual file.
      res->setHeader("Content-Length", httpsserver::intToString(file.size()));
      // Read the file and write it to the response
      uint8_t buffer[1024];
      ssize_t length = 0;
      do {
        length = file.read(buffer, 1024);
        res->write(buffer, length);
      } while (length > 0);
    }

    // Close the file
    file.close();
  } else {
    // discard remaining data from client
    req->discardRequestBody();
    // Send "405 Method not allowed" as response
    res->setStatusCode(405);
    res->setStatusText("Method not allowed");
    res->println("405 Method not allowed");
  }
}

#if defined(EN_REST)
bool checkAPIKey(HTTPRequest *req, HTTPResponse *res) {
  bool ret = true;
  String apikey = req->getHeader("Authorization").c_str();
  if (!apikey.equals(SECRET_REST_KEY)) {
    ret = false;
    // discard remaining data from client
    req->discardRequestBody();
    // Send "405 Method not allowed" as response
    res->setStatusCode(401);
    res->setStatusText("Unauthorized");
    res->println("401 Unauthorized");
  }
  return ret;
}

enum SSDP_RES { SSDP_UPDATED = 1, SSDP_ADDED = 0, SSDP_NO_SLOTS = -1, SSDP_NOT_FOUND = -2 };
/**
 * Attempt to locate a free subscriber slot and fill it with subscriber info.
 * Set idx to < 0 to remove subscriber.
 *
 * return
 *     1: success updated entry
 *     0: success new entry
 *    -1: fail no slots
 *    -2: delete not found
 */
int8_t addSubscriber(HTTPRequest *req, int8_t *idx) {
  int8_t ret = 0;

  // get key subscribe headers for searching
  String _host = req->getHeader("HOST").c_str();
  String _callback = req->getHeader("CALLBACK").c_str();
  String _timeout = req->getHeader("TIMEOUT").c_str();

  // check for host + callback match
  bool found = false; int16_t loc = -1;
  for (int n = 0; n < REST_MAX_SUBSCRIBERS; n++) {
    if (rest_subscribers[n]) {
      if (_host.equals(*rest_subscribers[n]->host) &&
          _callback.equals(*rest_subscribers[n]->callback))
      {
        found = true;
        loc = n;
        break;
      }
    } else {
      // save first empty location
      if (loc == -1)
        loc = n;
    }
  }

  // build the exire time value
  String tkey;
  uint32_t tval = 0;
  if (_timeout.length()) {
    char *tmult = strtok((char*)_timeout.c_str(), "-");
    if (tmult) {
      char *szval = strtok(NULL, "");
      if (szval) {
        tval = atoi(szval);
        tkey = strupr(tmult);
      }
    }
  }

  // match not found
  if (!found) {
    // free slot found for new entry
    if (loc > -1 && *idx > -1) {
      rest_subscribers[loc] = new rest_subscriber_item_t;
      rest_subscribers[loc]->host = new String(_host);
      rest_subscribers[loc]->callback = new String(_callback);
      rest_subscribers[loc]->uuid = new String;
      genUUID(loc+1, *rest_subscribers[loc]->uuid);
      rest_subscribers[loc]->expire_time = millis() + (TIME_MULTIPLIER[tkey] * tval);
      *idx = loc;
      ret = SSDP_ADDED;
    } else {
      if (*idx > -1) {
        // delete not found
        ret = SSDP_NOT_FOUND;
      } else {
        // no free slot
        ret = SSDP_NO_SLOTS;
      }
    }
  } else {
    if (*idx < 0) {
      // found existing delete requested.
      *idx = loc;
      freeSubscriberLOC(loc);
      ret = SSDP_UPDATED;
    } else {
      // found existing set new exire time.
      rest_subscribers[loc]->expire_time = millis() + (TIME_MULTIPLIER[tkey] * tval);
      ret = SSDP_UPDATED;
    }
    *idx = loc;
  }
  return ret;
}

/**
 * Free contents of subscriber
 */
void freeSubscriberLOC(uint8_t loc) {
  Serial.printf("!DBG:AD2EMB,SSDP freeSubscriberLOC loc(%i) uuid(%s)\n", loc, rest_subscribers[loc]->uuid->c_str());
  if (rest_subscribers[loc]->host)
    delete rest_subscribers[loc]->host;
  if (rest_subscribers[loc]->callback)
    delete rest_subscribers[loc]->callback;
  if (rest_subscribers[loc]->uuid)
    delete rest_subscribers[loc]->uuid;
  delete rest_subscribers[loc];
  rest_subscribers[loc] = 0;
}

/**
 * HTTP response for service subscribe request
 */
void handleEventSUBSCRIBE(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "application/json");

  int8_t rc = -1, idx = 0;

  // check api key
  if(!checkAPIKey(req, res)) {
    return;
  }

  if ((rc = addSubscriber(req, &idx)) > -1) {
    // Success
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe pass rc(%i) idx(%i) uuid(%s)\n", rc, idx, rest_subscribers[idx]->uuid->c_str());
  } else {
    // Printf error
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe fail rc(%i) idx(%i)\n", rc, idx);
    // discard remaining data from client
    req->discardRequestBody();
    // We could not store the subscriber, no free slot.
    res->setStatusCode(507);
    res->setStatusText("Insufficient storage");
    res->println("507 Insufficient storage");
  }
}

/**
 * HTTP response for service unsubscribe request
 */
void handleEventUNSUBSCRIBE(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "text/xml");

  int8_t rc = -1, idx = -1; // idx = -1 to remove if found
  if ((rc = addSubscriber(req, &idx)) > -1) {
    // Success
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe delete pass rc(%i)\n", rc);
  } else {
    // Printf error
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe delete fail rc(%i)\n", rc);
    // We could not find subscriber slot.
    res->setStatusCode(409);
    res->setStatusText("Application not subscribed to event source");
    res->println("409 Application not subscribed to event source");
  }
}
#endif // EN_REST
#endif // EN_HTTP || EN_HTTPS

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
