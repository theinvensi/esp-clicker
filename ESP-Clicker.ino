#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Servo.h>

static const int servoPin = 0;
Servo servo1;

ESP8266WebServer server(80);
String APP_VERSION = "v1.0";

String sta_ssid = "", sta_pass = "";
String ap_ssid = "ESP-Clicker", ap_pass = "12345678";
bool ap_hidden = false;
bool isSTAConfigured = false;
IPAddress ap_ip;

const char* CONFIG_FILE = "/config.json";

bool loadCredentials() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return false;
  }

  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("Configuration file not found.");
    return false;
  }

  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("Failed to open configuration file.");
    return false;
  }

  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  sta_ssid = doc["sta_ssid"] | "";
  sta_pass = doc["sta_pass"] | "";
  ap_ssid = doc["ap_ssid"] | "ESP-Config";
  ap_pass = doc["ap_pass"] | "12345678";
  ap_hidden = doc["ap_hidden"] | false;

  isSTAConfigured = sta_ssid.length() > 0;

  Serial.println("Credentials loaded.");
  return true;
}

bool saveCredentials() {
  StaticJsonDocument<384> doc;
  doc["sta_ssid"] = sta_ssid;
  doc["sta_pass"] = sta_pass;
  doc["ap_ssid"] = ap_ssid;
  doc["ap_pass"] = ap_pass;
  doc["ap_hidden"] = ap_hidden;

  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("Error writing to file.");
    return false;
  }

  serializeJson(doc, file);
  file.close();
  Serial.println("Settings saved.");
  return true;
}

void startAP() {
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str(), 1, ap_hidden);
  ap_ip = WiFi.softAPIP();
  Serial.println("[AP] Configuration mode active");
  Serial.print("[AP] SSID: ");
  Serial.println(ap_ssid);
  Serial.print("[AP] Hidden: ");
  Serial.println(ap_hidden ? "Yes" : "No");
  Serial.print("[AP] IP: ");
  Serial.println(ap_ip);
}

void tryConnectSTA() {
  WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  Serial.print("[STA] Connecting to ");
  Serial.println(sta_ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[STA] Connected!");
    Serial.print("[STA] IP: ");
    Serial.println(WiFi.localIP());
    delay(500);
  } else {
    Serial.println("[STA] Failed to connect.");
  }
}

bool isFromAP(IPAddress clientIP) {
  return clientIP[0] == ap_ip[0] && clientIP[1] == ap_ip[1] && clientIP[2] == ap_ip[2];
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    IPAddress clientIP = server.client().remoteIP();
    if (isFromAP(clientIP)) {
      String html = "<html><body><h2>Wi-Fi Configuration</h2>";
      html += "<p><b>Version:</b> " + APP_VERSION + "</p>";

      if (WiFi.status() == WL_CONNECTED) {
        html += "<p><b>STA IP:</b> " + WiFi.localIP().toString() + "</p>";
      } else {
        html += "<p><b>STA IP:</b> Not connected</p>";
      }

      html += "<form action='/save' method='POST'>";
      html += "STA SSID: <input name='sta_ssid' value='" + sta_ssid + "'><br>";
      html += "STA Password: <input name='sta_pass' type='password' value='" + sta_pass + "'><br><br>";
      html += "AP SSID: <input name='ap_ssid' value='" + ap_ssid + "'><br>";
      html += "AP Password: <input name='ap_pass' type='password' value='" + ap_pass + "'><br>";
      html += "<label><input type='checkbox' name='ap_hidden'";
      if (ap_hidden) html += " checked";
      html += "> Hide SSID</label><br><br>";
      html += "<input type='submit' value='Save and Restart'>";
      html += "</form></body></html>";
      server.send(200, "text/html", html);
    } else {
      server.send(404, "text/plain", "Not authorized.");
    }
  });

  server.on("/save", HTTP_POST, []() {
    IPAddress clientIP = server.client().remoteIP();
    if (isFromAP(clientIP)) {
      sta_ssid = server.arg("sta_ssid");
      sta_pass = server.arg("sta_pass");
      ap_ssid = server.arg("ap_ssid");
      ap_pass = server.arg("ap_pass");
      ap_hidden = server.hasArg("ap_hidden");

      if (saveCredentials()) {
        server.send(200, "text/html", "<html><body>Saved! Restarting...</body></html>");
        delay(2000);
        ESP.restart();
      } else {
        server.send(500, "text/plain", "Failed to save.");
      }
    } else {
      server.send(403, "text/plain", "Access denied.");
    }
  });

  server.on("/press", HTTP_POST, []() {
    server.send(200, "text/plain", "Pressed");
    servo1.write(90);
  });

  server.on("/release", HTTP_POST, []() {
    server.send(200, "text/plain", "Released");
    servo1.write(0);
  });

  server.begin();
}

void setupServo() {
  servo1.attach(servoPin);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  loadCredentials();

  WiFi.mode(WIFI_AP_STA);
  startAP();

  if (isSTAConfigured) {
    tryConnectSTA();
  }

  setupWebServer();
  setupServo();
}

void loop() {
  server.handleClient();
}
