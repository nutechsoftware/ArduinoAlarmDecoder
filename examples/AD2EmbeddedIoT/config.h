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
#define BASE_HOST_NAME  "AD2EMB"
#define BASE_HOST_VERSION    "1.0"

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
 * Base file system settings
 * WARNING. Max file name length including this path is 32 bytes.
 */
#define FS_PUBLIC_PATH "/pub"

/**
 * Base WiFi settings
 */
#if defined(EN_WIFI)
#define WIFI_CONNECT_RETRY_INTERVAL 8 * 1000 * 1000 // (µs) 8 second delay
#endif

/**
 * Base MQTT settings
 * [/SECRETROOT/]AD2EMB_PATH/[CONTROL|STREAM|EVENT]/FOO
 *
 * Test with mosquitto client capture all STREAM topics
 *   // subscribe all STREAM sub tree for a specific client
 *   mosquitto_sub -u admin -P admin -t 'AD2EMB/{CLIENT ID}/STREAM/+' -h localhost -d
 *   // Admin listen to all clients for PING
 *   mosquitto_sub -u admin -P admin -t 'AD2EMP/+/EVENT/PING'
 */
#if defined(EN_MQTT_CLIENT)
#define MQTT_CONNECT_RETRY_INTERVAL ( 5 * 1000 * 1000) // (µs) reconnect attempt every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL  (60 * 1000 * 1000) // (µs) Publish PING to subscribers every 60 seconds
#define MQTT_AD2EMB_PATH BASE_HOST_NAME "/"
// input topics
#define MQTT_CMD_SUB_TOPIC  "CONTROL/CMD"   // Subscribe for remote control. Arm/Disarm, Compass, configuration, etc.
// output topics
#define MQTT_PING_PUB_TOPIC  "EVENT/PING"   // Client sends a PING event with ID to notify subscriber(admin) the client is alive.
#define MQTT_LRR_PUB_TOPIC   "STREAM/LRR"   // LRR message topic
#define MQTT_KPM_PUB_TOPIC   "STREAM/KPM"   // Keypad message and state bits topic "Armed ready to arm etc"
#define MQTT_AUI_PUB_TOPIC   "STREAM/AUI"   // AUI message topic
#define MQTT_RFX_PUB_TOPIC   "STREAM/RFX"   // RFX message topic
#define MQTT_REL_PUB_TOPIC   "STREAM/REL"   // Relay message topic
#define MQTT_EXP_PUB_TOPIC   "STREAM/EXP"   // Expander message topic
#endif

/**
 * HTTP/HTTPS setings
 */
#if defined(EN_HTTP) || defined(EN_HTTPS)
#define HTTP_DIR_INDEX "index.html"
#define HTTP_PORT 80
#define HTTPS_PORT 443
#define HTTP_API_BASE "/api/alarmdecoder"
#endif // EN_HTTP || EN_HTTPS

/**
 * SSDP setings
 */
#if defined(EN_SSDP)
#define SSDP_MAX_SUBSCRIBERS 5
#endif // EN_SSDP

#endif // CONFIG_H
