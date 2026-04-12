#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// ===== WIFI =====
const char* ssid = "TVOJA_SIET";
const char* password = "HESLO";

// ===== PINY =====
const int sensorPin = 34;
const int relayPin = 25;
const int ledPin = 26;

// ===== WEB SERVER =====
WebServer server(80);

// ===== NTP =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// ===== RASTLINY =====
struct PlantProfile {
  String name;
  int moistureMin;
  int wateringTime;
};

PlantProfile cactus = {"cactus", 3000, 1000};
PlantProfile basil  = {"basil", 2000, 2000};
PlantProfile custom = {"custom", 2500, 1500};

PlantProfile currentPlant = cactus;

// ===== CUSTOM CAS =====
int customHour = 18;
int customMinute = 0;

// ===== REŽIM =====
bool autoMode = true;

// ===== TIMER =====
unsigned long lastWatering = 0;
const unsigned long minInterval = 60000;

// ===== POLIEVANIE =====
void waterPlant() {
  Serial.println("💧 Polievam...");

  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, HIGH);

  delay(currentPlant.wateringTime);

  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  lastWatering = millis();
}

// ===== WEB STRÁNKA =====
void handleRoot() {
  String html = R"rawliteral(
    <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>
    <body>
      <h1>🌱 Smart Kvetinac</h1>

      <h3>Vyber rastliny</h3>
      <form action="/setPlant">
        <select name="plant">
          <option value="cactus">Cactus</option>
          <option value="basil">Basil</option>
          <option value="custom">Custom</option>
        </select>
        <input type="submit" value="Nastav">
      </form>

      <h3>Custom nastavenia</h3>
      <form action="/setCustom">
        Cas polievania (ms): <input type="number" name="time"><br><br>
        Hodina: <input type="number" name="hour"><br><br>
        Minuta: <input type="number" name="minute"><br><br>
        Threshold: <input type="number" name="threshold"><br><br>
        <input type="submit" value="Uloz">
      </form>

      <h3>Manual</h3>
      <a href="/water">💧 Zaliať teraz</a>

      <h3>Režim</h3>
      <a href="/auto">AUTO</a><br>
      <a href="/manual">MANUAL</a>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// ===== SET PLANT =====
void handleSetPlant() {
  String plant = server.arg("plant");

  if (plant == "cactus") currentPlant = cactus;
  else if (plant == "basil") currentPlant = basil;
  else if (plant == "custom") currentPlant = custom;

  Serial.println("🌱 Zmena rastliny: " + currentPlant.name);

  server.send(200, "text/plain", "Rastlina nastavena");
}

// ===== CUSTOM =====
void handleSetCustom() {
  if (server.hasArg("time"))
    custom.wateringTime = server.arg("time").toInt();

  if (server.hasArg("threshold"))
    custom.moistureMin = server.arg("threshold").toInt();

  if (server.hasArg("hour"))
    customHour = server.arg("hour").toInt();

  if (server.hasArg("minute"))
    customMinute = server.arg("minute").toInt();

  Serial.println("⚙️ Custom nastavenia ulozene");

  server.send(200, "text/plain", "Custom ulozeny");
}

// ===== MANUAL WATER =====
void handleWater() {
  waterPlant();
  server.send(200, "text/plain", "Zaliate");
}

// ===== MODE =====
void handleAuto() {
  autoMode = true;
  server.send(200, "text/plain", "AUTO zapnute");
}

void handleManual() {
  autoMode = false;
  server.send(200, "text/plain", "MANUAL zapnuty");
}

// ===== TIME WATERING =====
void checkTimeWatering() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  if (currentPlant.name == "custom") {
    if (hour == customHour && minute == customMinute) {
      if (millis() - lastWatering > minInterval) {
        Serial.println("⏰ Cas na polievanie (custom)");
        waterPlant();
        delay(60000);
      }
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  // WIFI
  WiFi.begin(ssid, password);
  Serial.print("Pripajam sa");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nPripojene!");
  Serial.println(WiFi.localIP());

  // TIME
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // WEB
  server.on("/", handleRoot);
  server.on("/setPlant", handleSetPlant);
  server.on("/setCustom", handleSetCustom);
  server.on("/water", handleWater);
  server.on("/auto", handleAuto);
  server.on("/manual", handleManual);

  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  int soil = analogRead(sensorPin);

  Serial.print("Vlhkost: ");
  Serial.println(soil);

  if (autoMode && currentPlant.name != "custom") {
    if (soil > currentPlant.moistureMin &&
        millis() - lastWatering > minInterval) {

      Serial.println("AUTO: Polievam");
      waterPlant();
    }
  }

  checkTimeWatering();

  delay(2000);
}