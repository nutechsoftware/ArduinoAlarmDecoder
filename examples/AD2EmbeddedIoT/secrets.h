/**
 * Base network settings
 *  Static IP or 0,0,0,0 for DHCP
 */
IPAddress static_ip(0,0,0,0);
IPAddress static_subnet(255,255,255,0);
IPAddress static_gw(0,0,0,0);
IPAddress static_dns1(0,0,0,0);
IPAddress static_dns2(0,0,0,0);

/**
 * Base AlarmDecoder device settings
 */
#if defined(AD2_UART) && defined(AD2_SOCK)
#error Only one method of connecting to the AlarmDecoder can be defined [SOCK | UART]
#elif defined(AD2_UART)
// AD2 UART settings
#define AD2_BAUD 115200
#elif defined(AD2_SOCK)
// AD2 ser2sock settings
IPAddress AD2_SOCKIP(192, 168, 1, 2);
int AD2_SOCKPORT = 10000;
#else
#error Must define how the AlarmDecoder is connected. Via ser2sock or local hardware uart.
#endif

/**
 * WiFi SECRETS/SETTINGS
 */
#define SECRET_WIFI_SSID          ""
#define SECRET_WIFI_PASS          ""

/**
 * SSDP SECRETS/SETTINGS
 */
#define SECRET_SSDP_UUID_PREFIX    "416c6172-6d64-6563-6f64"

/**
 * MQTT Client SECRETS/SETTINGS
 *  select an MQTT server profile.
 */

// Unique ID for this AD2EMB device (FIXME USE NVRAM for this setting)
// must be unique and complex to avoid collisions on the network
// current max is 23 bytes.
#define SECRET_MQTT_CLIENT_ID "a952d8f4-5077-11ea-8d77"

// PASS: mosquitto.org clear text
#define MQTT_SERVER_CLEARTEXT_TEST_MOSQUITTO_ORG

// FAIL: mosquitto.org SSL
//#define MQTT_SERVER_SSL_TEST_MOSQUITO_ORG

// PASS: cloudmqtt.com SSL+AUTH
//#define MQTT_SERVER_SSLAUTH_POSTMAN_CLOUDMQTT_COM

// FAIL: dioty.co SSL+AUTH
//#define MQTT_SERVER_SSLAUTH_MQTT_DIOTY_CO

// PASS: dioty.co CLEARTEXT+AUTH
//#define MQTT_SERVER_CLEARTEXTAUTH_MQTT_DIOTY_CO

// PASS: fluux.io SSL
//#define MQTT_SERVER_SSL_MQTT_FLUUX_IO

/*
 * MQTT server Profiles
 */
// MQTT mosquitto.org CLEAR TEXT profile
#if defined(MQTT_SERVER_CLEARTEXT_TEST_MOSQUITTO_ORG)
#define SECRET_MQTT_ROOT          ""
#define SECRET_MQTT_SERVER        "test.mosquitto.org"
#define SECRET_MQTT_PORT          1883
#define SECRET_MQTT_USER          NULL
#define SECRET_MQTT_PASS          NULL

// MQTT SSL test.mosquitto.org profile
#elif defined(MQTT_SERVER_SSL_TEST_MOSQUITO_ORG)
#define SECRET_MQTT_ROOT          ""
#define SECRET_MQTT_SERVER        "test.mosquitto.org"
#define SECRET_MQTT_PORT          8883
#define SECRET_MQTT_USER          NULL
#define SECRET_MQTT_PASS          NULL

