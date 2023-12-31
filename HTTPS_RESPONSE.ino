#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <ctime>
#include <TinyGPS++.h>

TinyGPSPlus gps;
SoftwareSerial SerialGPS(4, 5); 

const char* ssid = "PLDTHOMEFIBRe3960";
const char* password = "PLDTWIFIjhq97";
const char* firebaseHost = "ube-express-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* dataPath = "/buses/B00001.json";
float Latitude, Longitude;
int year, month, date, hour, minute, second;
int emergency;


time_t getEpochTime(int year, int month, int date, int hour, int minute, int second)
{
  struct tm t;
  t.tm_year = year - 1900;  // Years since 1900
  t.tm_mon = month - 1;     // Months since January (0-11)
  t.tm_mday = date;         // Day of the month (1-31)
  t.tm_hour = hour;         // Hours since midnight (0-23)
  t.tm_min = minute;        // Minutes after the hour (0-59)
  t.tm_sec = second;        // Seconds after the minute (0-61, allows for leap seconds)
  t.tm_isdst = 0;           // Daylight Saving Time flag (0 for no DST)
  return mktime(&t);
}

void setup() {

  Serial.begin(9600);
  SerialGPS.begin(9600);
  pinMode(D6, INPUT);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
}

void loop() {
  static unsigned long previousMillis = 0;
  const unsigned long interval = 10000; // 10 seconds

  while (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      if (gps.location.isValid()) {
        Latitude = gps.location.lat();
        Longitude = gps.location.lng();
      }

      if (gps.date.isValid()) {
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();
      }

      if (gps.time.isValid()) {
        hour = gps.time.hour() + 8;  // Adjust for your timezone offset
        minute = gps.time.minute();
        second = gps.time.second();
      }
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (gps.location.isValid()) {
      putData();
    }
  }

  if (digitalRead(D6) == 1 && emergency != 1) {
    emergency = 1;
    Serial.println("Emergency");
  }
}

void putData() {
WiFiClientSecure client;
client.setInsecure(); // Ignore SSL certificate verification
HTTPClient http;


  String firebaseURL = String("https://") + firebaseHost + "/buses/B00001/emergency.json";

  http.begin(client, firebaseURL);

  // Send the GET request
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
    if (response == "\"notified\"") {
      emergency = 0;
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  // End the HTTPS connection
  http.end();

  // Construct the Firebase Realtime Database URL
  firebaseURL = String("https://") + firebaseHost + dataPath;
  String json = "{";
  json += "\"latitude\":" + String(Latitude, 6) + ",";
  json += "\"longitude\":" + String(Longitude, 6) + ",";
  json += "\"epochTime\":" + String(getEpochTime(year, month, date, hour, minute, second)) + ",";
  json += "\"emergency\":" + String(emergency);
  json += "}";

  // Start the HTTPS connection
  http.begin(client, firebaseURL);

  // Set the content type header
  http.addHeader("Content-Type", "application/json");

  // Track the response time
  unsigned long requestTime = millis();

  // Send the PUT request with the JSON data
  httpResponseCode = http.PUT(json);

  unsigned long responseTime = millis() - requestTime;

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
    Serial.print("Response Time: ");
    Serial.print(responseTime);
    Serial.println(" ms");

    // Append response time to /test.json
    firebaseURL = String("https://") + firebaseHost + "/test.json";
    String appendJson = "{\"" + String(getEpochTime(year, month, date, hour, minute, second)) + "\":" + String(responseTime) + "}";

    http.begin(client, firebaseURL);
    http.addHeader("Content-Type", "application/json");
    httpResponseCode = http.PATCH(appendJson);
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code for appending response time: ");
      Serial.println(httpResponseCode);
      String appendResponse = http.getString();
      Serial.println(appendResponse);
    } else {
      Serial.print("Error code for appending response time: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  // End the HTTPS connection
  http.end();
}

