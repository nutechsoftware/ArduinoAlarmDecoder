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
#include <PubSubClient.h>
#endif // EN_MQTT_CLIENT

/**
 * SSDP Services.
 *
 * Not in library manager.
 * cd ${home}/Arduino/libraries
 * git clone https://github.com/f34rdotcom/ESP32SSDP.git
 *  FIXME: The library needs work so I cloned it from for now.
 *    TODO: update parent https://github.com/luc-github/ESP32SSDP.git
 *    TODO: Add ModelDescription
 *    TODO: Add ability to set uuid
 */
#if defined(EN_SSDP)
#include <ESP32SSDP.h>
#endif


/**
 * HTTPServer/HTTPServer server  v0.3.1 by Frank Hessel
 * Library manager 'esp32_https_server'
 * https://github.com/fhessel/esp32_https_server
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
static uint32_t time_now = 0;

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

// SSDP service
#if defined(EN_SSDP)
typedef struct {
  unsigned long expire_time;
  String *host;
  String *callback;
  String *uuid;
} ssdp_subscriber_item_t;
ssdp_subscriber_item_t *ssdp_subscribers[SSDP_MAX_SUBSCRIBERS] = {};
std::map<String, uint32_t> TIME_MULTIPLIER = {{"SECONDS", 1 * 1000},{"MINUTES", 60 * 1000},{"HOURS", 3600 * 1000},{"DAYS", 86400 * 1000}};
#endif

#if defined(EN_HTTP) || defined(EN_HTTPS)
using namespace httpsserver;
#endif
// HTTP/HTTPS server
#if defined(EN_HTTPS)
// The HTTPS Server comes in a separate namespace. For easier use, include it here.
// Create an SSL certificate object from the files included above
SSLCert cert = SSLCert(
  example_crt_DER, example_crt_DER_len,
  example_key_DER, example_key_DER_len
);
HTTPSServer secureServer = HTTPSServer(&cert);
#endif
#if defined(EN_HTTP)
HTTPServer insecureServer = HTTPServer();
#endif
#if defined(EN_HTTP) || defined(EN_HTTPS)
// static resources
#include "src/static/favicon.h"
// Declare some handler functions for the various URLs on the server
void handleRoot(HTTPRequest * req, HTTPResponse * res);
void handleFavicon(HTTPRequest * req, HTTPResponse * res);
void handleAD2icon(HTTPRequest * req, HTTPResponse * res);
void handle404(HTTPRequest * req, HTTPResponse * res);
void handleDeviceDescription(HTTPRequest * req, HTTPResponse * res);
void handleServiceDescription(HTTPRequest * req, HTTPResponse * res);
void handleEventSUBSCRIBE(HTTPRequest * req, HTTPResponse * res);
void handleEventUNSUBSCRIBE(HTTPRequest * req, HTTPResponse * res);
#endif

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
#endif

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
#endif

#if defined(EN_WIFI)
  // Start wifi
  Serial.println("!DBG:AD2EMB,WiFi Start. Wait for interface");
  WiFi.setHostname(BASE_HOST_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
#endif

#if defined(EN_MQTT_CLIENT)
#if defined(SECRET_MQTT_SERVER_CERT)
#endif
  mqttSetup();
#endif

#if defined(EN_SSDP)
    SSDP.setSchemaURL("device_description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("AlarmDecoder Embedded IoT");
    SSDP.setSerialNumber("00000000");
    SSDP.setURL("/");
    SSDP.setModelName(BASE_HOST_NAME);
    SSDP.setModelNumber("2.0");
    //FIXME: SSDP.setModelDescription("AlarmDecoder Arduino embedded IoT appliance.");
    SSDP.setModelURL("https://github.com/nutechsoftware/alarmdecoder-embedded");
    SSDP.setManufacturer("Nu Tech Software, Solutions, Inc.");
    SSDP.setManufacturerURL("http://www.alarmdecoder.com/");
    //SSDP.setDeviceType("upnp:rootdevice");
    //SSDP.setDeviceType("urn:schemas-upnp-org:device:Basic:1");
    SSDP.setDeviceType("urn:schemas-upnp-org:device:AlarmDecoder:1");
    //FIXME: set <serviceList><server>
    //FIXME: set uuid
#endif

#if defined(EN_HTTP) || defined(EN_HTTPS)
ResourceNode * nodeRoot = new ResourceNode("/", "GET", &handleRoot);
ResourceNode * nodeFavicon = new ResourceNode("/favicon.ico", "GET", &handleFavicon);
ResourceNode * nodeAD2icon = new ResourceNode("/ad2icon.png", "GET", &handleAD2icon);
ResourceNode * node404  = new ResourceNode("", "GET", &handle404);
ResourceNode * nodeDeviceDescription = new ResourceNode("/device_description.xml", "GET", &handleDeviceDescription);
ResourceNode * nodeServiceDescription = new ResourceNode("/AlarmDecoder.xml", "GET", &handleServiceDescription);
ResourceNode * nodeEventSUBSCRIBE = new ResourceNode("/alarmdecoder/event", "SUBSCRIBE", &handleEventSUBSCRIBE);
ResourceNode * nodeEventUNSUBSCRIBE = new ResourceNode("/alarmdecoder/event", "UNSUBSCRIBE", &handleEventUNSUBSCRIBE);
#if defined(EN_HTTP)
  insecureServer.registerNode(nodeRoot);
  insecureServer.registerNode(nodeFavicon);
  insecureServer.registerNode(nodeAD2icon);
  insecureServer.registerNode(nodeDeviceDescription);
  insecureServer.registerNode(nodeServiceDescription);
  insecureServer.registerNode(nodeEventSUBSCRIBE);
  insecureServer.registerNode(nodeEventUNSUBSCRIBE);
  insecureServer.setDefaultNode(node404);
#endif
#if defined(EN_HTTPS)
  secureServer.registerNode(nodeRoot);
  secureServer.registerNode(nodeFavicon);
  secureServer.registerNode(nodeAD2icon);
  secureServer.registerNode(nodeDeviceDescription);
  secureServer.registerNode(nodeServiceDescription);
  secureServer.setDefaultNode(node404);
#endif
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

  // AD2* message processing
  ad2Loop();
  
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
  if (tlaps > 1000) {
     Serial.printf("!DBG:AD2EMB,TLAPS EXCEPTION A: %i\n",tlaps);
  }

#if defined(EN_WIFI)
  static int32_t wifi_reconnect_delay = 0;
  if (wifi_ready && !wifi_connected) {
    if (!wifi_reconnect_delay) {
      wifi_reconnect_delay = WIFI_CONNECT_RETRY_INTERVAL;
    } else {
      wifi_reconnect_delay -= tlaps;
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
    mqttLoop(tlaps);
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
    } else {
      // remove expired subscriptions
      for (int n = 0; n < SSDP_MAX_SUBSCRIBERS; n++) {
        if (ssdp_subscribers[n]) {
          if (time_now > ssdp_subscribers[n]->expire_time) {
             freeSubscriberLOC(n);
          }
        }
      }
    }
#endif

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
#endif

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
#endif  
      
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
void mqttLoop(uint32_t tlaps) {
    /// delay: counters must be signed to easily detect overflow by going negative
    static int32_t mqtt_reconnect_delay = 0;
    static int32_t mqtt_ping_delay = 0;
  
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
        mqtt_reconnect_delay -= tlaps;
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
      mqtt_ping_delay -= tlaps;
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
      Serial.printf("!DBG:AD2EMB,TLAPS EXCEPTION B: %i\n",mqtt_ping_delay);
    }
}
#endif

#if defined(EN_HTTP) || defined(EN_HTTPS)
// The hanlder functions are the same as in the Static-Page example.
// The only difference is the check for isSecure in the root handler
void handleRoot(HTTPRequest *req, HTTPResponse *res) {
  res->setHeader("Content-Type", "text/html");
  // Send content
  res->printf(_alarmdecoder_root_html,(int)(millis()/1000),req->isSecure() ? "HTTPS" : "HTTP");
}

/**
 * HTTP 404 not found error resopnse
 */