// openssl s_client -showcerts -connect test.mosquitto.org:8883
#define SECRET_MQTT_SERVER_CERT   "-----BEGIN CERTIFICATE-----\n" \
"MIIC8DCCAlmgAwIBAgIJAOD63PlXjJi8MA0GCSqGSIb3DQEBBQUAMIGQMQswCQYD\n" \
"VQQGEwJHQjEXMBUGA1UECAwOVW5pdGVkIEtpbmdkb20xDjAMBgNVBAcMBURlcmJ5\n" \
"MRIwEAYDVQQKDAlNb3NxdWl0dG8xCzAJBgNVBAsMAkNBMRYwFAYDVQQDDA1tb3Nx\n" \
"dWl0dG8ub3JnMR8wHQYJKoZIhvcNAQkBFhByb2dlckBhdGNob28ub3JnMB4XDTEy\n" \
"MDYyOTIyMTE1OVoXDTIyMDYyNzIyMTE1OVowgZAxCzAJBgNVBAYTAkdCMRcwFQYD\n" \
"VQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwGA1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1v\n" \
"c3F1aXR0bzELMAkGA1UECwwCQ0ExFjAUBgNVBAMMDW1vc3F1aXR0by5vcmcxHzAd\n" \
"BgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hvby5vcmcwgZ8wDQYJKoZIhvcNAQEBBQAD\n" \
"gY0AMIGJAoGBAMYkLmX7SqOT/jJCZoQ1NWdCrr/pq47m3xxyXcI+FLEmwbE3R9vM\n" \
"rE6sRbP2S89pfrCt7iuITXPKycpUcIU0mtcT1OqxGBV2lb6RaOT2gC5pxyGaFJ+h\n" \
"A+GIbdYKO3JprPxSBoRponZJvDGEZuM3N7p3S/lRoi7G5wG5mvUmaE5RAgMBAAGj\n" \
"UDBOMB0GA1UdDgQWBBTad2QneVztIPQzRRGj6ZHKqJTv5jAfBgNVHSMEGDAWgBTa\n" \
"d2QneVztIPQzRRGj6ZHKqJTv5jAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUA\n" \
"A4GBAAqw1rK4NlRUCUBLhEFUQasjP7xfFqlVbE2cRy0Rs4o3KS0JwzQVBwG85xge\n" \
"REyPOFdGdhBY2P1FNRy0MDr6xr+D2ZOwxs63dG1nnAnWZg7qwoLgpZ4fESPD3PkA\n" \
"1ZgKJc2zbSQ9fCPxt2W3mdVav66c6fsb7els2W2Iz7gERJSX\n" \
"-----END CERTIFICATE-----\n"

