#define ESP_DRD_USE_SPIFFS true //this needs to be defined before all other code to ensure that it works properly - double reset detector
////////
#include <Arduino.h>
#include <Ethernet.h> //using modified library from TTGO T-LITE https://github.com/Xinyuan-LilyGO/LilyGo-W5500-Lite
#include <EthernetUdp.h> //using modifiedd library from TTGO T-LITE https://github.com/Xinyuan-LilyGO/LilyGo-W5500-Lite
#include <SPI.h>
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"` //using included library from TTGO T-LITE https://github.com/Xinyuan-LilyGO/LilyGo-W5500-Lite
#include <WiFi.h>
#include <WiFiManager.h> //WiFiManager library by Tablatronix https://github.com/tzapu/WiFiManager
#include <ArduinoWebsockets.h> //ArduinoWebsockets library by Gil Maimon https://github.com/gilmaimon/ArduinoWebsockets
#include "EEPROM.h" 
#include <ESP_DoubleResetDetector.h> //ESP_DoubleResetDetector by Khoi Hoang https://github.com/khoih-prog/ESP_DoubleResetDetector
#include <ArduinoJson.h> //ArduinoJson library by Benoit Blanchon https://arduinojson.org
#include <FS.h>
#include <SPIFFS.h>

#define     ETH_RST         4
#define     ETH_CS          5
#define     ETH_SCLK       18
#define     ETH_MISO       23
#define     ETH_MOSI       19
#define     OLED_SCL       22
#define     OLED_SDA       21
#define JSON_CONFIG_FILE "/config.json"
#define DRD_TIMEOUT 10 //the timeout for the double reset in seconds
#define DRD_ADDRESS 0 //default bit to check for double reset

DoubleResetDetector* drd;

uint8_t mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
uint8_t ip[] = {192,168,99,5}; //static ethernet configuration
uint8_t gateway[] = {192,168,99,1}; //static ethernet configuration
uint8_t subnet[] = {255,255,255,0}; //static ethernet configuration
bool shouldSaveConfig = false;
char macString[50] = ""; //MAC address input for webconfigurator
char websockString[50] = "ws://"; //websocket input for webconfigurator
uint64_t chipid = 0; //gets the esp32 mac address
uint16_t chip = 0; //makes the esp32 mac address shorter

