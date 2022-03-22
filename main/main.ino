/*
 * UVR Mobile DAQ System
 * Turn on device, scan QR code to connect to access point
 * Navigate to http://192.168.4.1/ and load the user panel
 * 
 * Designed and built by @MorganStuyt
 * 12/03/2022
 */

// ==========================================
// ==             Configuration            ==
// ==========================================
#define SERIAL_DEBUG

// ===============================
// ==     Include Libraries     ==
// =============================== 
#include "WiFi.h"               // access point for esp32
#include "ESPAsyncWebServer.h"  // webserver
#include "SPIFFS.h"             // to store html and css files on chip
#include <HX711_ADC.h>          // HX711 board load cell
#include <EEPROM.h>             // EEPROM for load cell calibration data
#include "SD.h"                 // SD card
#include <SPI.h>                // SD card
#include "FS.h"                 // SD card
#include "Adafruit_MAX31855.h"  // MAX31885 thermo
#include <string>

// ===========================
// == Misc Gloabl Vars      ==
// ===========================
unsigned long lastUiTime = 0;
unsigned long lastLogTime = 0;
unsigned long lastGetTempTime = 0;
unsigned long uiDelay = 500;  // delay for ui display [ms]
unsigned long logDelay = 100;  // delay for logging to sd card [ms]
#define BLED 5 // BLUE ESP32 LED

// =========================
// ==     HX711 Vars      ==
// =========================
const int HX711_dout = 13; // HX711 circuit wiring
const int HX711_sck = 12;  // HX711 circuit wiring
float force; // global var to update with force from load cell
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;
unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
String hx711State;
float CalibrationWeight;

// ===========================
// ==     SD Card Vars      ==
// ===========================
String path = "/DAQ.csv";
File file;
int sd_count = 0;
String sdState;
String dataMessage;
boolean sd;

// ===============================
// ==     Access Point Vars     ==
// ===============================
const char* ssid = "UVR Mobile DAQ";
const char* password = "rocketsrock";
AsyncWebServer server(80); // Create AsyncWebServer object on port 80
AsyncEventSource events("/events");

// ============================
// ==     MAX31885 Vars      ==
// ============================
#define MAXDO   14
#define MAXCS   26
#define MAXCLK  27
unsigned long temperatureDelay = 1000;
double temperature;
String max31885State;
// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

// ============================
// ==     Pressure Vars      ==
// ============================
float pressure;
String pressureState;

// ==========================
// ==     Server Vars      ==
// ==========================
String daqState;
boolean startLog = false;

// Replaces placeholder with state value
String processor(const String& var){
  //Serial.println(var);
  if(var == "DAQSTATE"){
    return String(daqState);
  }
  else if(var == "HX711STATE"){ // %HX711STATE% from html span thing
    return String (hx711State);
  }
  else if(var == "SDSTATE"){
    return String(sdState);
  }
  else if(var == "LOAD"){
    return String(force);
  }
  else if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "PRESSURE"){
    return String(pressure);
  }
}

// ===========================================
// ==             Setup Function            ==
// =========================================== 
void setup(){

  #ifdef SERIAL_DEBUG
    Serial.begin(115200); // Serial port for debugging purposes
    while (!Serial)
      delay(100);
  #endif
  
  // Initailize LEDs and set LOW
  pinMode(BLED, OUTPUT);
  digitalWrite(BLED, LOW);

  initializeSPIFFS();

  initializeAP();

  initializeServer();

  initializeSD();

  initializeHx711();

  initializeMAX31855();

  daqState = "Ready";
  events.send("ping",NULL,millis());
  events.send(String(daqState).c_str(),"daqstate",millis());
  digitalWrite(BLED, HIGH);
  delay(30000);
  digitalWrite(BLED, LOW);
}

// ==========================================
// ==             Main Function            ==
// ==========================================
void loop(){
  if ((millis() - lastGetTempTime) > temperatureDelay) {
    get_temperature();
    lastGetTempTime = millis();
  }
  
  get_loadCell();
  get_pressure();
  
  if (((millis() - lastLogTime) > logDelay) && startLog){
    logSDCard();
    lastLogTime = millis();
  }
  
  if ((millis() - lastUiTime) > uiDelay){

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(daqState).c_str(),"daqstate",millis());
    events.send(String(sdState).c_str(),"sdstate",millis());
    events.send(String(force).c_str(),"loadcell",millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(pressure).c_str(),"pressure",millis());
    events.send(String(hx711State).c_str(),"hx711State",millis());
    lastUiTime = millis();
  }
}

