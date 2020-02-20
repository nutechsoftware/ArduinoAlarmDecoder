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
#define EN_SWAGGER_UI


/**
 * Base identity settings
 */
#define BASE_HOST_NAME  "AD2EMB"

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
#define SSDP_MAX_SUBSCRIBERS 5

static const char _alarmdecoder_device_schema_xml[] PROGMEM =
R"device_schema_xml(
<?xml version="1.0" encoding="UTF-8"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
   <specVersion>
      <major>1</major>
      <minor>0</minor>
   </specVersion>
   <device>
      <deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>
      <friendlyName>AlarmDecoder Embedded IoT</friendlyName>
      <presentationURL>/</presentationURL>
      <modelName>AD2ESP32</modelName>
      <modelNumber>2.0</modelNumber>
      <modelURL>https://github.com/nutechsoftware/ArduinoAlarmDecoder</modelURL>
      <manufacturer>Nu Tech Software, Solutions, Inc.</manufacturer>
      <manufacturerURL>http://www.AlarmDecoder.com/</manufacturerURL>
      <UDN>uuid:%s</UDN>
      <serialNumber>00000000</serialNumber>
      <iconList>
         <icon>
            <mimetype>image/png</mimetype>
            <height>32</height>
            <width>32</width>
            <depth>24</depth>
            <url>ad2icon.png</url>
         </icon>
      </iconList>
      <serviceList>
         <service>
            <serviceType>urn:schemas-upnp-org:service:AlarmDecoder:1</serviceType>
            <serviceId>urn:upnp-org:serviceId:AlarmDecoder:1</serviceId>
            <SCPDURL>AlarmDecoder.xml</SCPDURL>
            <eventSubURL>/api/v1/alarmdecoder/event</eventSubURL>
            <controlURL>/api/v1/alarmdecoder</controlURL>
         </service>
      </serviceList>
   </device>
</root>
)device_schema_xml";

// FIXME: sample not valid.
static const char _alarmdecoder_service_schema_xml[] PROGMEM =
R"service_schema_xml(
<?xml version="1.0" encoding="UTF-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
   <specVersion>
      <major>1</major>
      <minor>0</minor>
   </specVersion>
   <actionList>
      <action>
         <name>SetTarget</name>
         <argumentList>
            <argument>
               <name>newTargetValue</name>
               <relatedStateVariable>Target</relatedStateVariable>
               <direction>in</direction>
            </argument>
         </argumentList>
      </action>
      <action>
         <name>GetTarget</name>
         <argumentList>
            <argument>
               <name>RetTargetValue</name>
               <relatedStateVariable>Target</relatedStateVariable>
               <direction>out</direction>
            </argument>
         </argumentList>
      </action>
      <action>
         <name>GetStatus</name>
         <argumentList>
            <argument>
               <name>ResultStatus</name>
               <relatedStateVariable>Status</relatedStateVariable>
               <direction>out</direction>
            </argument>
         </argumentList>
      </action>
   </actionList>
   <serviceStateTable>
      <stateVariable sendEvents="no">
         <name>Target</name>
         <dataType>boolean</dataType>
         <defaultValue>0</defaultValue>
      </stateVariable>
      <stateVariable sendEvents="yes">
         <name>Status</name>
         <dataType>boolean</dataType>
         <defaultValue>0</defaultValue>
      </stateVariable>
   </serviceStateTable>
</scpd>
)service_schema_xml";
#endif // EN_SSDP

#if defined(EN_SWAGGER_UI)
static const char _swagger_ui_html[] PROGMEM =
R"swagger_ui(
<!DOCTYPE html>
<html>

