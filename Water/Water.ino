#include <Arduino.h> 
#include <ESP8266WiFi.h> 
#include <ESP8266HTTPClient.h> 
#include <PubSubClient.h> 
#include <Wire.h> // Include Wire if you're using I2C

// you are expected to change these params
const char *ssid = "ChinaNet-HHH";
const char *password = "hzylsbhyrhch";
const char *mqtt_server = "tx.3cat.top";
const int sleepTime = 3000 //ms

// you are not expected to change anything in follow
HTTPClient httpClient;
char mqtt_topic[128];
char mqtt_payload[256];
u32 ChipID = ESP.getChipId();
WiFiClient espClient;
PubSubClient client(espClient);
char logbuf[256];
bool g_bwaterd = false;

void setup_wifi()
{
  delay(10);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.print("\t");
  Serial.print("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  Serial.print("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (client.connect(clientId.c_str()))
  {
    Serial.print("connected\t");
  }
  else
  {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again ");
  }
}

void mqttReport()
{
  if (!client.connected())
  {
    Serial.print("MQTT Lost!  ");
    while (!client.connected())
    {
      delay(500);
      reconnect();
    }
    client.loop();
  }
  snprintf(mqtt_payload, 256, "{%0X:{water:%d}}\0",ChipID,(int)g_bwaterd);
  Serial.println(mqtt_payload);
  client.publish(mqtt_topic, mqtt_payload);
}


void setup()
{

  Serial.begin(9600);
  Serial.println("\n\nStart...");

  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(BUILTIN_LED, INPUT);

  digitalWrite(BUILTIN_LED, LOW);
  delay(100);
  digitalWrite(BUILTIN_LED, HIGH);

  setup_wifi();

  snprintf(mqtt_topic, 128, "SCUMAKERS/WATER/%08X", ChipID);
  mqtt_topic[127] = '\0';
  Serial.print("MQTT Topic=");
  Serial.println(mqtt_topic);

  client.setServer(mqtt_server, 1883);
  while (!client.connected())
  {
    delay(500);
    reconnect();
  }
  Serial.print("MQTT OK!  ");
  client.loop();
}

// the loop function runs over and over again forever
void loop()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    setup_wifi();
  }

  g_bwaterd = digitalRead(BUILTIN_LED);
  mqttReport();
  //httpReport();
  delay(sleepTime);
}
