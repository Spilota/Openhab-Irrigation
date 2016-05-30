
/* ESP8266 + MQTT Irrigation Controller
 * Controls 8 zone relay via external MQTT commands
 * Allows for 2 areas, each with 8 zones.  Defined in const as Frontyard and Backyard
 * Only one zone in either area is allowed to be on at once
 * Author: Ricky Hervey
 */

#include <MQTTClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const String localArea = "frontyard";
const String remoteArea = "backyard";
const char* ssid = "YourSSID";
const char* password = "YourPassword"; 
const char* server = "192.168.XXX.XXX"; // server or URL of MQTT broker
const char* espName = "Frontyard Irrigation Controller";
String clientName = "Frontyard-"; // just a name used to talk to MQTT broker
String macName;
char* sprinklerTopic = "myHome/sprinkler/frontyard/+/target";
char* statusTopic = "myHome/sprinkler/frontyard/status/#";
String publishTopic; 
unsigned long statusPublishPeriod = 60000; //ms, set to 1 minute for testing
unsigned long statusLastTransmit = 0;

WiFiClient wifiClient;
MQTTClient clientMQTT;

int relayPin[] ={16, 5, 4, 0, 2, 14, 12, 13};
unsigned long vlvOpenTime =0;
unsigned long vlvOpenReportTime =0;
unsigned long vlvOpenReportTimeInterval =5000;
unsigned long maxinterval = 1800000; //(ms) - 30 minutes max sprinkler on time
unsigned long resetPeriod = 86400000; // 1 day - this is the period after which we restart the CPU


void setup() {
  //Initialize relay control GPIO
    for (int i = 0; i<8; ++i) {
    pinMode(relayPin[i], OUTPUT);
    digitalWrite(relayPin[i], HIGH);
    }
  Serial.begin(115200);
  connectWiFi();

   // Generate client name based on MAC address and last 8 bits of microsecond counter
  uint8_t mac[6];
  WiFi.macAddress(mac);
  macName = macToStr(mac);
  clientName += macName;
  
  clientMQTT.begin(server,wifiClient);
  connectMQTT();

  turnOffLocalZones();
  publishTopic = "myHome/sprinkler/" + localArea + "/status/runtime";
  clientMQTT.publish(publishTopic, String(vlvOpenReportTime));

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"password");

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(espName);
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA update");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void loop() {  
  // Check WiFi connection.  Reconnect if required.
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  } 
  monitorValveOpenTime();
  clientMQTT.loop();

  // reset after a day to avoid memory leaks 
  if(millis()>resetPeriod){
    ESP.restart();
  }

  //Publish general status messages
  //Functions as watchdog.  Board will restart if cannot connect to broker
  long now = millis();
  if (now - statusLastTransmit > statusPublishPeriod) {
    statusLastTransmit = now;
    publishStatus();
  }
 
  ArduinoOTA.handle();
  //delay(1000);
}
void connectWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    monitorValveOpenTime ();
  }
}

void publishStatus(){
  clientMQTT.unsubscribe(statusTopic);
  
  //Publish IP Address
   publishTopic = "myHome/sprinkler/" + localArea + "/status/IPaddress";
   clientMQTT.publish(publishTopic, WiFi.localIP().toString());
   delay(10);

   //Publish MAC Address
   publishTopic = "myHome/sprinkler/" + localArea + "/status/MACaddress";
   clientMQTT.publish(publishTopic, macName);
   delay(10);

   //Publish signal strength
   publishTopic = "myHome/sprinkler/" + localArea + "/status/RSSI";
   clientMQTT.publish(publishTopic, String(WiFi.RSSI(),DEC));
   delay(10);

   //Publish MQTT Client Name
   publishTopic = "myHome/sprinkler/" + localArea + "/status/clientName";
   clientMQTT.publish(publishTopic, clientName);
   delay(10);

   //Publish MQTT Broker IP
   publishTopic = "myHome/sprinkler/" + localArea + "/status/brokerIP";
   clientMQTT.publish(publishTopic, server);
   delay(10);

   //Publish MQTT BSSID
   publishTopic = "myHome/sprinkler/" + localArea + "/status/BSSID";
   clientMQTT.publish(publishTopic, WiFi.BSSIDstr());
   delay(10);

   //Publish Status Publish Period
   publishTopic = "myHome/sprinkler/" + localArea + "/status/statusPeriod";
   clientMQTT.publish(publishTopic, String(statusPublishPeriod/60000,DEC));
   delay(10);

   //Publish Max On Time
   publishTopic = "myHome/sprinkler/" + localArea + "/status/maxOnTime";
   clientMQTT.publish(publishTopic, String(maxinterval/60000,DEC));
   delay(10);
   
   clientMQTT.subscribe(statusTopic);
}
void connectMQTT() {
  while(!clientMQTT.connected()){    
    if (clientMQTT.connect((char*) clientName.c_str())) {
      clientMQTT.subscribe(sprinklerTopic);
      clientMQTT.subscribe(statusTopic);
    } 
    else {
      delay(500);
      monitorValveOpenTime();
    }
  }
}
void monitorValveOpenTime (){
  long now = millis();
  
  // Monitor valve open time to ensure that sprinklers do not run more than the max interval time.
    
  if(now - vlvOpenReportTime > vlvOpenReportTimeInterval && vlvOpenReportTime != 0){
    long runtime = now - vlvOpenTime;
    publishTopic = "myHome/sprinkler/" + localArea + "/status/runtime";
    clientMQTT.publish(publishTopic, String(runtime));
    vlvOpenReportTime = millis();
  }
  if(now - vlvOpenTime > maxinterval && vlvOpenTime != 0){
    vlvOpenTime = 0;
    vlvOpenReportTime=0;
    for (int i = 1; i<9; ++i) {
      digitalWrite(relayPin[i-1], HIGH); 
      publishTopic = "myHome/sprinkler/" + localArea + "/" + i + "/status";
      clientMQTT.publish(publishTopic, "OFF");
      delay(10);
    }
    publishTopic = "myHome/sprinkler/" + localArea + "/status/runtime";
    clientMQTT.publish(publishTopic, String(vlvOpenReportTime));
  }
}
String macToStr(const uint8_t* mac){
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}
void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  int i;
  String messageString = "";
  
  // Parse topic 
  // Sprinkler topic formatted as myHome/sprinkler/{area / room}/zone#/parameter
