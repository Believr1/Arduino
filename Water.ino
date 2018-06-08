#include < Arduino.h > 
#include < ESP8266WiFi.h > 
#include < ESP8266HTTPClient.h > 
#include < PubSubClient.h > 
#include < Wire.h > // Include Wire if you're using I2C
HTTPClient httpClient;

//#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
//#include <Adafruit_SHT31.h>
//#include <Adafruit_BMP085.h>

//const char* ssid = "MopLink";
//const char* password = "ThisIsLeosWLN";
const char *ssid = "ck";
const char *password = "chuangke";
const char *mqtt_server = "tx.3cat.top";

char mqtt_topic[128];

String SERVER_ADDR = "http://lora.3cat.top/action/postdata"; //[图片]http://lora.3cat.top/action/postdata
u32 ChipID;

int ledPin = BUILTIN_LED;
int inPin = D3;
int val = 0; // variable to store the read value
char buf[256];
bool g_bwaterd = false;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
char logbuf[256];

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);

  int nRetry = 20;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (nRetry-- <= 0)
    {
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connect failed!");
    return;
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
  Serial.print((g_bwaterd) ? "\nPublish MQTT message: water! \t" : "\nPublish MQTT message: IDLE. \t");
  client.publish(mqtt_topic, (g_bwaterd) ? "1" : "0");
}

void httpReport()
{
  Serial.print("Connect to ");
  Serial.print(SERVER_ADDR);
  if (httpClient.begin(SERVER_ADDR))
  {
    Serial.print(" successful!\t");
    // GET
    httpClient.setTimeout(2000);
    snprintf(logbuf, 256, "%08X", ChipID);
    httpClient.addHeader("uuid", logbuf);
    snprintf(logbuf, 256, "{water:%d}", (g_bwaterd) ? 1 : 0);
    httpClient.addHeader("sensorInfo", logbuf);
    int httpCode = httpClient.GET();
    Serial.print(logbuf);
  }
  else
    Serial.print(" failed!");
  httpClient.end();
  Serial.print("\tClose http connection.");
}

void setup()
{

  Serial.begin(9600);
  Serial.println("\n\nStart...");

  pinMode(ledPin, OUTPUT);
  pinMode(inPin, INPUT);

  digitalWrite(ledPin, LOW);
  delay(100);
  digitalWrite(ledPin, HIGH);

  setup_wifi();

  ChipID = ESP.getChipId();
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

  g_bwaterd = digitalRead(inPin);
  mqttReport();
  httpReport();
  delay(300000);
}