//MPU6050 and Boosting and QuecFastFix Done!
//EPO Not Sure working!!
//Buffer & TinyGPS LEFT!
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <TinyGPS.h>
#include <stdio.h>


#define GSM_PWRKEY_PIN 10
const char* host = "esp8266-webupdate";
const char* ssid = "gRexa_net";
const char* password = "09124075426";
const char* mqtt_server = "88.99.155.208";
int counter=0,count=0;
unsigned long _time = 0, lastTime = 0, EPOtimer = 0;


ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient espClient;
PubSubClient client(espClient);
TinyGPS gps;
bool wifi_connected = false;

void(* resetFunc) (void) = 0;

bool setup_wifi(){
  delay(10);

  // We start by connecting to a WiFi network
  // Serial.println();
  // Serial.print("Connecting to ");
  // Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  for(byte i=0; i<20; i++){
    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      // Serial.print(".");
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    // Serial.println("");
    // Serial.println("WiFi connected");
    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());
    //client.publish("Bemine/log",WiFi.localIP().toString().c_str());
    return true;
  }
  return false;
}

void sub_callback(char* topic, byte* payload, unsigned int length){

  if (topic[0] == 'h' && topic[1] == 'a' && topic[2] == 'm' && topic[3] == 'i' && topic[4] == 'd')
  {
    if ((char)payload[0] == 'x') //calibration state
    {
      client.publish("Bemine/log", "\" x RECEIVED.\"");
    }
  }
}

void reconnect() {
  if (!client.connected()) {
    //Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(),"shelfix","shelfix1")) {
      // Serial.println("Reconnected");
      if(wifi_connected) client.publish("Bemine/log", "Reconnected");
    } else {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
    }
  }
}

void GSM_command(String cmd){
  Serial.println(cmd);
  delay(100);
}

bool read_response(String report, int timeout, String& response){
  String str;
  int loop_counter = 0;
  while(!Serial.available() && (loop_counter/10.0)<timeout){
    delay(100);
    httpServer.handleClient();
    loop_counter++;
  }
  response = Serial.readString();
  if (loop_counter != 0.0) {
    str = "loop_counter:" + String(loop_counter/10.0);
    if(wifi_connected) client.publish("Bemine/log", &str[0u]);
  }

  if (loop_counter/10.0 == timeout) {
    if(wifi_connected) client.publish("Bemine/log", "timeout");
  //  return false;
  }
  str = report + "->" + response;
  if(wifi_connected) client.publish("Bemine/log", &str[0u]);


  return true;
}

bool validate_response(String response, String val){

  String str;

  if(response.indexOf(val) != -1){
    //client.publish("Bemine/log", "succeed");
    return true;
  }else{
    if(wifi_connected) client.publish("Bemine/log", "validate failed");
    return false;
  }
}

void GSM_powerOn(){
  //delay(500);
  digitalWrite(GSM_PWRKEY_PIN,HIGH);
  delay(2700);
  digitalWrite(GSM_PWRKEY_PIN,LOW);
  delay(2500);
}

void GSM_powerOff(){
  delay(1500);
  digitalWrite(GSM_PWRKEY_PIN,HIGH);
  delay(850);
  digitalWrite(GSM_PWRKEY_PIN,LOW);
  delay(2500);
}

void GSM_setup(){
  String str;
  if(wifi_connected) client.publish("Bemine/log","Setting up GSM ...");
  // GSM_command("AT+QPOWD=1");
  // read_response("AT+QPOWD=1", 1, str);
  // validate_response(str, "NORMAL POWER DOWN");
  GSM_powerOff();
  if(wifi_connected) client.publish("Bemine/log","GSM off ...");
  GSM_powerOn();
  if(wifi_connected) client.publish("Bemine/log","GSM on ...");
  // read_response("boot ", 10, str);
  if (Serial.available()){
    str = Serial.readString();
    if(wifi_connected) client.publish("Bemine/log",str.c_str());
  }
}