<head>
    <meta charset="UTF-8">
    <meta http-equiv="x-ua-compatible" content="IE=edge">
    <title>Swagger UI</title>
    <link href='https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css' media='screen' rel='stylesheet' type='text/css' />
    <link href='https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/2.2.10/css/screen.css' media='screen' rel='stylesheet' type='text/css' />
    <script>
        if (typeof Object.assign != 'function') {
            (function() {
                Object.assign = function(target) {
                    'use strict';
                    if (target === undefined || target === null) {
                        throw new TypeError('Cannot convert undefined or null to object');
                    }
                    var output = Object(target);
                    for (var index = 1; index < arguments.length; index++) {
                        var source = arguments[index];
                        if (source !== undefined && source !== null) {
                            for (var nextKey in source) {
                                if (Object.prototype.hasOwnProperty.call(source, nextKey)) {
                                    output[nextKey] = source[nextKey];
                                }
                            }
                        }
                    }
                    return output;
                };
            })();
        }
    </script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/jquery/1.8.0/jquery-1.8.0.min.js' type='text/javascript'></script>
    <script>
        (function(b) {
            b.fn.slideto = function(a) {
                a = b.extend({
                    slide_duration: "slow",
                    highlight_duration: 3E3,
                    highlight: true,
                    highlight_color: "#FFFF99"
                }, a);
                return this.each(function() {
                    obj = b(this);
                    b("body").animate({
                        scrollTop: obj.offset().top
                    }, a.slide_duration, function() {
                        a.highlight && b.ui.version && obj.effect("highlight", {
                            color: a.highlight_color
                        }, a.highlight_duration)
                    })
                })
            }
        })(jQuery);
    </script>
    <script>
        jQuery.fn.wiggle = function(o) {
            var d = {
                speed: 50,
                wiggles: 3,
                travel: 5,
                callback: null
            };
            var o = jQuery.extend(d, o);
            return this.each(function() {
                var cache = this;
                var wrap = jQuery(this).wrap(' <div class="wiggle-wrap"></div>').css("position", "relative");
                var calls = 0;
                for (i = 1; i <= o.wiggles; i++) {
                    jQuery(this).animate({
                        left: "-=" + o.travel
                    }, o.speed).animate({
                        left: "+=" + o.travel * 2
                    }, o.speed * 2).animate({
                        left: "-=" + o.travel
                    }, o.speed, function() {
                        calls++;
                        if (jQuery(cache).parent().hasClass('wiggle-wrap')) {
                            jQuery(cache).parent().replaceWith(cache);
                        }
                        if (calls == o.wiggles && jQuery.isFunction(o.callback)) {
                            o.callback();
                        }
                    });
                }
            });
        };
    </script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/jquery.ba-bbq/1.2.1/jquery.ba-bbq.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/handlebars.js/4.0.5/handlebars.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/lodash-compat/3.10.1/lodash.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/backbone.js/1.1.2/backbone-min.js' type='text/javascript'></script>
    <script>
        Backbone.View = (function(View) {
            return View.extend({
                constructor: function(options) {
                    this.options = options || {};
                    View.apply(this, arguments);
                }
            });
        })(Backbone.View);
    </script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/2.2.10/swagger-ui.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.10.0/highlight.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.10.0/languages/json.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/json-editor/0.7.28/jsoneditor.min.js' type='text/javascript'></script>
    <script src='https://cdnjs.cloudflare.com/ajax/libs/marked/0.3.6/marked.min.js' type='text/javascript'></script>
    <script type="text/javascript">
        $(function() {
            url = "http://192.168.1.1/alarmdecoder.json";
            hljs.configure({
                highlightSizeThreshold: 5000
            });
            window.swaggerUi = new SwaggerUi({
                url: url,
                dom_id: "swagger-ui-container",
                supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch'],
                validatorUrl: null,
                onComplete: function(swaggerApi, swaggerUi) {},
                onFailure: function(data) {
                    log("Unable to Load SwaggerUI");
                },
                docExpansion: "none",
                jsonEditor: false,
                defaultModelRendering: 'schema',
                showRequestHeaders: false,
                showOperationIds: false
            });
            window.swaggerUi.load();

            function log() {
                if ('console' in window) {
                    console.log.apply(console, arguments);
                }
            }
        });
    </script>
</head>

<body class="swagger-section">
    <div id='header'>
        <div class="swagger-ui-wrap">
            <a id="logo" href="http://swagger.io"><img class="logo__img" alt="swagger" height="30" width="30" src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/2.2.10/images/logo_small.png" /><span class="logo__title">swagger</span></a>
            <form id='api_selector'></form>
        </div>
    </div>
    <div id="message-bar" class="swagger-ui-wrap" data-sw-translate>&nbsp;</div>
    <div id="swagger-ui-container" class="swagger-ui-wrap"></div>
</body>

</html>
)swagger_ui";

static const char _alarmdecoder_swagger_json[] PROGMEM =
R"ad2swaggerjson(
{
   "swagger":"2.0",
   "info":{
      "description":"This is a sample server Petstore server.",
      "version":"1.0.0",
      "title":"IoT application"
   },
   "host":"192.168.1.1",
   "tags":[
      {
         "name":"Temperature",
         "description":"Getting temperature measurements"
      }
   ],
   "paths":{
      "/temperature":{
         "get":{
            "tags":[
               "Temperature"
            ],
            "summary":"Endpoint for getting temperature measurements",
            "description":"",
            "operationId":"getTemperature",
            "responses":{
               "200":{
                  "description":"A list of temperature measurements",
                  "schema":{
                     "$ref":"#/definitions/temperatureMeasurement"
                  }
               }
            }
         }
      }
   },
   "definitions":{
      "temperatureMeasurement":{
         "type":"object",
         "properties":{
            "value":{
               "type":"string"
            },
            "timestamp":{
               "type":"string"
            }
         }
      }
   }
}
)ad2swaggerjson";

#endif
#endif // CONFIG_H