void handle404(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  res->setStatusCode(404);
  res->setStatusText("Not Found");
  res->setHeader("Content-Type", "text/html");
  // Send content
  res->print(_alarmdecoder_404_html);
}

/**
 * HTTP response for favicon icon file
 */
void handleFavicon(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "image/vnd.microsoft.icon");
  // Send content
  res->write(favicon_ico, favicon_ico_len);
}

/**
 * HTTP resposne for AD2 Icon
 */
void handleAD2icon(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "image/png");
  // Send content
  res->write(ad2icon_png, ad2icon_png_len);
}


#if defined(EN_SSDP)
/**
 * Attempt to locate a free subscriber slot and fill it with subscriber info.
 * Set idx to < 0 to remove subscriber.
 *
 * return
 *     1: success updated entry
 *     0: success new entry
 *    -1: fail general
 *    -2: fail malloc fail
 *    -3: fail no slots
 *    -4: delete not found
 */
int8_t addSubscriber(HTTPRequest *req, int8_t *idx) {
  int8_t ret = 0;

  // get key subscribe headers for searching
  String _host = req->getHeader("HOST").c_str();
  String _callback = req->getHeader("CALLBACK").c_str();
  String _timeout = req->getHeader("TIMEOUT").c_str();

  // check for host + callback match
  bool found = false; int16_t loc = -1;
  for (int n = 0; n < SSDP_MAX_SUBSCRIBERS; n++) {
    if (ssdp_subscribers[n]) {
      if (_host.equals(*ssdp_subscribers[n]->host) &&
          _callback.equals(*ssdp_subscribers[n]->callback))
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
      ssdp_subscribers[loc] = new ssdp_subscriber_item_t;
      ssdp_subscribers[loc]->host = new String(_host);
      ssdp_subscribers[loc]->callback = new String(_callback);
      ssdp_subscribers[loc]->uuid = new String;
      genUUID(loc+1, *ssdp_subscribers[loc]->uuid);
      ssdp_subscribers[loc]->expire_time = millis() + (TIME_MULTIPLIER[tkey] * tval);
      *idx = loc;
    } else {
      if (*idx > -1) {
        // delete not found
        ret = -4;
      } else {
        // no free slot
        ret = -3;
      }
    }
  } else {
    if (*idx < 0) {
      // found existing delete requested.
      *idx = loc;
      freeSubscriberLOC(loc);
    } else {
      // found existing set new exire time.
      ssdp_subscribers[loc]->expire_time = millis() + (TIME_MULTIPLIER[tkey] * tval);
      ret = 1;
    }
    *idx = loc;
  }
  return ret;
}

