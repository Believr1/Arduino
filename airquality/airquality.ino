#include < Arduino.h > 
#include "crc.h"

#include < ESP8266WiFi.h > 
#include < ESP8266HTTPClient.h > 
//WiFiClient httpClient;
HTTPClient httpClient; 

#include < SoftwareSerial.h > 
SoftwareSerial swSer(0, 2, false, 256); 

String SERVER_ADDR = "[图片]http://lora.3cat.top/action/postdata"; // [图片]http://lora.3cat.top/action/postdata
//const char* ssid = "MopLink";
//const char* password = "ThisIsLeosWLN";
//const char* ssid = "B325";
//const char* password = "tiansithebest";
const char * ssid = "ck"; 
const char * password = "chuangke"; 
#define PACKLEN 19
unsigned char g_bufPacket[PACKLEN]; 
char logbuf[256]; 
u32 ChipID; 

typedef struct _AIR_PARAMS {
int nCO2; 
float fTVOC; 
float fCH2O; 
int nPM25; 
int nPM10; 
float fHUMIDITY; 
float fTEMPERATURE; 
}AIR_PARAMS; 

void setup_wifi() {
delay(10); 
// We start by connecting to a WiFi network
Serial.print("Connecting to "); 
Serial.print(ssid); 

WiFi.begin(ssid, password); 

int nRetry = 20; 
while (WiFi.status() != WL_CONNECTED) {
delay(500); 
Serial.print("."); 
if (nRetry-- <= 0) {
//WiFi.disconnect();
break; 
}
}

if (WiFi.status() != WL_CONNECTED) {
Serial.println("Connect failed!"); 
return; 
}

randomSeed(micros()); 

Serial.print("\t"); 
Serial.print("WiFi connected! "); 
Serial.print("IP address: "); 
Serial.println(WiFi.localIP()); 
}

void printFrame(unsigned char * buf, int nLen) {
Serial.print("Data received: "); 
for (int n = 0; n < nLen; n++) {
snprintf(logbuf, 256, "%02X ", buf[n]); 
Serial.print(logbuf); 
}
Serial.println(""); 
}

void parseFrame(unsigned char * buf, int nLen, AIR_PARAMS & ap) {
ap.nCO2 = buf[3] * 256 + buf[4]; 
ap.fTVOC = (buf[5] * 256.0 + buf[6])/10; 
ap.fCH2O = (buf[7] * 256.0 + buf[8])/10; 
ap.nPM25 = buf[9] * 256 + buf[10]; 
ap.nPM10 = buf[15] * 256 + buf[16]; 
ap.fHUMIDITY = -6 + 125 * (buf[11] * 256.0 + buf[12])/65536; 
ap.fTEMPERATURE = -46.85 + 175.72 * (buf[13] * 256.0 + buf[14])/65536; 
}

bool getCompleteFrame(unsigned char chRcv, unsigned char * buf) {
for (int i = 0; i < PACKLEN - 1; i++)
buf[i] = buf[i + 1]; 
buf[PACKLEN - 1] = chRcv; 
if (buf[0] == 0x01 && buf[1] == 0x03 && buf[2] == 0x0e) {
if (CRC_Check(buf, PACKLEN)) {
return true; 
}
}
return false; 
}

void setup() {
Serial.begin(9600); 
swSer.begin(9600); 
setup_wifi(); 

ChipID = ESP.getChipId(); 
snprintf(logbuf, 256, "ChipID=%08X", ChipID); 
Serial.println(logbuf); 
}

void loop() {
if (WiFi.status() != WL_CONNECTED) {
while (WiFi.status() != WL_CONNECTED) {
setup_wifi(); 
delay(500); 
}
}
/* 
  while (swSer.available() > 0) {
    Serial.write(swSer.read());
  }
  while (Serial.available() > 0) {
    swSer.write(Serial.read());
  }
*/

while (swSer.available() > 0) {
if (getCompleteFrame(swSer.read(), g_bufPacket)) {
AIR_PARAMS ap; 
//printFrame(g_bufPacket, PACKLEN);
parseFrame(g_bufPacket, PACKLEN, ap); 

Serial.print("Connect to "); 
Serial.print(SERVER_ADDR); 
if (httpClient.begin(SERVER_ADDR)) {
Serial.print(" successful!"); 
char strTVOC[8], strCH2O[8], strHumid[8], strTemp[8]; 
dtostrf(ap.fTVOC, 4, 2, strTVOC); 
dtostrf(ap.fCH2O, 4, 2, strCH2O); 
dtostrf(ap.fHUMIDITY, 4, 2, strHumid); 
dtostrf(ap.fTEMPERATURE, 4, 2, strTemp); 
snprintf(logbuf, 256, "\tChipID[%08X]  CO2:%dppm TVOC:%sug/m3 CH2O:%sug/m3 PM2.5:%dug/m3 PM10:%dug/m3 Humidity:%s%% Temperature:%sC", ChipID, ap.nCO2, strTVOC, strCH2O, ap.nPM25, ap.nPM10, strHumid, strTemp); 
Serial.print(logbuf); 

// GET
httpClient.setTimeout(2000); 
snprintf(logbuf, 256, "%08X", ChipID); 
httpClient.addHeader("uuid", logbuf); 
snprintf(logbuf, 256, "{co2:%d,tvoc:%s,ch2o:%s,pm25:%d,pm10:%d,humid:%s,temp:%s}", ap.nCO2, strTVOC, strCH2O, ap.nPM25, ap.nPM10, strHumid, strTemp); 
httpClient.addHeader("sensorInfo", logbuf); 
int httpCode = httpClient.GET(); 

/*{
          // POST
snprintf(logbuf, 256, "{uuid:\" % 08X\",sensorInfo:{co2:%d,tvoc:%s,ch2o:%s,pm25:%d,pm10:%d,humid:%s,temp:%s}}", ChipID, ap.nCO2, strTVOC, strCH2O, ap.nPM25, ap.nPM10, strHumid, strTemp); 
httpClient.POST(logbuf); 
Serial.print("POST:"); 
Serial.println(logbuf); 
} */
}
else
        Serial.println(" failed!"); 
httpClient.end(); 
Serial.println("\t\tClose http connection."); 

delay(5000); 
}
}
}
