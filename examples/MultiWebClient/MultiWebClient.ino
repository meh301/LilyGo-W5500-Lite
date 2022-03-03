#define ESP_DRD_USE_SPIFFS true //this needs to be defined before all other code to ensure that it works properly - double reset detector
////////
#include <Arduino.h>
#include <Ethernet.h> //using modified library from TTGO T-LITE https://github.com/Xinyuan-LilyGO/LilyGo-W5500-Lite
#include <SPI.h>
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"` //using included library from TTGO T-LITE https://github.com/Xinyuan-LilyGO/LilyGo-W5500-Lite
#include <WiFi.h>
#include <WiFiManager.h> //WiFiManager library by Tablatronix https://github.com/tzapu/WiFiManager
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
#define JSON_CONFIG_FILE "/webconfig.json"
#define DRD_TIMEOUT 10 //the timeout for the double reset in seconds
#define DRD_ADDRESS 0 //default bit to check for double reset

DoubleResetDetector* drd;

EthernetClient ethclient;
WiFiClient wificlient;

char ethserver[] = "www.bing.com";
char wifiserver[] = "jp.pool.ntp.org";

uint8_t mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; //Default ethernet MAC address - change depending on needs
//uint8_t ipp[] = {192,168,99,5}; //static ethernet configuration
//uint8_t gateway[] = {192,168,99,1}; //static ethernet configuration
//uint8_t subnet[] = {255,255,255,0}; //static ethernet configuration
bool shouldSaveConfig = false;
char staticString[50] = ""; //Static ethernet IP input for webconfigurator
char gatewayString[50] = ""; //Ethernet gateway input for webconfigurator
char subnetString[50] = "255.255.255.0"; //Ethernet subnet input for webconfigurator
bool ethernetbool = true;
bool wifibool = true;
bool dhcpbool = false;
uint64_t chipid = 0; //gets the esp32 mac address
uint16_t chip = 0; //makes the esp32 mac address shorter

