#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <Preferences.h>
#include <LittleFS.h>

#define PIN_LDR        1
#define PIN_BT1        7
#define PIN_BT2        6
#define PIN_BT3        5
#define PIN_OLED_SDA   8
#define PIN_OLED_SCL   9
#define PIN_LED_B      12
#define PIN_LED_G      13
#define PIN_LED_R      14
#define PIN_DHT        15
#define PIN_BUZZER     17

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* AIO_USERNAME = "SEU_USUARIO";
const char* AIO_KEY      = "SUA_AIO_KEY";

const char* MQTT_HOST = "io.adafruit.com";
const uint16_t MQTT_PORT = 1883;

String feedTemp;
String feedHum;
String feedLight;
String feedAlarm;
String feedLedR;
String feedLedG;
String feedLedB;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DHTesp dht;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Preferences prefs;

const char* LOG_FILE = "/alarms.log";

struct Limits {
  float tempMin;
  float tempMax;
  float humMin;
  float humMax;
  int lightMin;
  int lightMax;
};

Limits limits;

float temperatureC = NAN;
float humidityPct = NAN;
int ldrRaw = 0;
int lightPct = 0;

bool wifiOk = false;
bool mqttOk = false;

bool alarmActive = false;
bool buzzerMuted = false;

bool ledRState = false;
bool ledGState = false;
bool ledBState = false;

enum ScreenMode {
  SCREEN_STATUS,
  SCREEN_MENU,
  SCREEN_EDIT
};

enum MenuItem {
  ITEM_TEMP_MAX,
  ITEM_TEMP_MIN,
  ITEM_HUM_MAX,
  ITEM_HUM_MIN,
  ITEM_LIGHT_MAX,
  ITEM_LIGHT_MIN,
  ITEM_EXIT,
  ITEM_COUNT
};

ScreenMode screenMode = SCREEN_STATUS;
int menuIndex = 0;

unsigned long lastSensorMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastPublishMs = 0;
unsigned long lastWiFiCheckMs = 0;
unsigned long lastBuzzerMs = 0;

const unsigned long SENSOR_INTERVAL_MS  = 2000;
const unsigned long DISPLAY_INTERVAL_MS = 250;
const unsigned long PUBLISH_INTERVAL_MS = 10000;
const unsigned long WIFI_CHECK_MS       = 3000;

struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  unsigned long lastDebounce;
};

Button bt1 = {PIN_BT1, HIGH, HIGH, 0};
Button bt2 = {PIN_BT2, HIGH, HIGH, 0};
Button bt3 = {PIN_BT3, HIGH, HIGH, 0};

const unsigned long debounceMs = 35;

void initPins();
void initDisplay();
void initStorage();
void loadLimits();
void saveLimits();

void connectWiFi();
void ensureWiFi();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void connectMQTT();
void ensureMQTT();
void publishData();

void readSensors();
void evaluateAlarms();
void setRgbOutputs();
void setBuzzer(bool on);
void appendAlarmLog(const String& line);

void drawStatusScreen();
void drawMenuScreen();
void drawEditScreen();
void handleButtons();
bool buttonPressed(Button &b);

void handleSerial();
String tsFromMillis();

void setup() {
  Serial.begin(115200);
  delay(300);

  initPins();
  initDisplay();
  initStorage();
  loadLimits();

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  dht.setup(PIN_DHT, DHTesp::DHT22);

  feedTemp  = String(AIO_USERNAME) + "/feeds/temperatura";
  feedHum   = String(AIO_USERNAME) + "/feeds/umidade";
  feedLight = String(AIO_USERNAME) + "/feeds/luminosidade";
  feedAlarm = String(AIO_USERNAME) + "/feeds/alarmes";
  feedLedR  = String(AIO_USERNAME) + "/feeds/led-r";
  feedLedG  = String(AIO_USERNAME) + "/feeds/led-g";
  feedLedB  = String(AIO_USERNAME) + "/feeds/led-b";

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  connectWiFi();
  connectMQTT();

  Serial.println("\nComandos seriais:");
  Serial.println("  logs");
  Serial.println("  clearlogs");
  Serial.println("  status");
}

