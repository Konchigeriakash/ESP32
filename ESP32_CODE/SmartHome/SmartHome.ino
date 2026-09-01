#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ==================================================
// Wi-Fi details
// ==================================================

const char* WIFI_SSID = "Akash";
const char* WIFI_PASSWORD = "anything";

// ==================================================
// Render Flask server
// ==================================================

const char* SERVER_URL =
  "https://smart-home-1hjk.onrender.com/device/sync";

// Your device API key
const char* DEVICE_API_KEY =
  "99aea5786b3427016e63e501fcd717ca";

// Check server every 2 seconds
const unsigned long POLL_INTERVAL_MS = 2000;

// ==================================================
// Relay pins
// ==================================================

const int FAN_RELAY_PIN = 23;
const int LIGHT_RELAY_PIN = 22;

// Most relay modules are active LOW
const bool RELAY_ACTIVE_LOW = true;

// ==================================================
// Device state
// ==================================================

bool fanState = false;
bool lightState = false;

unsigned long lastPollMs = 0;


// ==================================================
// SET RELAY
// ==================================================

void setRelay(int pin, bool on)
{
  if (RELAY_ACTIVE_LOW)
  {
    if (on)
      digitalWrite(pin, LOW);
    else
      digitalWrite(pin, HIGH);
  }
  else
  {
    if (on)
      digitalWrite(pin, HIGH);
    else
      digitalWrite(pin, LOW);
  }
}


// ==================================================
// CONNECT TO WI-FI
// ==================================================

bool connectWiFi()
{
  Serial.println();
  Serial.println("================================");
  Serial.println("Connecting to Wi-Fi...");
  Serial.println("================================");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());

    return true;
  }

  Serial.println("Wi-Fi connection FAILED!");

  return false;
}


// ==================================================
// APPLY COMMAND FROM SERVER
// ==================================================

void applyCommand(String command)
{
  command.trim();

  Serial.print("Command received: ");
  Serial.println(command);


  if (command == "fan_on")
  {
    fanState = true;

    setRelay(FAN_RELAY_PIN, true);

    Serial.println("Fan turned ON");
  }


  else if (command == "fan_off")
  {
    fanState = false;

    setRelay(FAN_RELAY_PIN, false);

    Serial.println("Fan turned OFF");
  }


  else if (command == "light_on")
  {
    lightState = true;

    setRelay(LIGHT_RELAY_PIN, false);

    Serial.println("Light turned ON");
  }


  else if (command == "light_off")
  {
    lightState = false;

    setRelay(LIGHT_RELAY_PIN, true);

    Serial.println("Light turned OFF");
  }


  else
  {
    Serial.println("Unknown command");
  }
}


// ==================================================
// EXTRACT COMMAND FROM JSON RESPONSE
//
// Example server response:
//
// {"command":"fan_on"}
//
// ==================================================

String getCommandFromResponse(String response)
{
  int start = response.indexOf("\"command\"");

  if (start == -1)
  {
    return "";
  }

  start = response.indexOf(":", start);

  if (start == -1)
  {
    return "";
  }

  start++;

  // Skip spaces
  while (start < response.length() &&
         (response[start] == ' ' || response[start] == '\"'))
  {
    start++;
  }

  int end = start;

  while (end < response.length() &&
         response[end] != '\"' &&
         response[end] != '}')
  {
    end++;
  }

  String command = response.substring(start, end);

  command.trim();

  return command;
}


// ==================================================
// SYNCHRONIZE WITH SERVER
// ==================================================

void syncWithServer()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi disconnected.");

    return;
  }


  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("Connecting to server...");
  Serial.println("--------------------------------");


  // HTTPS client
  WiFiClientSecure client;

  // IMPORTANT:
  // This disables certificate verification.
  // Useful for testing Render HTTPS.
  client.setInsecure();


  HTTPClient http;


  // Give Render enough time to respond
  http.setConnectTimeout(20000);
  http.setTimeout(20000);


  Serial.print("Server: ");
  Serial.println(SERVER_URL);


  // Start HTTPS connection
  if (!http.begin(client, SERVER_URL))
  {
    Serial.println("ERROR: http.begin() failed!");

    return;
  }


  // HTTP headers
  http.addHeader("Content-Type", "application/json");

  http.addHeader("X-Device-Key", DEVICE_API_KEY);


  // ==================================================
  // Create JSON manually
  // ==================================================

  String payload = "{";

  payload += "\"fan\":\"";
  payload += fanState ? "ON" : "OFF";

  payload += "\",";

  payload += "\"light\":\"";
  payload += lightState ? "ON" : "OFF";

  payload += "\"}";


  Serial.print("Sending: ");
  Serial.println(payload);


  // ==================================================
  // POST request
  // ==================================================

  int httpCode = http.POST(payload);


  Serial.print("HTTP result: ");
  Serial.println(httpCode);


  // ==================================================
  // SUCCESS
  // ==================================================

  if (httpCode > 0)
  {
    Serial.print("HTTP status code: ");
    Serial.println(httpCode);


    String response = http.getString();


    Serial.print("Server response: ");
    Serial.println(response);


    // Server should return something like:
    //
    // {"command":"fan_on"}
    //

    String command = getCommandFromResponse(response);


    if (command.length() > 0)
    {
      applyCommand(command);
    }
    else
    {
      Serial.println("No command received.");
    }
  }


  // ==================================================
  // CONNECTION ERROR
  // ==================================================

  else
  {
    Serial.println();
    Serial.println("!!!!!!!! SERVER CONNECTION ERROR !!!!!!!!");

    Serial.print("HTTP error code: ");
    Serial.println(httpCode);

    Serial.print("Error description: ");
    Serial.println(http.errorToString(httpCode));

    Serial.println("Possible causes:");
    Serial.println("1. Internet connection problem");
    Serial.println("2. DNS problem");
    Serial.println("3. HTTPS/TLS connection problem");
    Serial.println("4. Wi-Fi network blocking connection");
    Serial.println("5. Render server connection problem");
  }


  http.end();
}


// ==================================================
// SETUP
// ==================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 SMART HOME");
  Serial.println("================================");


  // Relay pins
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);


  // Start with both devices OFF
  setRelay(FAN_RELAY_PIN, false);
  setRelay(LIGHT_RELAY_PIN, false);


  // Connect Wi-Fi
  connectWiFi();
}


// ==================================================
// LOOP
// ==================================================

void loop()
{
  // --------------------------------------------------
  // Reconnect if Wi-Fi is lost
  // --------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi lost!");

    connectWiFi();

    delay(1000);

    return;
  }


  // --------------------------------------------------
  // Poll server every 2 seconds
  // --------------------------------------------------

  unsigned long currentMillis = millis();


  if (currentMillis - lastPollMs >= POLL_INTERVAL_MS)
  {
    lastPollMs = currentMillis;

    syncWithServer();
  }
}