WiFiManager wm; //WiFiManager setup based on https://github.com/witnessmenow/ESP32-WiFi-Manager-Examples Use Case 2
SSD1306Wire  display(0x3c, OLED_SDA, OLED_SCL);

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
/////////////////////////EXTRA FUNCTIONS//////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) { //https://stackoverflow.com/a/35236734
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);  // Convert byte
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
}
//////////////////////////////////////////////////////////////////////
void updatevalues(const char * text_box1, const char * text_box2, const char * text_box3, const char * check1, const char * check2, const char * check3) {
    strncpy(staticString, text_box1, sizeof(staticString));
    strncpy(gatewayString, text_box2, sizeof(gatewayString));
    strncpy(subnetString, text_box3, sizeof(subnetString));
    ethernetbool = (strncmp(check1, "T", 1) == 0);
    wifibool = (strncmp(check2, "T", 1) == 0);
    dhcpbool = (strncmp(check3, "T", 1) == 0);
    saveConfigFile();
}
//////////////////////////////////////////////////////////////////////
char *checkboxcheck(bool value) {
  char *customHtml;
  if (value) {
    customHtml = "type=\"checkbox\" checked";
  } else {
    customHtml = "type=\"checkbox\"";
  }
  return customHtml;
}
//////////////////////////////////////////////////////////////////////
void saveConfigFile()
{
  Serial.println(F("Saving config"));
  StaticJsonDocument<512> json;
  json["staticString"] = staticString;
  json["gatewayString"] = gatewayString;
  json["subnetString"] = subnetString;
  json["ethernetbool"] = ethernetbool;
  json["wifibool"] = wifibool;
  json["dhcpbool"] = dhcpbool;
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
          strcpy(staticString, json["staticString"]);
          strcpy(gatewayString, json["gatewayString"]);
          strcpy(subnetString, json["subnetString"]);
          ethernetbool = json["ethernetbool"].as<bool>();
          wifibool = json["wifibool"].as<bool>();
          dhcpbool = json["dhcpbool"].as<bool>();
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
  display.drawString(0, 0, "Configuring Gateway...");
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
void setup() {
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
    Serial.println("T-LITE Start");
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "T-LITE Start");
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
    wm.setBreakAfterConfig(true);
    wm.setAPCallback(configModeCallback);
    
    WiFiManagerParameter text_box1("static_address", "Enter Ethernet Static IP", staticString, 50);
    WiFiManagerParameter text_box2("gateway_address", "Enter Ethernet Gateway", gatewayString, 50);
    WiFiManagerParameter text_box3("subnet_address", "Enter Ethernet Subnet", subnetString, 50);
    
    WiFiManagerParameter check1("ethernetcheck", "Enable Ethernet", "T", 2, checkboxcheck(ethernetbool));
    WiFiManagerParameter check2("wificheck", "Enable WiFI", "T", 2, checkboxcheck(wifibool));
    WiFiManagerParameter check3("DHCPcheck", "Enable Ethernet DHCP", "T", 2, checkboxcheck(dhcpbool));

    wm.addParameter(&check2);
    wm.addParameter(&check1);
    wm.addParameter(&check3);
    wm.addParameter(&text_box1);
    wm.addParameter(&text_box2);
    wm.addParameter(&text_box3);
    
    char ssid[23];
    snprintf(ssid, 23, "Wlan_%04X", chip, (uint32_t)chipid); //WiFi configurator SSID //bookmark
    const char* password = "Wlan_Config"; // WiFi configurator password
//////////////////////////////////////////////////////////////////////////////////////////////////
    if (forceConfig)
    {
      if (!wm.startConfigPortal(ssid, password))
      {
        updatevalues(text_box1.getValue(), text_box2.getValue(), text_box3.getValue(), check1.getValue(), check2.getValue(), check3.getValue());
        Serial.println("failed to connect and hit timeout");
        display.clear();
        display.drawString(0,0,"WiFi Connect Failed!");
        display.display();
        delay(3000);
        ESP.restart();
        delay(1000);
      }
      if (shouldSaveConfig)
      {
        updatevalues(text_box1.getValue(), text_box2.getValue(), text_box3.getValue(), check1.getValue(), check2.getValue(), check3.getValue());
      }
    }
    else
    {
      if (wifibool) {
        Serial.println("Enabling WiFi");
        display.drawString(0, 24, "WiFI Enabled");
        display.display();
      if ((!wm.autoConnect(ssid, password)))
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
    }
//////////////////////////////////////////////////////////////////////////////////////////////////
    
 if (ethernetbool) {
    display.drawString(0, 48, "Starting Ethernet...");
    display.display();
    SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI);
    ethernetReset(ETH_RST);
    Ethernet.init(ETH_CS);
    if (dhcpbool) {
      Serial.println("Starting ethernet in DHCP mode");
      if (Ethernet.begin(mac) == 0) {
        display.clear();
        display.drawString(0, 24, "Failed to configure Eth using DHCP");
        Serial.println("Failed to configure Eth using DHCP");
        display.display();
        delay(10000);
        ESP.restart();
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            display.clear();
            display.drawString(0, 34, "Eth shield was not found.");
            Serial.println("Eth shield was not found.");
            display.display();
            delay(10000);
            ESP.restart();
        } else if (Ethernet.linkStatus() == LinkOFF) {
            display.clear();
            display.drawString(0, 34, "Eth cable is not connected.");
            Serial.println("Eth cable is not connected.");
            display.display();
        }
      } 
      Serial.println("ethernet started");
    }
    else {
      Serial.println("Starting ethernet in Static mode");
      byte ip[4];
      byte gateway[4];
      byte subnet[4];
      parseBytes(staticString, '.', ip, 4, 10);
      parseBytes(gatewayString, '.', gateway, 4, 10);
      parseBytes(subnetString, '.', subnet, 4, 10);
      Ethernet.begin(mac, ip, gateway, subnet);
      Serial.println("ethernet started");
     }
    }
    display.clear();
    if (wifibool) {
      display.drawString(0,0,"WIFI IP: "+WiFi.localIP().toString());
      if (wificlient.connect(wifiserver, 80)) {
        Serial.print("connected to ");
        Serial.println(wificlient.remoteIP());
        display.drawString(0,12,"Remote: "+wificlient.remoteIP().toString());
        // Make a HTTP request:
        wificlient.println("GET / HTTP/1.1");
        wificlient.println("Host: "+String(wifiserver));
        wificlient.println("Connection: close");
        wificlient.println();
        
    } else {
        // if you didn't get a connection to the server:
        Serial.println("WiFi connection failed");
        display.drawString(0,12,"WiFi Connection failed");
    }
    }
    if (ethernetbool) {
      display.drawString(0,36,"ETH IP: "+Ethernet.localIP().toString());
      if (ethclient.connect(ethserver, 80)) {
        Serial.print("connected to ");
        Serial.println(ethclient.remoteIP());
        display.drawString(0,48,"Remote: "+ethclient.remoteIP().toString());
        // Make a HTTP request:
        ethclient.println("GET / HTTP/1.1");
        ethclient.println("Host: "+String(ethserver));
        ethclient.println("Connection: close");
        ethclient.println();
        
    } else {
        // if you didn't get a connection to the server:
        Serial.println("Ethernet connection failed");
        display.drawString(0,48,"ETH Connection failed");
    }
    }
    display.display();
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
////////////////////////////////LOOP//////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void loop() {
//////////////////////////////////////////////////////////////////////
  drd->loop();
//////////////////////////////////////////////////////////////////////
}