void loop() {
  unsigned long now = millis();

  handleButtons();
  handleSerial();

  if (now - lastWiFiCheckMs >= WIFI_CHECK_MS) {
    lastWiFiCheckMs = now;
    ensureWiFi();
    ensureMQTT();
  }

  if (wifiOk && mqttOk) {
    mqtt.loop();
  }

  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;
    readSensors();
    evaluateAlarms();
  }

  if (wifiOk && mqttOk && now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;
    publishData();
  }

  if (alarmActive && !buzzerMuted) {
    if (now - lastBuzzerMs >= 400) {
      lastBuzzerMs = now;
      static bool buzz = false;
      buzz = !buzz;
      setBuzzer(buzz);
    }
  } else {
    setBuzzer(false);
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    if (screenMode == SCREEN_STATUS) drawStatusScreen();
    else if (screenMode == SCREEN_MENU) drawMenuScreen();
    else drawEditScreen();
  }
}

void initPins() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(PIN_BT1, INPUT_PULLUP);
  pinMode(PIN_BT2, INPUT_PULLUP);
  pinMode(PIN_BT3, INPUT_PULLUP);

  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, LOW);
  digitalWrite(PIN_BUZZER, LOW);
}

void initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void initStorage() {
  prefs.begin("iot-station", false);
  LittleFS.begin(true);

  if (!LittleFS.exists(LOG_FILE)) {
    File f = LittleFS.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.println("ALARM LOGS:");
      f.close();
    }
  }
}

void loadLimits() {
  limits.tempMin  = prefs.getFloat("tmin", 20.0f);
  limits.tempMax  = prefs.getFloat("tmax", 30.0f);
  limits.humMin   = prefs.getFloat("hmin", 30.0f);
  limits.humMax   = prefs.getFloat("hmax", 80.0f);
  limits.lightMin = prefs.getInt("lmin", 20);
  limits.lightMax = prefs.getInt("lmax", 85);
}

void saveLimits() {
  prefs.putFloat("tmin", limits.tempMin);
  prefs.putFloat("tmax", limits.tempMax);
  prefs.putFloat("hmin", limits.humMin);
  prefs.putFloat("hmax", limits.humMax);
  prefs.putInt("lmin", limits.lightMin);
  prefs.putInt("lmax", limits.lightMax);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Conectando ao WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  wifiOk = (WiFi.status() == WL_CONNECTED);
  if (wifiOk) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha no WiFi");
  }
}

void ensureWiFi() {
  wifiOk = (WiFi.status() == WL_CONNECTED);
  if (!wifiOk) connectWiFi();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  String t = String(topic);

  if (t == feedLedR) ledRState = (msg == "1" || msg == "ON" || msg == "on");
  if (t == feedLedG) ledGState = (msg == "1" || msg == "ON" || msg == "on");
  if (t == feedLedB) ledBState = (msg == "1" || msg == "ON" || msg == "on");

  setRgbOutputs();
}

void connectMQTT() {
  if (!wifiOk) return;

  while (!mqtt.connected()) {
    String clientId = "franz-lab01-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.println("Conectando ao MQTT...");
    if (mqtt.connect(clientId.c_str(), AIO_USERNAME, AIO_KEY)) {
      mqttOk = true;
      Serial.println("MQTT OK");

      mqtt.subscribe(feedLedR.c_str());
      mqtt.subscribe(feedLedG.c_str());
      mqtt.subscribe(feedLedB.c_str());

      mqtt.publish(feedAlarm.c_str(), "device-online");
    } else {
      mqttOk = false;
      Serial.print("MQTT falhou, rc=");
      Serial.println(mqtt.state());
      delay(2000);
      break;
    }
  }
}

void ensureMQTT() {
  mqttOk = mqtt.connected();
  if (!mqttOk && wifiOk) connectMQTT();
}

void publishData() {
  char buf[24];

  dtostrf(temperatureC, 0, 1, buf);
  mqtt.publish(feedTemp.c_str(), buf);

  dtostrf(humidityPct, 0, 1, buf);
  mqtt.publish(feedHum.c_str(), buf);

  snprintf(buf, sizeof(buf), "%d", lightPct);
  mqtt.publish(feedLight.c_str(), buf);
}

