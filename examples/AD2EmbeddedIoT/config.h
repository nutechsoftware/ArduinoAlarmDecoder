#ifndef CONFIG_H
#define CONFIG_H
/**
 *  Enable base features/service
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
 * Base identity settings
 */
#define BASE_HOST_NAME  "AD2ESP32"

/**
 * AD2* setings
 */
#define AD2_BAUD 115200

/**
 * Base network settings
 */
// Static IP or 0,0,0,0 for DHCP
//IPAddress static_ip(10,2,10,66);
IPAddress static_ip(0,0,0,0);
IPAddress static_subnet(255,255,255,0);
IPAddress static_gw(10,2,10,1);
IPAddress static_dns1(4,2,2,2);
IPAddress static_dns2(8,8,8,8);

// IP address in case DHCP fails
IPAddress dhcp_fail_ip(169,254,0,123);
IPAddress dhcp_fail_subnet(255,255,0,0);
IPAddress dhcp_fail_gw(169,254,0,1);

/**
 * Base embedded hardware setup
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

// 

/**
 * Base MQTT settings
 */
#if defined(EN_MQTT_CLIENT)
#define MQTT_CONNECT_RETRY_INTERVAL 5000   // every 5 seconds
#define MQTT_CONNECT_PING_INTERVAL 60000   // every 60 seconds
#endif

#endif // CONFIG_H
