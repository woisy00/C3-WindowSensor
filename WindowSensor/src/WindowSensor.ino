#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoHA.h>

#define VERSION "1.0.4"
//#define DEBUG 1

#define PIN_NO 1
#define PIN_NC 2
#define PIN_BAT_ADC 4
#define PIN_BAT_PWR 3
#define PIN_BUTTON 5

#define BATTERY_SAMPLES 5

#define WIFI_RETRY_LIMIT 40
#define WIFI_RETRY_DELAY 50

Preferences prefs;
WiFiManager wm;
WiFiClient wifiClient;

byte mac[6];

String device_name;
String mqtt_host;
String mqtt_user;
String mqtt_pass;
unsigned long sleep_time;
unsigned long loop_time;

HADevice haDevice;
HAMqtt mqtt(wifiClient, haDevice);
HABinarySensor windowSensor("window");
HASensorNumber batterySensor("battery", HASensorNumber::PrecisionP2);

WiFiManagerParameter p_dev;
WiFiManagerParameter p_host;
WiFiManagerParameter p_user;
WiFiManagerParameter p_pass;
WiFiManagerParameter p_sleep_time;
WiFiManagerParameter p_loop_time;

bool shouldSaveConfig = false;

bool windowOpen = false;

unsigned long wakeDeadline;

/* ---------- LOGGING ---------- */

#ifdef DEBUG
void logMsg(String message) {
  Serial.println(message);
  Serial.flush();
}
#endif

/* ---------- CONFIG STORAGE ---------- */

// ---------------- Helper: parse string -> millis ----------------
unsigned long parseDuration(const String& input) {
  String s = input;
  s.toLowerCase();
  unsigned long total = 0;
  int number = 0;

  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isdigit(c)) {
      number = number * 10 + (c - '0');
    } else {
      switch (c) {
        case 'h': total += number * 3600UL * 1000UL; break;
        case 'm': total += number * 60UL * 1000UL; break;
        case 's': total += number * 1000UL; break;
        default: break;
      }
      number = 0;
    }
  }
  // if user entered only a number (assume seconds)
  if (number > 0) total += number * 1000UL;
  return total;
}

// ---------------- Helper: millis -> human-readable ----------------
String millisToHuman(unsigned long ms) {
  unsigned long seconds = ms / 1000UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;
  seconds %= 60UL;

  String result = "";
  if (hours) result += String(hours) + "h";
  if (minutes) result += String(minutes) + "m";
  if (seconds || result == "") result += String(seconds) + "s";
  return result;
}

void saveConfig() {

  device_name = p_dev.getValue();
  mqtt_host = p_host.getValue();
  mqtt_user = p_user.getValue();
  mqtt_pass = p_pass.getValue();
  sleep_time = parseDuration(p_sleep_time.getValue());
  loop_time = parseDuration(p_loop_time.getValue());
  
  prefs.begin("config", false);
  prefs.putString("dev", device_name);
  prefs.putString("mqtt_host", mqtt_host);
  prefs.putString("mqtt_user", mqtt_user);
  prefs.putString("mqtt_pass", mqtt_pass);

  prefs.putULong("sleep_time", sleep_time);
  prefs.putULong("loop_time", loop_time);
  
  prefs.end();
}

bool loadConfig() {
  prefs.begin("config", true);
  device_name = prefs.getString("dev", "");
  mqtt_host = prefs.getString("mqtt_host", "");
  mqtt_user = prefs.getString("mqtt_user", "");
  mqtt_pass = prefs.getString("mqtt_pass", "");

  sleep_time = prefs.getULong("sleep_time", 4*60*60*1000);
  loop_time = prefs.getULong("loop_time", 3000);
  prefs.end();

  #ifdef DEBUG
  logMsg("Loaded config:");
  logMsg("  device_name: " + device_name);
  logMsg("  mqtt_host: " + mqtt_host);
  logMsg("  mqtt_user: " + mqtt_user);
  logMsg("  mqtt_pass: " + mqtt_pass);
  logMsg("  sleep_time: " + millisToHuman(sleep_time));
  logMsg("  loop_time: " + millisToHuman(loop_time));
  #endif
  return mqtt_host.length() > 0;
}