void readSensors() {
  TempAndHumidity data = dht.getTempAndHumidity();

  if (!isnan(data.temperature)) temperatureC = data.temperature;
  if (!isnan(data.humidity)) humidityPct = data.humidity;

  ldrRaw = analogRead(PIN_LDR);
  lightPct = constrain(map(ldrRaw, 0, 4095, 0, 100), 0, 100);
}

void evaluateAlarms() {
  bool overTemp  = !isnan(temperatureC) && (temperatureC > limits.tempMax);
  bool underTemp = !isnan(temperatureC) && (temperatureC < limits.tempMin);
  bool overHum   = !isnan(humidityPct)  && (humidityPct > limits.humMax);
  bool underHum  = !isnan(humidityPct)  && (humidityPct < limits.humMin);
  bool overLux   = (lightPct > limits.lightMax);
  bool underLux  = (lightPct < limits.lightMin);

  bool newAlarm = overTemp || underTemp || overHum || underHum || overLux || underLux;

  if (newAlarm && !alarmActive) {
    alarmActive = true;
    buzzerMuted = false;

    String line = tsFromMillis();
    line += " - Temperatura: " + String(temperatureC, 1) + "C";
    line += ", Luminosidade: " + String(lightPct);
    line += ", Umidade: " + String(humidityPct, 1) + "%";
    appendAlarmLog(line);

    if (mqtt.connected()) {
      mqtt.publish(feedAlarm.c_str(), line.c_str());
    }
  }

  if (!newAlarm) {
    alarmActive = false;
    buzzerMuted = false;
  }
}

void setRgbOutputs() {
  digitalWrite(PIN_LED_R, ledRState ? HIGH : LOW);
  digitalWrite(PIN_LED_G, ledGState ? HIGH : LOW);
  digitalWrite(PIN_LED_B, ledBState ? HIGH : LOW);
}

void setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

void appendAlarmLog(const String& line) {
  File f = LittleFS.open(LOG_FILE, FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
  }
}

void drawStatusScreen() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("T:");
  display.print(temperatureC, 1);
  display.print("C U:");
  display.print(humidityPct, 0);
  display.println("%");

  display.setCursor(0, 10);
  display.print("Luz:");
  display.print(lightPct);
  display.println("%");

  display.setCursor(0, 20);
  display.print("WiFi:");
  display.print(wifiOk ? "OK" : "OFF");
  display.print(" MQTT:");
  display.println(mqttOk ? "OK" : "OFF");

  display.setCursor(0, 30);
  display.print("T ");
  display.print(limits.tempMin, 0);
  display.print("-");
  display.print(limits.tempMax, 0);

  display.setCursor(0, 40);
  display.print("U ");
  display.print(limits.humMin, 0);
  display.print("-");
  display.print(limits.humMax, 0);

  display.setCursor(0, 50);
  if (alarmActive) display.print("ALARME! BT1 silencia");
  else display.print("BT1 menu");

  display.display();
}

void drawMenuScreen() {
  const char* items[ITEM_COUNT] = {
    "Temp Max", "Temp Min", "Umid Max", "Umid Min",
    "Luz Max", "Luz Min", "Sair"
  };

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Configurar Alarmes");

  for (int i = 0; i < ITEM_COUNT; i++) {
    display.setCursor(0, 10 + i * 8);
    display.print(i == menuIndex ? ">" : " ");
    display.print(items[i]);
  }
  display.display();
}

void drawEditScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Ajuste BT2/BT3");

  display.setCursor(0, 20);
  switch (menuIndex) {
    case ITEM_TEMP_MAX: display.print("Temp Max: "), display.print(limits.tempMax, 1); break;
    case ITEM_TEMP_MIN: display.print("Temp Min: "), display.print(limits.tempMin, 1); break;
    case ITEM_HUM_MAX: display.print("Umid Max: "), display.print(limits.humMax, 1); break;
    case ITEM_HUM_MIN: display.print("Umid Min: "), display.print(limits.humMin, 1); break;
    case ITEM_LIGHT_MAX: display.print("Luz Max: "), display.print(limits.lightMax); break;
    case ITEM_LIGHT_MIN: display.print("Luz Min: "), display.print(limits.lightMin); break;
  }

  display.setCursor(0, 54);
  display.print("BT1 salva");
  display.display();
}