//  Serial.print("incoming: ");
//  Serial.print(topic);
//  Serial.print(" - ");
//  Serial.print(payload);
//  Serial.println();
  
  //remove myHome/
  topic.remove(0,topic.indexOf('/')+1); // sprinkler/area/zone#/parameter
  
  // Get Sensor type - Confirm Sprinkler?
  String topicType = topic.substring(0,topic.indexOf('/'));
  topic.remove(0,topic.indexOf('/')+1); // area/zone#/parameter

   // Get area
  String area = topic.substring(0,topic.indexOf('/'));
  topic.remove(0,topic.indexOf('/')+1); // zone#/paramater

  // Get zone#
  String zoneNumber = topic.substring(0,topic.indexOf('/'));
  
  int zoneIndex = zoneNumber.toInt();
  topic.remove(0,topic.indexOf('/')+1); // paramater

  String parameter = topic;

  if (zoneNumber == "status") {
    setStatusVariables(parameter, payload);
  } 
  if (parameter == "target"){
    if (payload == "ON"){
      publishLocalZonesState();
      turnOffLocalZones();
      //Turn off remote zones
      for (i=1; i<9; i++){
        publishTopic = "myHome/sprinkler/" + remoteArea + "/" + i + "/target";
        clientMQTT.publish(publishTopic, "OFF");
        delay(100);      
      }
      //Turn on target zone
      digitalWrite(relayPin[zoneIndex-1], LOW);   
      vlvOpenTime = millis();
      vlvOpenReportTime = vlvOpenTime;
      publishTopic = "myHome/sprinkler/" + localArea + "/" + zoneIndex + "/status";
      clientMQTT.publish(publishTopic,"ON"); 
    }
    if (payload == "OFF"){
      if(digitalRead(relayPin[zoneIndex-1])==LOW){
        digitalWrite(relayPin[zoneIndex-1], HIGH);
        publishTopic = "myHome/sprinkler/" + localArea + "/" + zoneIndex + "/status";
        clientMQTT.publish(publishTopic, "OFF");
        vlvOpenReportTime=0;
        vlvOpenTime=0; 
        publishTopic = "myHome/sprinkler/" + localArea + "/status/runtime";
        clientMQTT.publish(publishTopic, String(vlvOpenReportTime));
        //delay(25);
      }
    }
  }
}

void setStatusVariables(String variable, String payload){
    if (variable == "statusPeriod"){
      statusPublishPeriod = payload.toInt()*60000L; //convert minutes to milliseconds
      publishStatus();
    }
    else if(variable == "maxOnTime"){
      maxinterval = payload.toInt()*60000L; //convert minutes to milliseconds
      publishStatus();        
    }
}

void publishLocalZonesState(){
  for (int i=1; i<9; i++) {
    publishTopic = "myHome/sprinkler/" + localArea + "/" + i + "/status";
    if (digitalRead(relayPin[i-1]) == LOW) {
        clientMQTT.publish(publishTopic, "ON");
      }
    else {
      clientMQTT.publish(publishTopic,"OFF");
    }
    delay(50);
  }
}

void turnOffLocalZones(){
  //Turn off all local zones
    for (int i=1; i<9; i++){
      publishTopic = "myHome/sprinkler/" + localArea + "/" + i + "/status";
      digitalWrite(relayPin[i-1],HIGH);
      clientMQTT.publish(publishTopic, "OFF");
      delay(50); 
    }
    vlvOpenReportTime=0;
    vlvOpenTime=0; 
    publishTopic = "myHome/sprinkler/" + localArea + "/status/runtime";
    clientMQTT.publish(publishTopic, String(vlvOpenReportTime));
}