/*
 * SPIFFS initializtion used to store html and css files on esp drive
 */
void initializeSPIFFS(){
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}
/*
 * Function to initialize the wifi access point on esp32. Currently no password is required to access
 * the server, but can be added in after ssid. Server IP address is 192.146.4.1
 */
void initializeAP(){
  // Connect to Wi-Fi
  WiFi.softAP(ssid); // password removed

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address: ");
  Serial.println(IP);
}

/*
 * Start the server on the esp32 access point. load in the html page, css files, and igon logo.
 * Action to exicute when log button is pressed.Events handler.
 */
void initializeServer(){
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/light.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/light.css", "text/css");
  });
  // Route to load style.css file
  server.on("/dark.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/dark.css", "text/css");
  });

  // Route to load logo
  server.on("/uvrlogo.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/uvrlogo.png", "image/png");
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });

  server.on("/logDAQ", HTTP_GET, [](AsyncWebServerRequest *request){ // id logDAQ from html button id
    startLogging();
    request->send(200, "text/plain", "OK");
  });
  server.on("/tareLoad", HTTP_GET, [](AsyncWebServerRequest *request){
    loadCellTare();
    request->send(200, "text/plain", "OK");
  });
  server.on("/sdInitialize", HTTP_GET, [](AsyncWebServerRequest *request){
    initializeSD();
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/reverseLoad", HTTP_GET, [](AsyncWebServerRequest *request){
    reverseLoadCell();
    request->send(200, "text/plain", "OK");
  });
  server.on("/calib", HTTP_GET, [](AsyncWebServerRequest *request){
    reverseLoadCell();
    request->send(200, "text/plain", "OK");
  });
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String loadCellCalibrationWeight;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(loadCellCalibrationWeight)) {
      inputMessage = request->getParam(loadCellCalibrationWeight)->value();
      CalibrationWeight = loadCellCalibrationWeight.toFloat();
      calibrate();
    }
  });
  server.addHandler(&events);

  server.begin(); // Start server
}
/*
 * Function to set daq states and turn on LED when logging starts and turn off LED when logging stops.
 * TODO: add section to only log if SD card mount and setup success
 */
void startLogging(){
  if (!startLog){
    startLog = true;
    daqState = "Logging";
    sdState = "Logging";
    digitalWrite(BLED, HIGH);
  } else {
    startLog = false;
    digitalWrite(BLED, LOW);
    daqState = "Ready";
    sdState = "Ready";
  }
}

void initializeSD() {
  if (!SD.begin(2)) {
    sdState = "SD MOUNT FAIL";
    #ifdef SERIAL_DEBUG
      Serial.println("Card Mount Failed");
    #endif
    return;
  }

  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    sdState = "NO SD";
    #ifdef SERIAL_DEBUG
      Serial.println("No SD card attached");
    #endif
    return;
  }

  file = SD.open("/data.csv");
  if(!file){
    #ifdef SERIAL_DEBUG
      Serial.println("File doens't exist");
      Serial.println("Creating file...");
    #endif
    writeFile(SD, "/data.csv", "Time [ms], Force [N], Temperature [C], Pressure [PSI]\r\n");
  } else {
    #ifdef SERIAL_DEBUG
      Serial.println("File already exists");
    #endif  
  }
  file.close();
  sdState = "Ready";
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  #ifdef SERIAL_DEBUG
    Serial.printf("Writing file: %s\n", path);
  #endif

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    #ifdef SERIAL_DEBUG
      Serial.println("Failed to open file for writing");
    #endif
    return;
  }
  if(file.print(message)) {
    #ifdef SERIAL_DEBUG
      Serial.println("File written");
    #endif
  } else {
    #ifdef SERIAL_DEBUG
      Serial.println("Write failed");
    #endif
  }
  file.close();
}

