#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>   // Install via Library Manager: "ArduinoJson" by Benoit Blanchon

// ===== Wi-Fi details =====
const char* WIFI_SSID = "OPPO";
const char* WIFI_PASSWORD = "abcdefgh";

// ===== Server details =====
// Your deployed Flask app, e.g. "https://your-app.onrender.com"
// Use https:// in production. Only use http:// for local testing.
const char* SERVER_URL = "https://your-app.example.com";

// Must match DEVICE_API_KEY in the server's .env / environment variables.
// This proves to the server that requests are really coming from this
// device -- keep it secret, don't commit a real key to a public repo.
const char* DEVICE_API_KEY = "REPLACE_WITH_DEVICE_API_KEY";

// How often to check in with the server, in milliseconds.
const unsigned long POLL_INTERVAL_MS = 2000;

// ===== Relay setup =====
const int FAN_RELAY_PIN = 23;         // Change if you use another GPIO
const int LIGHT_RELAY_PIN = 22;       // Change if you use another GPIO
const bool RELAY_ACTIVE_LOW = true;   // Most relay modules are active LOW

bool fanState = false;
bool lightState = false;
unsigned long lastPollMs = 0;

void setRelay(int pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("ESP32 local IP (informational only, server doesn't need it): ");
  Serial.println(WiFi.localIP());
}

// Applies a command string received from the server.
void applyCommand(const String& command) {
  if (command == "fan_on") {
    fanState = true;
    setRelay(FAN_RELAY_PIN, true);
    Serial.println("Fan turned ON");
  } else if (command == "fan_off") {
    fanState = false;
    setRelay(FAN_RELAY_PIN, false);
    Serial.println("Fan turned OFF");
  } else if (command == "light_on") {
    lightState = true;
    setRelay(LIGHT_RELAY_PIN, true);
    Serial.println("Light turned ON");
  } else if (command == "light_off") {
    lightState = false;
    setRelay(LIGHT_RELAY_PIN, false);
    Serial.println("Light turned OFF");
  }
}

// Reports current state to the server and, in the same request, receives
// the next queued command (if any). This is the only network call the
// ESP32 ever makes -- it always initiates the connection outward, so no
// port forwarding or public IP is needed on the home network.
void syncWithServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, skipping sync");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "/device/sync";

  bool isHttps = url.startsWith("https://");
  WiFiClientSecure secureClient;

  if (isHttps) {
    // NOTE: for real deployments, prefer http.begin(secureClient, url) with a
    // configured root CA via secureClient.setCACert(...) instead of this.
    // setInsecure() skips certificate validation, which is fine for quick
    // testing but should be tightened before long-term production use.
    secureClient.setInsecure();
    http.begin(secureClient, url);
  } else {
    http.begin(url);
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_API_KEY);

  StaticJsonDocument<128> body;
  body["fan"] = fanState ? "ON" : "OFF";
  body["light"] = lightState ? "ON" : "OFF";
  String payload;
  serializeJson(body, payload);

  int statusCode = http.POST(payload);

  if (statusCode == 200) {
    String response = http.getString();

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, response);

    if (!err && !doc["command"].isNull()) {
      String command = doc["command"].as<String>();
      applyCommand(command);
    }
  } else {
    Serial.print("Sync failed, HTTP status: ");
    Serial.println(statusCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  setRelay(FAN_RELAY_PIN, false);
  setRelay(LIGHT_RELAY_PIN, false);

  connectWiFi();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    syncWithServer();
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
}
