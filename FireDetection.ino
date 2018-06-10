#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Wire.h>  // Include Wire if you're using I2C
HTTPClient httpClient;

//#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
//#include <Adafruit_SHT31.h>
//#include <Adafruit_BMP085.h>

// const char* ssid = "MopLink";
// const char* password = "ThisIsLeosWLN";
const char* ssid = "ck";
const char* password = "chuangke";
const char* mqtt_server = "tx.3cat.top";

char mqtt_topic[128];

String SERVER_ADDR = "http://10.132.40.15/action/postdata";          // http://lora.3cat.top/action/postdata
u32 ChipID;

#define WARNING_QUEUE_LEN 20
#define FIRE_ALARM_THRESH 5
#define HEARTBEAT_IN_SEC 60

int ledPin = BUILTIN_LED; 
int firePin = D6; 
int inPin = D3;   // pushbutton connected to digital pin 7
int val = 0;     // variable to store the read value

volatile int nHighStartTime1 = 0;
volatile int nLowStartTime1 = 0;
volatile int nHighTime1 = 0;
volatile int nLowTime1 = 0;
char buf[256];

bool alarmQueue[WARNING_QUEUE_LEN];
uint8_t queue_index = 0;
int32_t g_nLastCycleTime = 0;
int32_t g_nHeartbeatCountDown = 0;
bool g_bFired = false;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
char logbuf[256];
int value = 0;

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
    if (nRetry -- <= 0) {
      //WiFi.disconnect();
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED)  {
    Serial.println("Connect failed!");
    return;
  }
  
  randomSeed(micros());

  Serial.print("\t");
  Serial.print("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.print("connected\t");
      // Once connected, publish an announcement...
      //client.publish("testTopic", "Hello world, I am ESP8266!");
      // ... and resubscribe
      //client.subscribe("DCS/ESP8266/CTRL");//inTopic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again ");
  }
}

void inline lowInterrupt();
void inline highInterrupt(){
  detachInterrupt(inPin);
  nHighStartTime1 = micros();
  nHighTime1 = nHighStartTime1 - nLowStartTime1;
//  Serial.print(nHighStartTime1);
//  Serial.println("  HIGH");
  attachInterrupt(inPin, lowInterrupt, FALLING);
}

void inline lowInterrupt(){
  detachInterrupt(inPin);
  nLowStartTime1 = micros();
  nLowTime1 = nLowStartTime1 - nHighStartTime1;
//  Serial.print(nLowStartTime1);
//  Serial.println("  LOW");
  attachInterrupt(inPin,  highInterrupt, RISING);
}

void mqttReport() {
  if (!client.connected())  {
    Serial.print("MQTT Lost!  ");
    while (!client.connected()) { delay(500); reconnect();}
    client.loop();
   }
   Serial.print((g_bFired)?"\nPublish MQTT message: FIRE! \t":"\nPublish MQTT message: IDLE. \t");
   client.publish(mqtt_topic, (g_bFired)?"1":"0");
}

void httpReport()
{
  Serial.print("Connect to ");
  Serial.print(SERVER_ADDR);
  httpClient.begin(SERVER_ADDR);
  httpClient.setTimeout(2000);
  snprintf(logbuf, 256, "%08X", ChipID);
  httpClient.addHeader("uuid", logbuf);
  snprintf(logbuf, 256, "{fire:%d}", (g_bFired) ? 1 : 0);
  httpClient.addHeader("sensorInfo", logbuf);
  Serial.print(logbuf);
  int httpCode = httpClient.GET();
  if (httpCode > 0)
  {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
      String payload = httpClient.getString();
      Serial.print("\t");
      Serial.print(payload);
    }
  }
  else
    Serial.print(" failed!");
  httpClient.end();
  Serial.print("\tClose http connection.");
}