EthernetUDP Udp;
WiFiManager wm; //WiFiManager setup based on https://github.com/witnessmenow/ESP32-WiFi-Manager-Examples Use Case 2
SSD1306Wire  display(0x3c, OLED_SDA, OLED_SCL);
using namespace websockets;
WebsocketsClient websockclient;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
/////////////////////////EXTRA FUNCTIONS//////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
    if (message.data() == String(macString)) {
      wakeMyPC();

    }
}
//////////////////////////////////////////////////////////////////////
void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}
//////////////////////////////////////////////////////////////////////
void wakeMyPC() { //Basic Wake on Lan implementation based on https://github.com/mikispag/arduino-WakeOnLan
  const char *MACAddress = macString;
  uint8_t MAC[6];
  char* ptr;
  MAC[0] = strtol(MACAddress, &ptr, HEX );
  for( uint8_t i = 1; i < 6; i++ )
  {
    MAC[i] = strtol(ptr+1, &ptr, HEX );
  } //This loop converts the mac address string to a byte array

  byte preamble[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //magic packets start with 6 bytes 
  byte _ip[4] = {255,255,255,255}; //general broadcast - you can set this to a specific subnet if needed, e.g 192.168.1.255
  byte port = 9; // default port for WoL, can be switched to others such as port 7
  byte i;
  
  Udp.beginPacket(_ip, port);
  Udp.write(preamble, sizeof preamble);
  for (i = 0; i < 16; i++) {
    Udp.write(MAC, sizeof preamble); // we loop the mac address 16 times to create the WoL packet
  }
  Udp.endPacket();
  Serial.println("Sent Magic Packet");
  websockclient.send("Starting PC "+String(macString));
}

//////////////////////////////////////////////////////////////////////
void saveConfigFile()
{
  Serial.println(F("Saving config"));
  StaticJsonDocument<512> json;
  json["macString"] = macString;
  json["websockString"] = websockString;
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}
//////////////////////////////////////////////////////////////////////
bool loadConfigFile()
{
Serial.println("mounting FS...");

  // May need to make it begin(true) first time you are using SPIFFS
  // NOTE: This might not be a good way to do this! begin(true) reformats the spiffs
  // it will only get called if it fails to mount, which probably means it needs to be
  // formatted, but maybe dont use this if you have something important saved on spiffs
  // that can't be replaced.
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("opened config file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("\nparsed json");

          strcpy(macString, json["macString"]);
          strcpy(websockString, json["websockString"]);

          return true;
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  //end read
  return false;
}
//////////////////////////////////////////////////////////////////////
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
//////////////////////////////////////////////////////////////////////
void configModeCallback(WiFiManager *myWiFiManager)
{
  display.clear();
  Serial.println("Entered Conf Mode");
  display.drawString(0, 0, "Configuring WoL Injector...");
  display.display();
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.drawString(0, 12, "Config SSID: "+String(myWiFiManager->getConfigPortalSSID()));

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
  display.drawString(0, 24, "Config IP: "+WiFi.softAPIP().toString());
  display.display();
}
//////////////////////////////////////////////////////////////////////
void ethernetReset(const uint8_t resetPin)
{
    pinMode(resetPin, OUTPUT);
    digitalWrite(resetPin, HIGH);
    delay(250);
    digitalWrite(resetPin, LOW);
    delay(50);
    digitalWrite(resetPin, HIGH);
    delay(350);
}
//////////////////////////////////////////////////////////////////////
void getchipid() {
  chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  chip = (uint16_t)(chipid >> 32);
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
////////////////////////////////SETUP/////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void setup()
{
    bool forceConfig = false; //set to true if developing
//  SPIFFS.format(); //emergency fix

    drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
    if (drd->detectDoubleReset())
    {
      Serial.println(F("Forcing config mode as there was a Double reset detected"));
      forceConfig = true;
    }
//////////////////////////////////////////////////////////////////////////////////////////////////  
    Serial.begin(115200);
    Serial.println("TTGO T-LITE Start");
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "ESP32 WIFI Start");
    display.display();
    getchipid();
//////////////////////////////////////////////////////////////////////////////////////////////////
    bool spiffsSetup = loadConfigFile();
    if (!spiffsSetup)
    {
      Serial.println(F("Forcing config mode as there is no saved config"));
      forceConfig = true;
    }
    WiFi.mode(WIFI_STA);
    delay(100);
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setAPCallback(configModeCallback);
    WiFiManagerParameter custom_text_box("mac_address", "Enter MAC Address", macString, 50);
    WiFiManagerParameter custom_text_box2("websocket_address", "Enter Websocket Address", websockString, 50);
    wm.addParameter(&custom_text_box);
    wm.addParameter(&custom_text_box2);
    char ssid[23];
    snprintf(ssid, 23, "WoL_%04X", chip, (uint32_t)chipid); //WiFi configurator SSID
    const char* password = "WoL_Config"; // WiFi configurator password
//////////////////////////////////////////////////////////////////////////////////////////////////
    if (forceConfig)
    {
      if (!wm.startConfigPortal(ssid, password))
      {
        Serial.println("failed to connect and hit timeout");
        display.clear();
        display.drawString(0,0,"WiFi Connect Failed!");
        display.display();
        delay(3000);
        ESP.restart();
        delay(1000);
      }
    }
    else
    {
      if (!wm.autoConnect(ssid, password))
      {
        Serial.println("failed to connect and hit timeout");
        display.clear();
        display.drawString(0,0,"WiFi Connect Failed!");
        display.display();
        delay(3000);
        ESP.restart();
        delay(5000);
      }
    }
//////////////////////////////////////////////////////////////////////////////////////////////////
    delay(100);
    display.drawString(0, 48, "Starting Ethernet...");
    display.display();
    SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI);
    ethernetReset(ETH_RST);
    Ethernet.init(ETH_CS);
    Ethernet.begin(mac, ip, gateway, subnet);
//////////////////////////////////////////////////////////////////////////////////////////////////    
    display.clear();
    display.drawString(0,0,"WIFI IP: "+WiFi.localIP().toString());
    display.drawString(0,12,"ETH IP: "+Ethernet.localIP().toString());
////////////////////////////////////////////////////////////////////////////////////////////////// 
    strncpy(macString, custom_text_box.getValue(), sizeof(macString));
    strncpy(websockString, custom_text_box2.getValue(), sizeof(websockString));
    Serial.print("macString: ");
    Serial.println(macString);
    Serial.print("websockString: ");
    Serial.println(websockString);
    display.drawString(0,36,"MAC: "+String(macString));
    display.display();
    if (shouldSaveConfig)
    {
      saveConfigFile();
    }
//////////////////////////////////////////////////////////////////////////////////////////////////
    websockclient.onMessage(onMessageCallback);
    websockclient.onEvent(onEventsCallback);
    while(!websockclient.connect(String(websockString))) {
      delay(500);
      Serial.print(">");
    }
    Serial.println("Websocket connected!");
    display.drawString(0,48,"Websocket Connected.");
    display.display();
    websockclient.send(String(macString));
//////////////////////////////////////////////////////////////////////////////////////////////////
    Udp.begin(9); //this can be any port but it needs to be called
//////////////////////////////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
////////////////////////////////LOOP//////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void loop()
{
    drd->loop();
    websockclient.poll();
}