/* ---------- CONFIG PORTAL ---------- */


void saveConfigCallback() {
  shouldSaveConfig = true;
}

void configureWiFiManager() {  
  new (&p_dev) WiFiManagerParameter("dev", "Device Name", device_name.c_str(), 32);
  new (&p_host) WiFiManagerParameter("mqtt", "MQTT Host", mqtt_host.c_str(), 64);
  new (&p_user) WiFiManagerParameter("user", "MQTT User", mqtt_user.c_str(), 32);
  new (&p_pass) WiFiManagerParameter("pass", "MQTT Pass", mqtt_pass.c_str(), 32);

  new (&p_sleep_time) WiFiManagerParameter("sleep_time", "Sleep time", millisToHuman(sleep_time).c_str(), 32);
  new (&p_loop_time) WiFiManagerParameter("loop_time", "Loop time", millisToHuman(loop_time).c_str(), 32);
  
  wm.addParameter(&p_dev);
  wm.addParameter(&p_host);
  wm.addParameter(&p_user);
  wm.addParameter(&p_pass);  
  wm.addParameter(&p_sleep_time);  
  wm.addParameter(&p_loop_time);  


  wm.setSaveConfigCallback(saveConfigCallback);
  
  wm.setConfigPortalTimeout(0);
  wm.setShowStaticFields(true); // force show static ip fields
  wm.setShowDnsFields(true);    // force show dns field always  
}

/* ---------- WIFI ---------- */

void connectWiFiFast(bool forceConfig) {  
  WiFi.macAddress(mac);
  WiFi.hostname(device_name);
  wm.setHostname(device_name);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  WiFi.hostname(device_name);
  wm.setHostname(device_name);
  
  bool rebootRequested = false;

  if (MDNS.begin(device_name.c_str())) {
    #ifdef DEBUG
    logMsg("mDNS " + device_name + ".local ready");
    #endif
  }
  
  if (WiFi.waitForConnectResult(8000) != WL_CONNECTED) {
    rebootRequested = true;
    #ifdef DEBUG
    logMsg("WiFi failed... starting captive portal");
    #endif
    configureWiFiManager();
    wm.startConfigPortal("Window-Sensor");
  } else if (forceConfig) {
    WiFi.hostname(device_name);
    wm.setHostname(device_name);
    
    rebootRequested = true;
    configureWiFiManager();
    wm.startWebPortal();
    #ifdef DEBUG
    logMsg("Portal running");
    #endif
    while (wm.getWebPortalActive()) {
      wm.process();
      wakeDeadline = millis() + 10000;
      loop();
    }
    #ifdef DEBUG
    logMsg("Exited portal");
    #endif
  } else {
    WiFi.hostname(device_name);
    wm.setHostname(device_name);    
  }

  if (shouldSaveConfig) {
    saveConfig();
    delay(1000);
  }

  if (rebootRequested) {
    #ifdef DEBUG
    logMsg("Rebooting after WiFiManager exited");
    #endif
    ESP.restart();
  }
}

/* ---------- HARDWARE ---------- */

bool readWindowState() {
  bool no = digitalRead(PIN_NO);

  #ifdef DEBUG
  logMsg("Read window state: " + boolToString(no));
  #endif
  
  return no == LOW;
}

float readBattery() {
  // enable divider (connect to ground)
  pinMode(PIN_BAT_PWR, OUTPUT);
  digitalWrite(PIN_BAT_PWR, LOW);

  delay(20);   // allow voltage to settle

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

  // discard first sample
  analogRead(PIN_BAT_ADC);

  delay(10);

  uint32_t sum = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    sum += analogRead(PIN_BAT_ADC);
    delay(2);
  }
  
  uint16_t raw = sum / BATTERY_SAMPLES;

  // disable divider
  pinMode(PIN_BAT_PWR, INPUT);

  float adc_voltage = (raw / 4095.0) * 2.5;

  float battery_voltage = adc_voltage * 3.128;

  #ifdef DEBUG
  logMsg("Battery state: " + String(battery_voltage));
  #endif
  return battery_voltage;

  // digitalWrite(PIN_BAT_PWR, HIGH);
  // delay(5);
  // analogReadResolution(12);
  // uint16_t raw = analogRead(PIN_BAT_ADC);
  // digitalWrite(PIN_BAT_PWR, LOW);

  
  
  // float voltage = (raw / 4095.0) * 1.1 * 3.128;
  // return voltage;
}

