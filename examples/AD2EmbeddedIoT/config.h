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

// 

/**
 * Base MQTT settings
 */
#if defined(EN_MQTT_CLIENT)
#define MQTT_CONNECT_RETRY_INTERVAL 5000   // every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL 60000   // every 60 seconds
#endif

#endif // CONFIG_H
