#ifndef CONFIG_H
#define CONFIG_H

/**
 *  Enable Base features/service
 *  
 *  It may not be possible to enable all at the same time.
 *  This will depend on the hardware code and ram space.
 */
#define EN_ETH
//#define EN_WIFI
#define EN_MQTT_CLIENT
//#define EN_SSDP
//#define EN_HTTP
//#define EN_HTTPS
//#define EN_REST


/**
 * Base identity settings
 */
#define BASE_HOST_NAME  "AD2ESP32"

/**
 * Base network settings
 */
// IP address in case DHCP fails
IPAddress dhcp_fail_ip(169,254,0,123);
IPAddress dhcp_fail_subnet(255,255,0,0);
IPAddress dhcp_fail_gw(169,254,0,1);

/**
 * AlarmDecoder setings
 *   [AD2_SOCK] Use a socket connection for testing. This allows an easy way to test code
 *   and not be near an alarm panel.
 *   [AD2_UART] The AD2* is directly connected to this host on a UART port.
 */
#define AD2_UART
//#define AD2_SOCK

/**
 * Base embedded hardware setup
 * FIXME: needs design work.
 */
#define HW_ESP32_EVB_EA

//#define HW_ESP32_TTGO_TBEAM10
//#define HW_ESP32_THING

// OLIMAX ESP32-EVB-EA HARDWARE SETTINGS
// Connect the AD2* TX PIN to ESP32 RX PIN and AD2* RX PIN to ESP32 TX PIN
#if defined(HW_ESP32_EVB_EA)
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

/**
 * Base WiFi settings
 */
#if defined(EN_WIFI)
#define WIFI_CONNECT_RETRY_INTERVAL 8000   // 8 seconds
#endif

/**
 * Base MQTT settings
 */
#if defined(EN_MQTT_CLIENT)
#define MQTT_CONNECT_RETRY_INTERVAL 5000   // every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL 60000   // every 60 seconds
#endif

/**
 * HTTP/HTTPS setings
 */
#if defined(EN_HTTP) || defined(EN_HTTPS)
static const char _alarmdecoder_root_html[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>AD2EmbeddedIoT web services</title>"
"<link rel=\"Shortcut Icon\" href=\"/favicon.ico\" type=\"image/x-icon\">"
"</head>"
"<body>"
"<h1>AlarmDecoder Embedded IoT web interface coming soon!</h1>"
"<p>Your server is running for %i seconds.</p>"
"<p>You are connected via <strong>%s</strong>.</p>"
"</body>"
"</html>";

static const char _alarmdecoder_404_html[] PROGMEM =
"<!DOCTYPE html>"
"<html>"
"<head><title>Not Found</title></head>"
"<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>"
"</html>";
#endif

/**
 * SSDP setings
 */
#if defined(EN_SSDP)
static const char _alarmdecoder_device_schema_xml[] PROGMEM =
"<?xml version=\"1.0\"?>"
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
  "<specVersion>"
    "<major>1</major>"
    "<minor>0</minor>"
  "</specVersion>"
  "<device>"
    "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
    "<friendlyName>AlarmDecoder Embedded IoT</friendlyName>"
    "<presentationURL>/</presentationURL>"
    "<serialNumber>00000000</serialNumber>"
    "<modelName>AD2ESP32</modelName>"
    "<modelNumber>2.0</modelNumber>"
    "<modelURL>https://github.com/nutechsoftware/alarmdecoder-embedded</modelURL>"
    "<manufacturer>Nu Tech Software, Solutions, Inc.</manufacturer>"
    "<manufacturerURL>http://www.AlarmDecoder.com/</manufacturerURL>"
    "<UDN>uuid:38323636-4558-4dda-9188-cda0e60014e4</UDN>"
    "<iconList>"
      "<icon>"
       "<mimetype>image/png</mimetype>"
       "<height>32</height>"
       "<width>32</width>"
       "<depth>24</depth>"
       "<url>ad2icon.png</url>"
      "</icon>"
    "</iconList>"
    "<serviceList>"
      "<service>"
        "<serviceType>urn:schemas-upnp-org:service:AlarmDecoder:1</serviceType>"
        "<serviceId>urn:upnp-org:serviceId:AlarmDecoder:1</serviceId>"
        "<SCPDURL>AlarmDecoder.xml</SCPDURL>"
        "<eventSubURL>/api/v1/alarmdecoder/event</eventSubURL>"
        "<controlURL>/api/v1/alarmdecoder</controlURL>"
      "</service>"
    "</serviceList>"
  "</device>"
"</root>\r\n"
"\r\n";

// FIXME: sample not valid.
static const char _alarmdecoder_service_schema_xml[] PROGMEM =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
  "<specVersion>"
    "<major>1</major>"
    "<minor>0</minor>"
  "</specVersion>"
  "<actionList>"
    "<action>"
      "<name>SetTarget</name>"
      "<argumentList>"
        "<argument>"
          "<name>newTargetValue</name>"
          "<relatedStateVariable>Target</relatedStateVariable>"
          "<direction>in</direction>"
        "</argument>"
      "</argumentList>"
    "</action>"
    "<action>"
      "<name>GetTarget</name>"
      "<argumentList>"
        "<argument>"
          "<name>RetTargetValue</name>"
          "<relatedStateVariable>Target</relatedStateVariable>"
          "<direction>out</direction>"
        "</argument>"
      "</argumentList>"
    "</action>"
    "<action>"
      "<name>GetStatus</name>"
      "<argumentList>"
        "<argument>"
          "<name>ResultStatus</name>"
          "<relatedStateVariable>Status</relatedStateVariable>"
          "<direction>out</direction>"
        "</argument>"
      "</argumentList>"
    "</action>"
  "</actionList>"
  "<serviceStateTable>"
    "<stateVariable sendEvents=\"no\">"
      "<name>Target</name>"
      "<dataType>boolean</dataType>"
      "<defaultValue>0</defaultValue>"
    "</stateVariable>"
    "<stateVariable sendEvents=\"yes\">"
      "<name>Status</name>"
      "<dataType>boolean</dataType>"
      "<defaultValue>0</defaultValue>"
    "</stateVariable>"
  "</serviceStateTable>"
"</scpd>";
#endif // EN_SSDP
#endif // CONFIG_H