bool GSM_init(){
  String str;
  bool valid = true;

  GSM_command("AT");
  read_response("AT", 1, str);
  validate_response(str, "OK");

  // GSM_command("AT+QREFUSECS=1,1");    // turn off Echo mode
  // read_response("AT+QREFUSECS=1,1", 3, str);
  // validate_response(str, "OK");

  GSM_command("ATE0&W");    // turn off Echo mode
  read_response("ATE0&W", 3, str);
  validate_response(str, "OK");

  // if (Serial.available()){
  //   str = Serial.readString();
  //   if(wifi_connected) client.publish("Bemine/log",str.c_str());
  // }
   
  GSM_command("AT+COPS?");   // Query the currently selected operator
  read_response("AT+COPS?", 10, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+CFUN=1");  //Set in Full functionality
  read_response("AT+CFUN=1", 10, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+CREG?");    // Query register state of GSM network
  read_response("AT+CREG?", 1, str);    //<stat>=1 means GSM network is registered
  valid = validate_response(str, "OK");
  if(!valid) return false;  

  GSM_command("AT+CGREG?");    // Query register state of GPRS network
  read_response("AT+CGREG?",1, str);    // <stat>=1 means GPRS network is registered
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+CGATT=1");    // Attach to GPRS Service
  read_response("AT+CGATT=1", 1, str);    
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+CGDCONT=1,\"IP\",\"mtnirancell\"");    // Define PDP Context; PDP_type=IP, APN=mtnirancell
  read_response("AT+CGDCONT=1,\"IP\",\"mtnirancell\"", 1, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+CGACT=1");    // Activate GPRS context
  read_response("AT+CGACT=1", 10, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  // GSM_command("AT+QIFGCNT=2");    //Set PDP Context(Select PDP Context as Foreground)
  // read_response("AT+QIFGCNT=2", 10, str);
  // validate_response(str, "OK");

  GSM_command("AT+QIMUX?");    // Control Whether or Not to Enable Multiple TCPIP
  read_response("AT+QIMUX?", 1, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+QIMODE=0");    // Select TCPIP Transfer Mode ; Non-Transparent=0, Transparent=1
  read_response("AT+QIMODE=0", 1, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+QIDNSIP=1");    // Use "domain name" as the address to establish TCP session, AT+QIDNSIP=0 : IP
  read_response("AT+QIDNSIP=1", 1, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  // GSM_command("AT+IFGCNT=2");    // Select GPRS as the Bearer, apn=mtnirancell, user=<NULL>, pass=<NULL>
  // read_response("AT+IFGCNT=2", 10, str);
  // validate_response(str, "OK");

  GSM_command("AT+QICSGP=1,\"mtnirancell\",\"\",\"\"");    // Select GPRS as the Bearer, apn=mtnirancell, user=<NULL>, pass=<NULL>
  read_response("AT+QICSGP=1,\"mtnirancell\",\"\",\"\"", 1, str);
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+QIREGAPP=\"mtnirancell\",\"\",\"\"");    // Start TCPIP Task; apn=mtnirancell, user=<NULL>, pass=<NULL>
  read_response("AT+QIREGAPP=\"mtnirancell\",\"\",\"\"", 1, str);    // All of following commands should be executed together in sequence
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+QIACT");    // Activate GPRS Context
  read_response("AT+QIACT", 10, str);   // Reboot the module if there is no response for AT+QIACT in 180s
  valid = validate_response(str, "OK");
  if(!valid) return false; 

  GSM_command("AT+QNITZ=1");    //Enable Network Time synchronization by Nitz
  read_response("AT+QNITZ=1", 1, str);
  validate_response(str, "OK");

  GSM_command("AT+CTZU?");    //Check if NITZ is Enable
  read_response("AT+CTZU?", 1, str);
  validate_response(str, "OK");

  GSM_command("AT+QNTP=\"2.ir.pool.ntp.org\""); //  Default SERVER: 210.72.145.44 2.ir.pool.ntp.org
  delay(3000);
  read_response("AT+QNTP=\"2.ir.pool.ntp.org\"", 10, str);
  valid = validate_response(str, "QNTP: 0"); 
  if(!valid) return false; 

  GSM_command("AT+QGNSSTS?");    //Enable Network Time synchronization by Nitz
  read_response("AT+QGNSSTS?", 5, str);
  validate_response(str, "1");

  GSM_command("AT+QNTP"); //  Checking Time synchronization By QNTP
  delay(3000);
  read_response("AT+QNTP", 10, str);
  validate_response(str, "QNTP: 0"); 

  if(wifi_connected) client.publish("Bemine/log","valid=");
  if(wifi_connected) client.publish("Bemine/log",String(valid).c_str());
  return true;
}

void GNSS_setup(){
  String str;

  GSM_command("AT+QGNSSEPO=1");    // Get Current Location based on the density of network cells
  read_response("AT+QGNSSEPO=1", 10, str);
  validate_response(str, "OK");
  delay(2000);

  EPOtimer = millis();

  GSM_command("AT+QGNSSC=1");    // Control Power Supply of GNSS Module
  read_response("AT+QGNSSC=1", 10, str);
  validate_response(str, "OK");

  delay(2000);

  GSM_command("AT+QGREFLOC=35.78838,51.425595");    //
  read_response("AT+QGREFLOC=35.78838,51.425595", 10, str);
  validate_response(str, "OK");

}

void GNSS_init(){
  String str;
  String latitude,longitude,refloc;
  int lat,lon;

  GSM_command("AT+QGNSSRD=\"NMEA/GGA\"");    // Global Positioning System Fix Data
  read_response("AT+QGNSSRD=\"NMEA/GGA\"", 10, str);
  validate_response(str, "OK");

  GSM_command("AT+QGNSSRD=\"NMEA/VTG\"");    // Course Over Ground and Ground Speed
  read_response("AT+QGNSSRD=\"NMEA/VTG\"", 10, str);
  validate_response(str, "OK");

  GSM_command("AT+QCELLLOC=1");    // Get Current Location based on the density of network cells
  read_response("AT+QCELLLOC=1", 6, str);
  validate_response(str, "OK");

  if (str.indexOf("+QCELLLOC:")){
    lat=str.indexOf("LOC:");
    lat += 4;
    latitude=str.substring(lat+1,lat+10);
    lon=lat+11;
    longitude=str.substring(lon,lon+8);
    refloc="AT+QGREFLOC=" + longitude + "," + latitude;
    GSM_command(refloc);    //
    read_response(refloc, 5, str);
    validate_response(str, "OK");
    if(!validate_response(str, "OK")){
      GSM_command("AT+QGREFLOC=35.78838,51.425595");    //
      read_response("AT+QGREFLOC=35.78838,51.425595", 10, str);
      validate_response(str, "OK");
    }
  }
  
}

String GNSS_read(){
  String str, ret_str;

  // GSM_command("AT+CCLK?");    // Get Time Synchronization Status for GNSS Module
  // read_response("AT+CCLK?", 1, str);
  // validate_response(str, "OK");
  //ret_str = str + "\r\n";

  GSM_command("AT+QGNSSRD=\"NMEA/GGA\"");    // Global Positioning System Fix Data
   read_response("AT+QGNSSRD=\"NMEA/GGA\"", 5, str);
   validate_response(str, "OK");
   ret_str += str + "\r\n";

  if(str.length()>=70 && count==0){
    EPOtimer = millis() - EPOtimer ;
    str="EPOtimer :" + String(EPOtimer);
    if(wifi_connected) client.publish("Bemine/log",&str[0u]);
    count++;
  }
  // if(count>=1){
  //   ret_str += "\r\ntime with EPO:" + String(EPOtimer);
  // }

  GSM_command("AT+QGNSSRD=\"NMEA/VTG\"");    // Course Over Ground and Ground Speed
  read_response("AT+QGNSSRD=\"NMEA/VTG\"", 5, str);
  validate_response(str, "OK");
  //ret_str += str;
  
  return ret_str;
}

void TCP_send(String domain, String query){
  char buf[4];
  String str;

  GSM_command("AT+QIOPEN=\"TCP\",\""+domain+"\",\"80\"");
  read_response("AT+QIOPEN=\"TCP\",\""+domain+"\",\"80\"", 10, str);
  validate_response(str, "OK");
  read_response("continue", 20, str);
  validate_response(str, "CONNECT");

  GSM_command("AT+QISEND");
  read_response("AT+QISEND", 1, str);
  validate_response(str, ">");
  Serial.print(query);
  Serial.write(26);
  read_response("QUERY_SEND", 10, str);
  validate_response(str, "SEND OK");

  GSM_command("AT+QISACK");    // Check the information for the sending data
  read_response("AT+QISACK", 1, str);
  validate_response(str, "OK");

  GSM_command("AT+QICLOSE");    // Close TCP Connection
  read_response("AT+QICLOSE", 1, str);
  validate_response(str, "CLOSE OK");
}


void setup() {

  String str;
  Serial.begin(115200);

  randomSeed(micros());

  wifi_connected = false;
  if (setup_wifi()){
    // setup OTA
    MDNS.begin(host);
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    MDNS.addService("http", "tcp", 80);   //  Open http://[host].local/update in your browser
    httpServer.handleClient();

    // connect to mqtt server
    client.setServer(mqtt_server, 8883);
    client.setCallback(sub_callback);
    reconnect();
    client.publish("Bemine/log",WiFi.localIP().toString().c_str());
    client.publish("Bemine/log","Connected to wifi");
    wifi_connected = true;
  }

  pinMode(GSM_PWRKEY_PIN, OUTPUT);
  digitalWrite(GSM_PWRKEY_PIN,LOW);
  delay(500);

  do{
    GSM_setup();
  }while(!GSM_init());
 
  GNSS_setup();
  GNSS_init();

  lastTime = millis();
}

void loop() {
  counter++;
  String str, GNSS_str;
  _time = millis();

  if (WiFi.status() != WL_CONNECTED && _time-lastTime > 20000){
    //Serial.println(WiFi.status());
    if (setup_wifi()){
      // setup OTA
      MDNS.begin(host);
      httpUpdater.setup(&httpServer);
      httpServer.begin();
      MDNS.addService("http", "tcp", 80);   //  Open http://[host].local/update in your browser
      httpServer.handleClient();

      // connect to mqtt server
      client.setServer(mqtt_server, 8883);
      client.setCallback(sub_callback);
      reconnect();
      client.publish("Bemine/log",WiFi.localIP().toString().c_str());
      client.publish("Bemine/log","Connected to wifi");

      wifi_connected = true;
    }
    lastTime = millis();
    wifi_connected = false;
  }

if(wifi_connected) {
  httpServer.handleClient();
  reconnect();
}
  client.loop();

  GNSS_str = GNSS_read();
  GNSS_str += "\r\n";

  str = "POST /samanTel.php HTTP/1.1\r\n";
  str += "Host: www.fdli.ir\r\n";
  str += "Content-Type: application/json;\r\n";
  str += "Content-Length: " + String(GNSS_str.length()) + "\r\n";
  str += "\r\n";
  str += GNSS_str;

  TCP_send("www.fdli.ir", str);

  GSM_command("AT+QGEPOF=0,255");    // Check EPO Files Sizes
  read_response("AT+QGEPOF=0,255", 5, str);
  validate_response(str, "OK");

  GSM_command("AT+QGEPOF=2");    // Check last time EPO Files downloaded
  read_response("AT+QGEPOF=2", 5, str);
  validate_response(str, "OK");
  
}
//TinyGPS , QuecFastFix Done , MPU6050 Done , Buffer