/**
 * Free contents of subscriber
 */
void freeSubscriberLOC(uint8_t loc) {
  Serial.printf("!DBG:AD2EMB,SSDP freeSubscriberLOC loc(%i) uuid(%s)\n", loc, ssdp_subscribers[loc]->uuid->c_str());
  if (ssdp_subscribers[loc]->host)
    delete ssdp_subscribers[loc]->host;
  if (ssdp_subscribers[loc]->callback)
    delete ssdp_subscribers[loc]->callback;
  if (ssdp_subscribers[loc]->uuid)
    delete ssdp_subscribers[loc]->uuid;
  delete ssdp_subscribers[loc];
  ssdp_subscribers[loc] = 0;
}

/**
 * HTTP response for device description xml file
 */
void handleDeviceDescription(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "text/xml");
  // Send schema
  //String schema;
  //SSDP.schema(schema);
  //res->write((uint8_t*)schema.c_str(), schema.length());
  res->print(_alarmdecoder_device_schema_xml);
}

/**
 * HTTP response for service description xml file
 */
void handleServiceDescription(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "text/xml");
  // Write data from header file
  res->printf(_alarmdecoder_service_schema_xml);
}

/**
 * HTTP response for service subscribe request
 */
void handleEventSUBSCRIBE(HTTPRequest *req, HTTPResponse *res) {
  // Set Content-Type
  res->setHeader("Content-Type", "text/xml");

  int8_t rc = -1, idx = 0;
  if ((rc = addSubscriber(req, &idx)) > -1) {
    // Success
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe pass rc(%i) idx(%i) uuid(%s)\n", rc, idx, ssdp_subscribers[idx]->uuid->c_str());
  } else {
    // Printf error
    Serial.printf("!DBG:AD2EMB,SSDP addSubscribe fail rc(%i) idx(%i)\n", rc, idx);
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
  }

  // Write data from header file
  res->printf(_alarmdecoder_service_schema_xml);
}
#endif // EN_SSDP
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