/* ---------- HA SETUP ---------- */

void setupHA() {
  haDevice.setUniqueId(mac, sizeof(mac));
  haDevice.setName(device_name.c_str());
  haDevice.setSoftwareVersion(VERSION);
  haDevice.setModel("BWS-01");
  haDevice.setManufacturer("bergermeier");

  // This method enables availability for all device types registered on the device.
  // For example, if you have 5 sensors on the same device, you can enable
  // shared availability and change availability state of all sensors using
  // single method call "device.setAvailability(false|true)"
  haDevice.enableSharedAvailability();
  

  // Optionally, you can enable MQTT LWT feature. If device will lose connection
  // to the broker, all device types related to it will be marked as offline in
  // the Home Assistant Panel.
  //haDevice.enableLastWill();

  //windowSensor.setIcon("mdi:window-open-variant");
  windowSensor.setDeviceClass("window");
  windowSensor.setName("Window Sensor");
  
  //batterySensor.setIcon("mdi:battery");
  batterySensor.setName("Battery");
  batterySensor.setDeviceClass("voltage");
  batterySensor.setUnitOfMeasurement("V");

  mqtt.begin(mqtt_host.c_str(), mqtt_user.c_str(), mqtt_pass.c_str());
}

/* ---------- WAKE CONFIG ---------- */

void setupWake() {
  
  // Wake after 1 hour
  if (sleep_time > 0) {
    #ifdef DEBUG
    logMsg("Seetting wake timer to " + String(sleep_time));
    #endif
    esp_sleep_enable_timer_wakeup(sleep_time * 1000ULL);   
  }
  
  if (windowOpen) {
    #ifdef DEBUG
    logMsg("Going to sleept now until NC_PIN is low");
    #endif
    esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_NC), ESP_GPIO_WAKEUP_GPIO_LOW);
  } else {
    #ifdef DEBUG
    logMsg("Going to sleept now until NO_PIN is low");
    #endif
    esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_NO), ESP_GPIO_WAKEUP_GPIO_LOW);
  }
  
}

/* ---------- SLEEP ---------- */

void goToSleep() {
  setupWake();
  delay(200);
  esp_deep_sleep_start();
}

bool bootButtonPressed() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(20);
  return digitalRead(PIN_BUTTON) == LOW;
}

/* ---------- SETUP ---------- */

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);    
  #endif
  
  pinMode(PIN_NO, INPUT);
  pinMode(PIN_BAT_PWR, OUTPUT);
  digitalWrite(PIN_BAT_PWR, LOW);

  bool configExists = loadConfig();
  bool portalRequested = bootButtonPressed();
  #ifdef DEBUG
  logMsg("Portal requested");
  logMsg("  config: " + boolToString(configExists));
  logMsg("  button: " + boolToString(portalRequested));
  #endif
  
  connectWiFiFast(!configExists || portalRequested);


  setupHA();

  windowOpen = readWindowState();
  float battery = readBattery();

  
  windowSensor.setState(windowOpen, true);
  batterySensor.setValue(battery, true);
  #ifdef DEBUG
  logMsg("Sending state to HA: window: " + boolToString(windowOpen) + ", batt: " + String(battery));
  #endif

  wakeDeadline = millis() + loop_time;

}

String boolToString(bool b) {
  return b ? "true" : "false";
}

/* ---------- LOOP ---------- */

void loop() {
  mqtt.loop();  

  if (millis() >= wakeDeadline)
    goToSleep();

  delay(5);
}