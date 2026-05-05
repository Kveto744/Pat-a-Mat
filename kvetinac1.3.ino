#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ===== WIFI =====
const char* ssid = "patovhrad"; //meno vasej siete
const char* password = "likavka124";  //heslo od vasej siete

// ===== PINY =====
const int sensorPin = 34;
const int relayPin = 25;

// ===== DHT11 =====
#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== I2C =====
#define SDA_PIN 21
#define SCL_PIN 22

// ===== DISPLAY =====
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ===== SENSORY =====
BH1750 lightMeter;

// ===== WEB =====
WebServer server(80);

// ===== TIME =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
int screenToggle = 0;
unsigned long lastSwitch = 0;
// ===== DATA =====
int soil = 0;
float lightLevel = 0;
float temperature = 0;
float humidity = 0;

// ===== REŽIMY =====
bool autoMode = true;
bool timeMode = false;

// ===== TIMER =====
unsigned long lastWatering = 0;
const unsigned long minInterval = 60000;

// ===== TIME =====
int customHour = 18;
int customMinute = 0;

// ===== PLANT =====
struct PlantProfile {
  String name;
  int soilMin;
  float tempMin;
  float lightMin;
  int requiredConditions;
  int wateringTime;
};

PlantProfile plants[] = {
  {"kaktus", 3200, 22, 900, 1, 5000},
  {"bazalka", 2500, 20, 500, 2, 5000},
  {"paradajka", 2200, 22, 700, 2, 5000},
  {"mäta", 2300, 18, 400, 2, 5000},
  {"šalát", 2100, 16, 300, 2, 5000},
  {"paprika", 2400, 22, 600, 2, 5000},
  {"jahoda", 2300, 20, 500, 2, 5000},
  {"rozmarín", 3000, 20, 800, 1, 5000},
  {"orchidea", 2600, 21, 400, 2, 5000},
  {"špenát", 2200, 18, 350, 2, 5000}
};

PlantProfile currentPlant = plants[0];

// ===== POLIEVANIE =====
void waterPlant(int timeMs) {
  digitalWrite(relayPin, LOW);
  delay(timeMs);
  digitalWrite(relayPin, HIGH);
  lastWatering = millis();
}

// ===== SENSORY =====
void readSensors() {
  soil = analogRead(sensorPin);
  lightLevel = lightMeter.readLightLevel();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

// ===== SMART =====
void smartWatering() {
  int met = 0;

  if (soil > currentPlant.soilMin) met++;
  if (temperature > currentPlant.tempMin) met++;
  if (lightLevel > currentPlant.lightMin) met++;

  if (met >= currentPlant.requiredConditions &&
      millis() - lastWatering > minInterval) {
    waterPlant(currentPlant.wateringTime);
  }
}

// ===== TIME MODE =====
void checkTimeWatering() {
  if (!timeMode) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == customHour &&
      timeinfo.tm_min == customMinute &&
      millis() - lastWatering > minInterval) {
    waterPlant(10000);
  }
}