bool buttonPressed(Button &b) {
  bool reading = digitalRead(b.pin);

  if (reading != b.lastReading) {
    b.lastDebounce = millis();
  }

  if ((millis() - b.lastDebounce) > debounceMs) {
    if (reading != b.stableState) {
      b.stableState = reading;
      if (b.stableState == LOW) {
        b.lastReading = reading;
        return true;
      }
    }
  }

  b.lastReading = reading;
  return false;
}

void handleButtons() {
  bool ok   = buttonPressed(bt1);
  bool up   = buttonPressed(bt2);
  bool down = buttonPressed(bt3);

  if (alarmActive && ok && screenMode == SCREEN_STATUS) {
    buzzerMuted = true;
    return;
  }

  if (screenMode == SCREEN_STATUS) {
    if (ok) screenMode = SCREEN_MENU;
    return;
  }

  if (screenMode == SCREEN_MENU) {
    if (up) menuIndex = (menuIndex - 1 + ITEM_COUNT) % ITEM_COUNT;
    if (down) menuIndex = (menuIndex + 1) % ITEM_COUNT;
    if (ok) {
      if (menuIndex == ITEM_EXIT) screenMode = SCREEN_STATUS;
      else screenMode = SCREEN_EDIT;
    }
    return;
  }

  if (screenMode == SCREEN_EDIT) {
    if (up) {
      switch (menuIndex) {
        case ITEM_TEMP_MAX:  limits.tempMax += 0.5f; break;
        case ITEM_TEMP_MIN:  limits.tempMin += 0.5f; break;
        case ITEM_HUM_MAX:   limits.humMax += 1.0f; break;
        case ITEM_HUM_MIN:   limits.humMin += 1.0f; break;
        case ITEM_LIGHT_MAX: limits.lightMax = min(100, limits.lightMax + 1); break;
        case ITEM_LIGHT_MIN: limits.lightMin = min(100, limits.lightMin + 1); break;
      }
    }
    if (down) {
      switch (menuIndex) {
        case ITEM_TEMP_MAX:  limits.tempMax -= 0.5f; break;
        case ITEM_TEMP_MIN:  limits.tempMin -= 0.5f; break;
        case ITEM_HUM_MAX:   limits.humMax -= 1.0f; break;
        case ITEM_HUM_MIN:   limits.humMin -= 1.0f; break;
        case ITEM_LIGHT_MAX: limits.lightMax = max(0, limits.lightMax - 1); break;
        case ITEM_LIGHT_MIN: limits.lightMin = max(0, limits.lightMin - 1); break;
      }
    }
    if (ok) {
      saveLimits();
      screenMode = SCREEN_MENU;
    }
  }
}

void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "logs") {
    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (!f) {
      Serial.println("Nao foi possivel abrir logs.");
      return;
    }
    while (f.available()) Serial.write(f.read());
    f.close();
    Serial.println();
  } else if (cmd == "clearlogs") {
    File f = LittleFS.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.println("ALARM LOGS:");
      f.close();
      Serial.println("Logs apagados.");
    }
  } else if (cmd == "status") {
    Serial.println("=== STATUS ===");
    Serial.printf("Temperatura: %.1f C\n", temperatureC);
    Serial.printf("Umidade: %.1f %%\n", humidityPct);
    Serial.printf("Luminosidade: %d %%\n", lightPct);
    Serial.printf("Alarme: %s\n", alarmActive ? "ATIVO" : "INATIVO");
  }
}

String tsFromMillis() {
  unsigned long total = millis() / 1000UL;
  unsigned long h = total / 3600UL;
  unsigned long m = (total % 3600UL) / 60UL;
  unsigned long s = total % 60UL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}