// MQTT cloudmqtt.com SSL+AUTH profile
// openssl s_client -showcerts -connect  postman.cloudmqtt.com:20445
#elif defined(MQTT_SERVER_SSLAUTH_POSTMAN_CLOUDMQTT_COM)
#define SECRET_MQTT_ROOT          ""
#define SECRET_MQTT_SERVER        "postman.cloudmqtt.com"
#define SECRET_MQTT_PORT          20445
#define SECRET_MQTT_USER          ""
#define SECRET_MQTT_PASS          ""
#define SECRET_MQTT_SERVER_CERT   "-----BEGIN CERTIFICATE-----\n" \
"MIIFdDCCBFygAwIBAgIQJ2buVutJ846r13Ci/ITeIjANBgkqhkiG9w0BAQwFADBv\n" \
"MQswCQYDVQQGEwJTRTEUMBIGA1UEChMLQWRkVHJ1c3QgQUIxJjAkBgNVBAsTHUFk\n" \
"ZFRydXN0IEV4dGVybmFsIFRUUCBOZXR3b3JrMSIwIAYDVQQDExlBZGRUcnVzdCBF\n" \
"eHRlcm5hbCBDQSBSb290MB4XDTAwMDUzMDEwNDgzOFoXDTIwMDUzMDEwNDgzOFow\n" \
"gYUxCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0ZXIxEDAO\n" \
"BgNVBAcTB1NhbGZvcmQxGjAYBgNVBAoTEUNPTU9ETyBDQSBMaW1pdGVkMSswKQYD\n" \
"VQQDEyJDT01PRE8gUlNBIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MIICIjANBgkq\n" \
"hkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAkehUktIKVrGsDSTdxc9EZ3SZKzejfSNw\n" \
"AHG8U9/E+ioSj0t/EFa9n3Byt2F/yUsPF6c947AEYe7/EZfH9IY+Cvo+XPmT5jR6\n" \
"2RRr55yzhaCCenavcZDX7P0N+pxs+t+wgvQUfvm+xKYvT3+Zf7X8Z0NyvQwA1onr\n" \
"ayzT7Y+YHBSrfuXjbvzYqOSSJNpDa2K4Vf3qwbxstovzDo2a5JtsaZn4eEgwRdWt\n" \
"4Q08RWD8MpZRJ7xnw8outmvqRsfHIKCxH2XeSAi6pE6p8oNGN4Tr6MyBSENnTnIq\n" \
"m1y9TBsoilwie7SrmNnu4FGDwwlGTm0+mfqVF9p8M1dBPI1R7Qu2XK8sYxrfV8g/\n" \
"vOldxJuvRZnio1oktLqpVj3Pb6r/SVi+8Kj/9Lit6Tf7urj0Czr56ENCHonYhMsT\n" \
"8dm74YlguIwoVqwUHZwK53Hrzw7dPamWoUi9PPevtQ0iTMARgexWO/bTouJbt7IE\n" \
"IlKVgJNp6I5MZfGRAy1wdALqi2cVKWlSArvX31BqVUa/oKMoYX9w0MOiqiwhqkfO\n" \
"KJwGRXa/ghgntNWutMtQ5mv0TIZxMOmm3xaG4Nj/QN370EKIf6MzOi5cHkERgWPO\n" \
"GHFrK+ymircxXDpqR+DDeVnWIBqv8mqYqnK8V0rSS527EPywTEHl7R09XiidnMy/\n" \
"s1Hap0flhFMCAwEAAaOB9DCB8TAfBgNVHSMEGDAWgBStvZh6NLQm9/rEJlTvA73g\n" \
"JMtUGjAdBgNVHQ4EFgQUu69+Aj36pvE8hI6t7jiY7NkyMtQwDgYDVR0PAQH/BAQD\n" \
"AgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAGBgRVHSAAMEQGA1UdHwQ9\n" \
"MDswOaA3oDWGM2h0dHA6Ly9jcmwudXNlcnRydXN0LmNvbS9BZGRUcnVzdEV4dGVy\n" \
"bmFsQ0FSb290LmNybDA1BggrBgEFBQcBAQQpMCcwJQYIKwYBBQUHMAGGGWh0dHA6\n" \
"Ly9vY3NwLnVzZXJ0cnVzdC5jb20wDQYJKoZIhvcNAQEMBQADggEBAGS/g/FfmoXQ\n" \
"zbihKVcN6Fr30ek+8nYEbvFScLsePP9NDXRqzIGCJdPDoCpdTPW6i6FtxFQJdcfj\n" \
"Jw5dhHk3QBN39bSsHNA7qxcS1u80GH4r6XnTq1dFDK8o+tDb5VCViLvfhVdpfZLY\n" \
"Uspzgb8c8+a4bmYRBbMelC1/kZWSWfFMzqORcUx8Rww7Cxn2obFshj5cqsQugsv5\n" \
"B5a6SE2Q8pTIqXOi6wZ7I53eovNNVZ96YUWYGGjHXkBrI/V5eu+MtWuLt29G9Hvx\n" \
"PUsE2JOAWVrgQSQdso8VYFhH2+9uRv0V9dlfmrPb2LjkQLPNlzmuhbsdjrzch5vR\n" \
"pu/xO28QOG8=\n" \
"-----END CERTIFICATE-----\n"

// MQTT mqtt.dioty.co CLEAR TEXT AUTH profile
#elif defined(MQTT_SERVER_CLEARTEXTAUTH_MQTT_DIOTY_CO)
#define SECRET_MQTT_ROOT          "/foo@example.com/"
#define SECRET_MQTT_SERVER        "mqtt.dioty.co"
#define SECRET_MQTT_PORT          1883
#define SECRET_MQTT_USER          "foo@example.com"
#define SECRET_MQTT_PASS          "bar"

// MQTT mqtt.dioty.co SSL AUTH profile
#elif defined(MQTT_SERVER_SSLAUTH_MQTT_DIOTY_CO)
#define SECRET_MQTT_ROOT          "/foo@example.com/"
#define SECRET_MQTT_SERVER        "mqtt.dioty.co"
#define SECRET_MQTT_PORT          8883
#define SECRET_MQTT_USER          "foo@example.com"
#define SECRET_MQTT_PASS          "bar"