// ===== DISPLAY =====
void updateDisplay() {

  // prepínanie každé 2 sekundy
  if (millis() - lastSwitch > 2000) {
    lastSwitch = millis();
    screenToggle = !screenToggle;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(WHITE);

  // ===== RIADOK 1 (názov rastliny) =====
  display.setTextSize(2);
  display.println(currentPlant.name);

  // ===== RIADKY 2 + 3 (veľké písmo) =====
  display.setTextSize(2);

  if (screenToggle == 0) {
    // 🌡️ + 💧
    display.print("T:");
    display.println(temperature);

    display.print("H:");
    display.println(humidity);
  } else {
    // 🌱 + ☀️
    display.print("S:");
    display.println(soil);

    display.print("L:");
    display.println(lightLevel);
  }

  display.display();
}

// ===== API =====
void handleData() {
  String json = "{";
  json += "\"soil\":" + String(soil) + ",";
  json += "\"temp\":" + String(temperature) + ",";
  json += "\"hum\":" + String(humidity) + ",";
  json += "\"light\":" + String(lightLevel);
  json += "}";
  server.send(200, "application/json", json);
}

// ===== ROUTES =====
void handleWater() {
  int sec = server.arg("sec").toInt();
  waterPlant(sec * 1000);
  server.send(200, "text/plain", "OK");
}

void handleSetTime() {
  customHour = server.arg("h").toInt();
  customMinute = server.arg("m").toInt();
  server.send(200, "text/plain", "OK");
}

void handleSetPlant() {
  int id = server.arg("id").toInt();
  if (id >= 0 && id < 10) currentPlant = plants[id];
  server.send(200, "text/plain", "OK");
}

void handleAuto(){
  autoMode = true;
  timeMode = false;
  server.send(200, "text/plain", "AUTO");
}

void handleManual(){
  autoMode = false;
  timeMode = false;
  server.send(200, "text/plain", "MANUAL");
}

void handleTime(){
  timeMode = true;
  autoMode = false;
  server.send(200, "text/plain", "TIME");
}

// ===== WEB =====
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
body {
  font-family: Arial;
  background:#f4f7f6;
  text-align:center;
}

.card {
  background:white;
  padding:15px;
  margin:15px;
  border-radius:10px;
  box-shadow:0 2px 8px rgba(0,0,0,0.2);
}

h1 {
  color:#2e7d32;
}

button {
  padding:12px 20px;
  margin:6px;
  border:none;
  border-radius:10px;
  background:#4CAF50;
  color:white;
  font-size:16px;
  transition:0.15s;
}

/* klik animacia */
button:active {
  transform: scale(0.92);
  background:#2e7d32;
}

input, select {
  padding:8px;
  margin:5px;
  border-radius:6px;
  border:1px solid #ccc;
}

/* aktivny mod */
.active {
  background:#2e7d32 !important;
}

</style>
</head>

<body>

<h1>🌱 Smart kvetináč</h1>

<div class="card">
<h3>Senzory</h3>
<p>Vlhkosť pôdy: <span id="soil">--</span></p>
<p>Teplota: <span id="temp">--</span> °C</p>
<p>Vlhkosť vzduchu: <span id="hum">--</span> %</p>
<p>Svetlo: <span id="light">--</span> lx</p>
</div>

<div class="card">
<h3>Rastlina</h3>
<select id="plant">
<option value="0">Kaktus</option>
<option value="1">Bazalka</option>
<option value="2">Paradajka</option>
<option value="3">Mäta</option>
<option value="4">Šalát</option>
<option value="5">Paprika</option>
<option value="6">Jahoda</option>
<option value="7">Rozmarín</option>
<option value="8">Orchidea</option>
<option value="9">Špenát</option>
</select><br>
<button onclick="setPlant(this)">Nastaviť rastlinu</button>
</div>

<div class="card">
<h3>Manuálne polievanie</h3>
<input id="sec" placeholder="sekundy">
<button onclick="water(this)">Zaliať</button>
</div>

<div class="card">
<h3>Čas polievania</h3>
<input id="h" placeholder="hodina">
<input id="m" placeholder="minúta">
<button onclick="setTime(this)">Nastaviť čas</button>
</div>

<div class="card">
<h3>Režim</h3>
<button id="autoBtn" onclick="setMode(this,'auto')">AUTO</button>
<button id="manualBtn" onclick="setMode(this,'manual')">MANUAL</button>
<button id="timeBtn" onclick="setMode(this,'time')">TIME</button>
</div>

<script>

// 🔄 LIVE DATA
setInterval(()=>{
 fetch('/data')
 .then(r=>r.json())
 .then(d=>{
  document.getElementById('soil').innerText = d.soil;
  document.getElementById('temp').innerText = d.temp;
  document.getElementById('hum').innerText = d.hum;
  document.getElementById('light').innerText = d.light;
 });
},2000);


// 💧 MANUAL WATER
function water(btn){
  flash(btn);
  let sec = document.getElementById('sec').value;
  if(!sec || sec < 5) sec = 5; // minimum 5s
  fetch('/water?sec='+sec);
}


// ⏰ TIME
function setTime(btn){
  flash(btn);
  let h = document.getElementById('h').value;
  let m = document.getElementById('m').value;
  fetch('/setTime?h='+h+'&m='+m);
}


// 🌱 PLANT
function setPlant(btn){
  flash(btn);
  let id = document.getElementById('plant').value;
  fetch('/setPlant?id='+id);
}


// 🔁 MODE
function setMode(btn,mode){
  flash(btn);

  fetch('/'+mode);

  // vizuálne označenie
  document.getElementById('autoBtn').classList.remove('active');
  document.getElementById('manualBtn').classList.remove('active');
  document.getElementById('timeBtn').classList.remove('active');

  btn.classList.add('active');
}


// 🔥 klik animacia (extra)
function flash(btn){
  btn.style.background="#2e7d32";
  setTimeout(()=>{
    btn.style.background="#4CAF50";
  },150);
}

</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  lightMeter.begin();
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/water", handleWater);
  server.on("/setTime", handleSetTime);
  server.on("/setPlant", handleSetPlant);
  server.on("/auto", handleAuto);
  server.on("/manual", handleManual);
  server.on("/time", handleTime);

  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  readSensors();
  updateDisplay();

  if (autoMode) smartWatering();
  checkTimeWatering();

  delay(100);
}
}
