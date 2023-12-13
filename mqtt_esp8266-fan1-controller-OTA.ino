/*
 ESP8266 MQTT OTA Updateable Sunsynk5.5 DC Side Top Fan1 Controller
 Works with most 4pin fans that can measure rpms and have builtin pwm controller
*/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <Wire.h>
// Define message buffer and publish string
char message_buff[16];
char message_buffe[16];
String pubString;
String pubStringe;
int iTotalDelay;
const char* versionNumber = "Online-Ver1030";
// ------------------------------------------------------------
// Start Editing here
const char* ssid = "YOURWIFINAME";
const char* password = "YOURWIFIPASSWORD";
const char* mqtt_username = "YOURMQTTUSERNAME";
const char* mqtt_password = "YOURMQTTPASSWORD";
const char* mqtt_server = "YOURMQTTSERVERADDRESS";
const char* host = "esp8266-webupdateFan1";
const char* noderedtopic = "YOURFANSPEEDTOPICFROMMQTT"; // Subscribe to Fan Speed MQTT Topic sent to mqtt From Nodered
/* 
 *  FAN SPEED SHOULD IDEALLY HAVE 5 VALUES for 1% - 100%
 *  THESE VALUES CAN BE DIFFERENT DEPENDING ON YOUR FAN PWM REQUIREMENTS
 *  USING NODERED TO GET THE SUNSYNK TEMPS AND CONVERT TO PWM VALUES AS SUGGESTED BELOW
 *  1 FOR LOWEST/SLOWEST SPEED
 *  20 25% SPEED
 *  55 50% SPEED
 *  105 75% SPEED
 *  250 100% SPEED
*/
#define FAN_PIN 14 //pwm fan control signal
#define SIGNAL_PIN 13 // Read Fan RPMs
// Stop Editing here
// Making changes below this line could cause unexpected results
// -------------------------------------------------------------
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#define DELAY_TIME 10000 // time between measurements [ms]
#define MIN_FAN_SPEED_PERCENT 20 // minimum fan speed [%]
#define MIN_TEMP 20 // turn fan off below [deg C]
#define MAX_TEMP 250 // turn fan to full speed above [deg C]

int vTaskDelay;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

volatile  bool TopicArrived = false;
const     int mqttpayloadSize = 100;
char mqttpayload [mqttpayloadSize] = {'\0'};
String mqtttopic;

void callback(char* topic, byte* payload, unsigned int length)
{
  if ( !TopicArrived )
  {
    memset( mqttpayload, '\0', mqttpayloadSize ); // clear payload char buffer
    mqtttopic = ""; //clear topic string buffer
    mqtttopic = topic; //store new topic
    memcpy( mqttpayload, payload, length );
    TopicArrived = true;
  }
}

// Wifi Client
WiFiClient wifiClient;
//
// This function yields back to the watchdog to avoid random ESP8266 resets
//
void myDelay(int ms)  
{
  int i;
  for(i=1; i!=ms; i++) 
  {
    delay(1);
    if(i%100 == 0) 
   {
      ESP.wdtFeed(); 
      yield();
    }
  }
  iTotalDelay+=ms;
}

//
// Starts WIFI connection
//
void startWIFI() 
{
    // If we are not connected
    if (WiFi.status() != WL_CONNECTED) 
    {
      int iTries;
      iTries=0;
      Serial.println(" ");
      Serial.println("Starting WIFI connection");
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(); 
      WiFi.begin(ssid, password);
    
      // If not WiFi connected, retry every 2 seconds for 15 minutes
      while (WiFi.status() != WL_CONNECTED) 
      {
        iTries++;
        Serial.print(".");
        delay(2000);
        
        // If can't get to Wifi for 15 minutes, reboot ESP Default 450
        if (iTries > 50)
        {
           Serial.println("TOO MANY WIFI ATTEMPTS, REBOOTING!");
           ESP.reset();
        }
      }
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println(WiFi.localIP());    
      // Let network have a chance to start up
      myDelay(1500);
    }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect to mqtt
    if (client.connect("esp8266Fan1ClientOTA1", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("Fan1ControllerOTA", versionNumber);
      // ... and resubscribe
      client.subscribe(noderedtopic); //Get Fan Speed From NodeRed
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  mqtttopic.reserve(100);
//  setup_wifi();
startWIFI();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(FAN_PIN, OUTPUT);
  pinMode(SIGNAL_PIN, INPUT);  
// start OTA
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
// end OTA
}

int getFanSpeedRpm() {
  int highTime = pulseIn(SIGNAL_PIN, HIGH);
  int lowTime = pulseIn(SIGNAL_PIN, LOW);
  int period = highTime + lowTime;
  if (period == 0) {
    return 0;
  }
  float freq = 1000000.0 / (float)period;
  return (freq * 60.0) / 2.0; // two cycles per revolution
}

void setFanSpeedPercent(int p) {
  int value = p; // (p / 100.0) * 255;
  analogWrite(FAN_PIN, value);
  Serial.println(value);
  String pubStringe = String(value);
  pubStringe.toCharArray(message_buffe, pubStringe.length()+1);  
  client.publish("Fan1ControlerPWMPinValue", message_buffe);
}

void loop(void) {
    httpServer.handleClient();
    MDNS.update();
    if (!client.connected()) {
    reconnect();
  }
    client.loop();
    if ( TopicArrived )
  {
    //parse topic
    Serial.print("Fan 2 Speed: ");
    Serial.println( mqttpayload );
    TopicArrived = false;
    float mqttpayloadresult = atof(mqttpayload); // float f = atof(carray); 
    float temp = mqttpayloadresult;
    Serial.print("Temperature derived PWM value is ");
    Serial.print(temp);
    Serial.println(" - RPMs Signal");
    int fanSpeedPercent, actualFanSpeedRpm;
    fanSpeedPercent = temp;
    Serial.print("Setting fan1 speed to ");
    Serial.print(fanSpeedPercent);
    Serial.println(" %");
    setFanSpeedPercent(fanSpeedPercent);
    actualFanSpeedRpm = getFanSpeedRpm();
    Serial.print("Fan1 speed is ");
    Serial.print(actualFanSpeedRpm);
    String pubString = String(actualFanSpeedRpm);
    pubString.toCharArray(message_buff, pubString.length()+1);
    client.publish("FanControllerRPMs", message_buff );
    Serial.println(" RPM");
    Serial.println();
  }
}
