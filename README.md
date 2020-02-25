# AlarmDecoder Arduino library for ESP32

This project provides a framework for building a network IoT appliance to monitor and control one or thousands of alarm panels using an AD2pHAT from AlarmDecoder.com

The AD2EmbeddedIot Arduino sketch example has the following features that can be selectively enabled or disabled.
- HTTP/HTTPS web server with REST API.
- MQTT/MQTTS client to connect to public or private MQTT/MQTTS broker.
- SSDP UPNP. Provides discovery capabilities with Hubitat Elevation™, SmartThings™ hub and other SSDP capable controllers.
- Socket server. Allows connections from the AlarmDecoder keypad GUI app, Micasaverde VERA, Hasbian and other systems that support TCP/IP connections.
- AlarmDecoder protocol pass through. Connect the ESP32 USB port to a host computer to access the AD2 device directly using a serial terminal.

Using an Olimex ESP32-EVB-EA it takes 6 seconds after booting to connect to the network and start sending messages.

## Arduino IDE Setup
- Use the latest Arduino IDE(1.8.x)
  - https://www.arduino.cc/en/main/software
- Add support for ESP32 using the Boards Manager.
 - Goto Tools > Board > Boards Manager...
 - Search for 'ESP32 by Espressif Systems' press install.
- Install esp32fs plugin(1.0) for SPIFFS support.
  - https://github.com/me-no-dev/arduino-esp32fs-plugin

## Getting the code

## Configuring
- Set partition scheme in Arduino IDE
  - Minimal SPIFFS (Large APPS with OTA)

## Building

## Contributors
 - Submit issues and contribute improvements on [github/nutechsoftware](https://github.com/nutechsoftware)

## Authors
 - Sean Mathews <coder@f34r.com> - Initial skeleton and R&D

## License
 - [Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)