// Write the sensor readings on the SD card
void logSDCard() {
  dataMessage = String(millis()) + "," + String(force) + "," + String(temperature) + "," + 
                String(pressure) + "\r\n";
  #ifdef SERIAL_DEBUG
    Serial.print("Save data: ");
    Serial.println(dataMessage);
  #endif
  appendFile(SD, "/data.csv", dataMessage.c_str());
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  #ifdef SERIAL_DEBUG
    Serial.printf("Appending to file: %s\n", path);
  #endif

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    #ifdef SERIAL_DEBUG
      Serial.println("Failed to open file for appending");
    #endif
    return;
  }
  if(file.print(message)) {
    #ifdef SERIAL_DEBUG
      Serial.println("Message appended");
    #endif
  } else {
    #ifdef SERIAL_DEBUG
      Serial.println("Append failed");
    #endif
  }
  file.close();
}

void reverseLoadCell(){
  LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
}

void initializeHx711(){
  LoadCell.begin();

  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    #ifdef SERIAL_DEBUG
      Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    #endif
    while (1);
  }else {
    LoadCell.setCalFactor(1.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    #ifdef SERIAL_DEBUG
      Serial.println("Startup is complete");
    #endif
  }
  while (!LoadCell.update());
}
void calibrate() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell an a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    LoadCell.tareNoDelay();
    if (LoadCell.getTareStatus() == true) {
      hx711State = "Tare OK";
      #ifdef SERIAL_DEBUG
        Serial.println("Tare complete");
      #endif
      _resume = true;
    }
  }
  #ifdef SERIAL_DEBUG
    Serial.println("Now, place your known mass on the loadcell.");
    Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");
  #endif
  hx711State = "Add Mass";
  delay(30000);
  
  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
      known_mass = CalibrationWeight;
      if (known_mass != 0) {
        #ifdef SERIAL_DEBUG
          Serial.print("Known mass is: ");
          Serial.println(known_mass);
        #endif
        _resume = true;
      }
  }

  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value
  
  #ifdef SERIAL_DEBUG
    Serial.print("New calibration value has been set to: ");
    Serial.print(newCalibrationValue);
    Serial.println(", use this as calibration value (calFactor) in your project sketch.");
    Serial.print("Save this value to EEPROM adress ");
    Serial.print(calVal_eepromAdress);
    Serial.println("? y/n");
  #endif
  
  _resume = false;
  while (_resume == false) {
    EEPROM.begin(512);
    EEPROM.put(calVal_eepromAdress, newCalibrationValue);
    EEPROM.commit();
    EEPROM.get(calVal_eepromAdress, newCalibrationValue);
    #ifdef SERIAL_DEBUG
      Serial.print("Value ");
      Serial.print(newCalibrationValue);
      Serial.print(" saved to EEPROM address: ");
      Serial.println(calVal_eepromAdress);
    #endif
    _resume = true;
  }
  hx711State = "EEPROM Saved";
  delay(15000);
  
  #ifdef SERIAL_DEBUG
    Serial.println("End calibration");
    Serial.println("***");
    Serial.println("To re-calibrate, send 'r' from serial monitor.");
    Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
    Serial.println("***");
  #endif
  hx711State = "Calib OK";
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  Serial.print("Save this value to EEPROM adress ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  Serial.println("End change calibration value");
  Serial.println("***");
}

void loadCellTare(){
  // receive command from html page to tare load cell
  LoadCell.tareNoDelay();
  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    #ifdef SERIAL_DEBUG
      Serial.println("Tare complete");
    #endif
  }
}

void get_loadCell(){
  static boolean newDataReady = 0;
  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;
  
  // get smoothed value from the dataset:
  if (newDataReady) {
    force = LoadCell.getData();
    //Serial.print("Load_cell output val: ");
    
    newDataReady = 0;
  }
}

void get_temperature(){
  temperature = thermocouple.readCelsius();
  //internalTemp = thermocouple.readInternal();
  if (isnan(temperature)) {
    #ifdef SERIAL_DEBUG
      //Serial.println("Something wrong with thermocouple!");
    #endif
  }
}

void get_pressure(){
  //Serial.println("Get Pressure");
}

void initializeMAX31855(){
  if (!thermocouple.begin()) {
    #ifdef SERIAL_DEBUG
      Serial.println("MAX31885 Begin ERROR.");
    #endif
  } else {
    #ifdef SERIAL_DEBUG
      Serial.println("MAX31885 Initialized.");
    #endif
  }
}