// openssl s_client -showcerts -connect  mqtt.dioty.co:8883
#define SECRET_MQTT_SERVER_CERT_FAIL "-----BEGIN CERTIFICATE-----\n" \
"MIIDJzCCAg8CCQDqaZJSuv1dUDANBgkqhkiG9w0BAQUFADCBljELMAkGA1UEBhMC\n" \
"R0IxFzAVBgNVBAgMDkdyZWF0ZXIgTG9uZG9uMQ8wDQYDVQQHDAZMb25kb24xFzAV\n" \
"BgNVBAoMDlN0YXJ0IExlYW4gTHRkMQ4wDAYDVQQLDAVESW9UWTEVMBMGA1UEAwwM\n" \
"d3d3LmRpb3R5LmNvMR0wGwYJKoZIhvcNAQkBFg5hZG1pbkBkaW90eS5jbzAeFw0x\n" \
"NDA2MjUxNDAxNTJaFw0yNDA2MjIxNDAxNTJaMIGXMQswCQYDVQQGEwJHQjEXMBUG\n" \
"A1UECAwOR3JlYXRlciBMb25kb24xDzANBgNVBAcMBkxvbmRvbjEXMBUGA1UECgwO\n" \
"U3RhcnQgTGVhbiBMdGQxDjAMBgNVBAsMBURJb1RZMRYwFAYDVQQDDA1tcXR0LmRp\n" \
"b3R5LmNvMR0wGwYJKoZIhvcNAQkBFg5hZG1pbkBkaW90eS5jbzCBnzANBgkqhkiG\n" \
"9w0BAQEFAAOBjQAwgYkCgYEArYyFfhamOocnzdGqIe6eck14g/jkDAcw74SWVXmD\n" \
"ETH9UcVTTQO3I7vvsJFqZy/iw79/HQRaKTBlmQG5Q377ABpzWFKXpC4qwCI2E7UZ\n" \
"13xJnTYjfYg3GnMiJ+aLhUj+seKU+4oiFL3/ly6HHghl3ztvvdAOVQ72QN+0XGgQ\n" \
"lt0CAwEAATANBgkqhkiG9w0BAQUFAAOCAQEADQUGc1y1biMYppdK0LDOMGmsQcaq\n" \
"N5MCrgwmOeS/JdkrQ8cbnbgjtGIhxI840gMgW/qwXOYM2LQdBj2vUWF22XPbWG3m\n" \
"4PaNJnl7FeR1ULYEKCZ+EGcwGTbVz7wljA6TZsfSUruKpkeP4iT7MHBWkXk/JCLv\n" \
"3aYZxbOT61b6YfcBDsjqoDU35ydoyJTwjM4PDM7qMNneU7WbR453sLQL/G3ImsAv\n" \
"jOEKj2lmXYLJE8UP2oy9iUr9bF0Clvcw5IHY5VDJZEeepfoGSOsqCZHj/JW7m6NQ\n" \
"a3rxGOLJcGfeMyKpbSW3AZBoIK4+tcQPKSYecA0oz3AKKIhIklyyc1LudQ==\n" \
"-----END CERTIFICATE-----\n"

