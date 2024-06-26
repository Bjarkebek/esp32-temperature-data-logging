/**
 * @file main.cpp
 * @brief Temperature logging system using ESP32, SD card, DS18B20 sensor, and NTPClient.
 * 
 * This project logs temperature readings from a DS18B20 sensor onto an SD card along with 
 * the date and time obtained from an NTP server. It also provides a WebSocket interface to 
 * update temperature readings in real-time on a web interface.
 * 
 * @author Bjarke Bekner
 * @date 06/05/24
 */

#include <Arduino.h>

/**
 * @defgroup SD SD Card Libraries
 * @brief Libraries for SD card
 * @{
 */
#include "FS.h"
#include "SD.h"
#include <SPI.h>
/** @} */

/**
 * @defgroup DS18B20 DS18B20 Libraries
 * @brief Libraries for DS18B20 temperature sensor
 * @{
 */
#include <OneWire.h>
#include <DallasTemperature.h>
/** @} */

/**
 * @defgroup NTP NTP Client Libraries
 * @brief Libraries to get time from NTP Server
 * @{
 */
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
/** @} */

/**
 * @defgroup WebServer Webserver Libraries
 * @brief Libraries for webserver
 * @{
 */
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
/** @} */

/** Declarations */
void getReadings();
void getTimeStamp();
void logSDCard();
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendFile(fs::FS &fs, const char *path, const char *message);

/** Define deep sleep options */
uint64_t uS_TO_S_FACTOR = 1000000; /**< Conversion factor for micro seconds to seconds */
/** Sleep for 10 minutes = 600 seconds */
uint64_t TIME_TO_SLEEP = 600;

/** Network credentials */
const char *ssid = "E308";
const char *password = "98806829";

/** Define serverport */
AsyncWebServer server(80); /**< Set up an AsyncWebServer instance */
AsyncWebSocket ws("/ws");

/** Define CS pin for the SD card module */
#define SD_CS 5

/** Save reading number on RTC memory */
RTC_DATA_ATTR int readingID = 0;

String dataMessage;

/** Data wire is connected to ESP32 GPIO 21 */
#define ONE_WIRE_BUS 21

/** Setup a oneWire instance to communicate with a OneWire device */
OneWire oneWire(ONE_WIRE_BUS);

/** Pass our oneWire reference to Dallas Temperature sensor */
DallasTemperature sensors(&oneWire);

/** Temperature Sensor variables */
float temperature;

/** Define NTP Client to get time */
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

/** Variables to save date and time */
String formattedDate;
String dayStamp;
String timeStamp;

/** -------------------------------------------- Temperature logging functions */

/**
 * @brief Get temperature reading from DS18B20 sensor.
 * 
 * This function requests temperature readings from the DS18B20 sensor and stores
 * the temperature value in the global variable `temperature`.
 */
void getReadings()
{
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0); /**< Temperature in Celsius */
  /// temperature = sensors.getTempFByIndex(0); /**< Temperature in Fahrenheit */
  Serial.print("Temperature: ");
  Serial.println(temperature);

  /// sends last temp read to websocket to update chart
  ws.textAll(String(temperature));

  getTimeStamp();
}

/**
 * @brief Get date and time from NTP server.
 * 
 * This function retrieves the current date and time from an NTP server and stores
 * them in the global variables `formattedDate`, `dayStamp`, and `timeStamp`.
 */
void getTimeStamp()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  /// The formattedDate comes with the following format:
  /// 2018-05-28T16:00:13Z
  /// We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  /// Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  /// Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);

  logSDCard();
}

/**
 * @brief Log sensor readings onto the SD card.
 * 
 * This function constructs a data message including the reading ID, date, time, and
 * temperature, and appends it to the SD card file.
 */
void logSDCard()
{
  dataMessage = String(readingID) + "," + String(dayStamp) + "," + String(timeStamp) + "," +
                String(temperature) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

/**
 * @brief Write data to a file on the file system.
 * 
 * @param fs File system object.
 * @param path Path to the file.
 * @param message Message to write to the file.
 */
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

/**
 * @brief Append data to a file on the file system.
 * 
 * @param fs File system object.
 * @param path Path to the file.
 * @param message Message to append to the file.
 */
void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

/** -------------------------------------------- Websocket */

/**
 * @brief Notify all websocket clients with the latest temperature reading.
 */
void notifyClients() {
  ws.textAll(String(temperature));
}

/**
 * @brief Handle incoming WebSocket messages.
 * 
 * @param arg WebSocket frame information.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    notifyClients();
  }
}

/**
 * @brief Callback function for WebSocket events.
 * 
 * @param server Pointer to the WebSocket server.
 * @param client Pointer to the WebSocket client.
 * @param type WebSocket event type.
 * @param arg Additional arguments.
 * @param data Pointer to the data received.
 * @param len Length of the data received.
 */
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

/**
 * @brief Initialize WebSocket communication.
 */
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  /** Start serial communication for debugging purposes */
  Serial.begin(115200);

  /** Initialize SPIFFS */
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  /** Connect to Wi-Fi network with SSID and password */
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  /** Print ESP32 Local IP Address */
  Serial.println(WiFi.localIP());

  /** Route for root / web page */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });
  
  /** Route to load style.css file */
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  /** Route to load script.js file */
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/script.js");
  });

  /** Route to load favicon.png file -- gives the browser window an icon */
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png");
  });

  /** Start server */
  server.begin();

  /** Initialize a NTPClient to get time */
  timeClient.begin();
  timeClient.setTimeOffset(7200); /**< GMT +1 (+summertime) = 7200 */

  /** Initialize SD card */
  SD.begin(SD_CS);
  if (!SD.begin(SD_CS)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)){
    Serial.println("ERROR - SD card initialization failed!");
    return; /**< init failed */
  }

  /** If the data.txt file doesn't exist
   * Create a file on the SD card and write the data labels */
  File file = SD.open("/data.txt");
  if (!file){
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading ID, Date, Hour, Temperature \r\n");
  } else{
    Serial.println("File already exists");
  }
  file.close();

  /** Start the DallasTemperature library */
  sensors.begin();

  initWebSocket();

  /** Increment readingID on every new reading */
  readingID++;
}

void loop()
{
  getReadings();
  delay(10000); /**< Update every 10 seconds */
  // delay(1000 * 60); /**< Update every minute */
  // delay(1000 * 60 * 60); /**< Update every hour */
}