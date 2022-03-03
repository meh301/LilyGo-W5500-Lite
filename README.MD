# LilyGo W5500 Lite

- Need to rely on two library files in libdeps

- WebServer example, because the original library is not compatible with ESP32, has been modified, if you need to run, please copy the folder in the libdeps directory to the Libraries directory
- Arduino IDE board select `ESP32 Dev Module` for the board

## Use Pins

```
ETH_RST         4
ETH_CS          5
ETH_SCLK       18
ETH_MISO       23
ETH_MOSI       19
OLED_SCL       22
OLED_SDA       21
```

## This fork includes sketches for the T-LITE + optional ethernet gateway

- `T-LITE_WoL` is an IoT-enabled (websocket) Wake-on-Lan dongle with a WiFi configuration interface.

  - Input the local WiFi details, target MAC address, and websocket connection in the configuration settings. Default WiFi password is `WoL_Config`.
  - On boot, the T-LITE will transmit over WiFi to the websocket server the target mac address as confirmation. To send a magic packet, you must send the mac address as a string back to the T-LITE. It will then respond with a string `"Starting PC" + macaddress`.
  - Configuration mode can be accessed using the ESP_DoubleResetDetector library. Simply reset the T-LITE twice within 10 seconds.

- `T-LITE_WebConfig` allows you to setup the ethernet gateway as a static connection or automatic DHCP connection, as well as configure the WiFi connection. After that, simply add in whatever code you may need. Default WiFi password is `Wlan_Config`.

- Included in the examples folder are two sketches demonstrating the double client and double server capabilities of the T-LITE based on the `T-LITE_WebConfig` sketch.