// https://github.com/wardv/DIoTY-ca_cert/blob/master/dioty_ca.crt
#define SECRET_MQTT_SERVER_CERT "-----BEGIN CERTIFICATE-----\n" \
"MIIEATCCAumgAwIBAgIJALlFBeielqZzMA0GCSqGSIb3DQEBBQUAMIGWMQswCQYD\n" \
"VQQGEwJHQjEXMBUGA1UECAwOR3JlYXRlciBMb25kb24xDzANBgNVBAcMBkxvbmRv\n" \
"bjEXMBUGA1UECgwOU3RhcnQgTGVhbiBMdGQxDjAMBgNVBAsMBURJb1RZMRUwEwYD\n" \
"VQQDDAx3d3cuZGlvdHkuY28xHTAbBgkqhkiG9w0BCQEWDmFkbWluQGRpb3R5LmNv\n" \
"MB4XDTE0MDYyNTEzNDk1N1oXDTI0MDYyMjEzNDk1N1owgZYxCzAJBgNVBAYTAkdC\n" \
"MRcwFQYDVQQIDA5HcmVhdGVyIExvbmRvbjEPMA0GA1UEBwwGTG9uZG9uMRcwFQYD\n" \
"VQQKDA5TdGFydCBMZWFuIEx0ZDEOMAwGA1UECwwFRElvVFkxFTATBgNVBAMMDHd3\n" \
"dy5kaW90eS5jbzEdMBsGCSqGSIb3DQEJARYOYWRtaW5AZGlvdHkuY28wggEiMA0G\n" \
"CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDTm+TcMLyWJmKkAz+Di4C32TIMX3Uj\n" \
"i9iq5S3huICrWibo8y5bFhbuWVS/0dP1HLTZVCazS9DLAcvS15KnP6gi19jANTEu\n" \
"mLPFwJFBRsxrdLx72yAQjHUzESzx/IyvTE2MsdpcYSFyWmhX+Fl3V3rj5TY+UJZm\n" \
"fMEKnPuhq5ZoSo8eRcw1zTyhMhvVup+IbSOcp3rcAzMsllt/q9oxC7BqbGzZgSH2\n" \
"YVg0f23Q7cNFMzaJPuGfheWvYQdondZaiwjN568GmDapfZvTdX21AVv9KN04tKTG\n" \
"plti1A69d+kVzGTXVRyQXAvYGgEHvftu0BBdGvigarEjGvBJdqhZsn/RAgMBAAGj\n" \
"UDBOMB0GA1UdDgQWBBSekA+7MDiMuu/Ivsv7rq4cHaQ5dzAfBgNVHSMEGDAWgBSe\n" \
"kA+7MDiMuu/Ivsv7rq4cHaQ5dzAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUA\n" \
"A4IBAQA4Qv9MTLHPPlJhTDsTiauvJ8nCRA5SrjACLT0RL1kFAVhZ7gwMkUv1ZG6E\n" \
"mu0qrjhsxymQNPFrTqIjVtcwbSGmEozl5OJJaLW3QSBBKuR4ucdMZQaQ26J+6CNg\n" \
"kzAVxN9AF1F3Vk9g9cVWRoJn7twyzB7y8WASXQOiuZrX/GBAnoOUG0LnOhZT9tE4\n" \
"sdrqMfCK00roIt08hBItaVKIX9hRcES4rXfZspx3br3qAMphcCB4PIxhcWWt1cNd\n" \
"dHUW6BEeRjuIZQrbeWPAwOm+/DMP9+h9+Y+Kp0Reu1Q98hio/sCyM6y4qAE/f2pi\n" \
"MplNSgyPNMDcpOyObMOvL9gRSwHG\n" \
"-----END CERTIFICATE-----\n"


// MQTT fluux.io SSL profile
#elif defined(MQTT_SERVER_SSL_MQTT_FLUUX_IO)
#define SECRET_MQTT_ROOT          ""
#define SECRET_MQTT_SERVER        "mqtt.fluux.io"
#define SECRET_MQTT_PORT          8883
#define SECRET_MQTT_USER          NULL
#define SECRET_MQTT_PASS          NULL

// openssl s_client -showcerts -connect mqtt.fluux.io:8883
#define SECRET_MQTT_SERVER_CERT   "-----BEGIN CERTIFICATE-----\n" \
"MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n" \
"SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n" \
"GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" \
"AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n" \
"q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n" \
"SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n" \
"Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n" \
"a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n" \
"/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n" \
"AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n" \
"CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n" \
"bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n" \
"c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n" \
"VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n" \
"ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n" \
"MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n" \
"Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n" \
"AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n" \
"uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n" \
"wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n" \
"X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n" \
"PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n" \
"KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n" \
"-----END CERTIFICATE-----\n"
#endif