void action() {
  if (nHighTime1<3000000 && nLowTime1<3000000)  { // prevent micros overflow
    float fRatio = 0.0;
    if (nLowTime1)
      fRatio = 1.0*nHighTime1/nLowTime1;
    else
      nHighTime1 = 0; // signal off

    if (nHighTime1 && nLowTime1)  {
      //snprintf(buf, 255, "\n%d-%d    ratio=%d.%02d", nHighTime1, nLowTime1, (int)fRatio, (int)(fRatio*100 - (int)fRatio*100));  Serial.print(buf);
      digitalWrite(ledPin, LOW);
      alarmQueue[queue_index] = true;
    }
    else  {
      digitalWrite(ledPin, HIGH);
      alarmQueue[queue_index] = false;
    }
  }

  queue_index++;
  if (queue_index >= WARNING_QUEUE_LEN)
    queue_index = 0;

  int nTotalAlarmCount = 0;
  for (int i=0; i<WARNING_QUEUE_LEN; i++)
    if (alarmQueue[i] == true)
      nTotalAlarmCount++;

  if (nTotalAlarmCount > FIRE_ALARM_THRESH) {
    //Serial.print(" ALARM!");
    digitalWrite(firePin, HIGH);

    // comment line below to avoid mqtt package lost, continue send packages. LATER change mqtt client lib, set qos=2
    if (!g_bFired)
    {
      g_nHeartbeatCountDown = HEARTBEAT_IN_SEC * 1000000;
      g_bFired = true;
      mqttReport();
      httpReport();
    }
  }
  else {
    digitalWrite(firePin, LOW);

    if (g_bFired) // only send once when fire gone, if this pack lost, heartbeat will refresh the status
    {
      g_nHeartbeatCountDown = HEARTBEAT_IN_SEC * 1000000;
      g_bFired = false;
      mqttReport();
      httpReport();
    }
  }
}

void setup() {
  for (int i=0; i<WARNING_QUEUE_LEN; i++)
    alarmQueue[i] == false;
      
  Serial.begin(9600);
  Serial.println("\n\nStart...");
    
  pinMode(ledPin, OUTPUT);   
  pinMode(firePin, OUTPUT);  
  pinMode(inPin, INPUT);     

  digitalWrite(ledPin, HIGH);
  digitalWrite(firePin, LOW);

  setup_wifi();

  ChipID = ESP.getChipId();
  snprintf(mqtt_topic, 128, "SCUMAKERS/FIRE/%08X", ChipID);  
  mqtt_topic[127] = '\0';
  Serial.print("MQTT Topic=");
  Serial.println(mqtt_topic);
  
  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
      delay(500);
      reconnect();
  }
  Serial.print("MQTT OK!  ");
  client.loop();
 
  attachInterrupt(inPin, highInterrupt, RISING);
//  attachInterrupt(inPin, lowInterrupt, FALLING);
}

// the loop function runs over and over again forever
void loop() {
  detachInterrupt(inPin);

  if (WiFi.status() != WL_CONNECTED)  {
    while (WiFi.status() != WL_CONNECTED)  {
      setup_wifi();
      delay(500);
    }
  }

  action();
  nHighTime1 = nLowTime1 = 0;

  // check delta time, send heartbeat
  uint32_t nTimeNow = micros();
  int32_t nDelta = 0;
  if (nTimeNow > g_nLastCycleTime)  // while exceed uint32_t, about 50 days
    nDelta = nTimeNow - g_nLastCycleTime;
  g_nLastCycleTime = nTimeNow;
  
  g_nHeartbeatCountDown -= nDelta;
  if (g_nHeartbeatCountDown <= 0) {
    g_nHeartbeatCountDown = HEARTBEAT_IN_SEC * 1000000;
    mqttReport();
    httpReport();
  }


  //Serial.print("nTimeNow=");                  Serial.print(nTimeNow);  
  //Serial.print("\tnDelta=");                  Serial.print(nDelta);
  //Serial.print("\tg_nHeartbeatCountDown=");   Serial.println(g_nHeartbeatCountDown);
  
  
  attachInterrupt(inPin,  highInterrupt, RISING);
  delay(500);
}
