/*
  ESP32-C6 — MQTT (SoC lesen) + Modbus TCP (Feed-in-Limit konstant 800 W schreiben)
  ----------------------------------------------------
  - MQTT: empfängt SoC (Topic: N/demo/system/Dc/Battery/Soc)
  - Modbus TCP: schreibt konstant 800 W auf Register 2706 (MaxFeedInPower)
*/

#include <Network.h>
#include <ETH.h>
#include <SPI.h>

#include <WiFi.h>              // NUR für WiFiClient als generischen TCP-Client (kein WiFi wird benutzt)
#include <PubSubClient.h>
#include <ModbusIP_ESP8266.h>
#include <ArduinoJson.h>

// ==== LAN (W5500) ====
#define ETH_PHY_TYPE   ETH_PHY_W5500
#define ETH_PHY_ADDR   1
#define ETH_PHY_CS     5
#define ETH_PHY_IRQ    4     
#define ETH_PHY_RST   -1     

#define ETH_SPI_SCK    6
#define ETH_SPI_MISO   2
#define ETH_SPI_MOSI   7

static volatile bool eth_connected = false;

// ==== STATIC IP (ESP32) ====
// -> LOCAL_IP muss frei sein in deinem Netz!
IPAddress LOCAL_IP (192, 168, 0, 50);
IPAddress GATEWAY  (192, 168, 0, 1);
IPAddress SUBNET   (255, 255, 255, 0);
IPAddress DNS1     (192, 168, 0, 1);
IPAddress DNS2     (8, 8, 8, 8);

// ==== MQTT ====
#define MQTT_HOST  "192.168.0.106"
#define MQTT_PORT  1883

// MQTT Topics (vrm-id: c0619aba3d93 -> siehe victron gerät)
#define MQTT_TOPIC_VBAT   "N/c0619aba3d93/system/0/Dc/Battery/Voltage" 
#define MQTT_TOPIC_IBAT   "N/c0619aba3d93/system/0/Dc/Battery/Current" 
#define MQTT_TOPIC_PBAT   "N/c0619aba3d93/system/0/Dc/Battery/Power"   
#define MQTT_TOPIC_SOC    "N/c0619aba3d93/system/0/Dc/Battery/Soc"    

// ---- Victron MQTT "Wake up" / KeepAlive ----
#define MQTT_TOPIC_WAKE_SERIAL     "R/c0619aba3d93/system/0/Serial"
#define MQTT_TOPIC_WAKE_KEEPALIVE  "R/c0619aba3d93/keepalive"
#define KEEPALIVE_INTERVAL_MS      30000UL

// ==== Modbus TCP Ziel ====
IPAddress MB_SERVER_IP(192, 168, 0, 106);
#define MODBUSIP_PORT 502
#define MB_UNIT_ID_SETTINGS 100

#define REG_MAX_FEEDIN 2706

// ====== NEU: Grid power setpoint (signed) ======
#define REG_GRID_SETPOINT 2700
#define GRID_SP_MIN_W    -100
#define GRID_SP_MAX_W     300

// TCP Client (funktioniert auch für Ethernet, obwohl Name WiFiClient ist)
WiFiClient netClient;
PubSubClient mqtt(netClient);

ModbusIP mb;

// globale Werte
float g_soc  = NAN;   // %
float g_vbat = NAN;   // V
float g_ibat = NAN;   // A
float g_pbat = NAN;   // W

// ====== live Setpoint ======
volatile int16_t g_grid_setpoint_w = 0;

// ===== Ethernet Events =====
void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  Serial.printf("[EV] event=%d\n", (int)event);
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[EV] ETH Started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[EV] ETH Connected (link up)");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("[EV] ETH Got IP");
      Serial.print("IP: "); Serial.println(ETH.localIP());
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[EV] ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("[EV] ETH Lost IP");
      eth_connected = false;
      break;
    default:
      break;
  }
}

// -------- MQTT Callback --------
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.printf("[MQTT] JSON parse error on %s: %s\n", topic, err.c_str());
    Serial.printf("[MQTT] Raw: %s\n", msg.c_str());
    return;
  }

  if (!doc.containsKey("value")) {
    Serial.printf("[MQTT] No 'value' field on %s. Raw: %s\n", topic, msg.c_str());
    return;
  }

  float value = doc["value"].as<float>();
  String t(topic);

  if (t.endsWith("/Dc/Battery/Soc")) {
    g_soc = value;
  } else if (t.endsWith("/Dc/Battery/Voltage")) {
    g_vbat = value;
  } else if (t.endsWith("/Dc/Battery/Current")) {
    g_ibat = value;
  } else if (t.endsWith("/Dc/Battery/Power")) {
    g_pbat = value;
  }
}

// -------- Victron "Wake up" / KeepAlive Publish --------
void sendVictronKeepAlive() {
  if (!mqtt.connected()) return;
  mqtt.publish(MQTT_TOPIC_WAKE_SERIAL, "{}", false);
  mqtt.publish(MQTT_TOPIC_WAKE_KEEPALIVE, "{\"value\":1}", false);
}

// -------- Helper: Modbus-Write (blocking) --------
bool writeU16_blocking(uint16_t reg, uint16_t val, uint32_t timeout_ms = 500) {
  uint16_t tr = mb.writeHreg(MB_SERVER_IP, reg, val, nullptr, MB_UNIT_ID_SETTINGS);
  if (tr == 0) return false;
  uint32_t t0 = millis();
  while (mb.isTransaction(tr)) {
    mb.task();
    if (millis() - t0 > timeout_ms) return false;
    delay(1);
  }
  return true;
}

bool writeI16_blocking(uint16_t reg, int16_t val, uint32_t timeout_ms = 500) {
  return writeU16_blocking(reg, (uint16_t)val, timeout_ms);
}

int16_t clamp_grid_sp(long v) {
  if (v < GRID_SP_MIN_W) return GRID_SP_MIN_W;
  if (v > GRID_SP_MAX_W) return GRID_SP_MAX_W;
  return (int16_t)v;
}

void handleSerialSetpoint() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        long req = line.toInt();
        int16_t clamped = clamp_grid_sp(req);
        g_grid_setpoint_w = clamped;
        Serial.printf("[Setpoint] Request=%ld W -> clamp=%d W (min=%d, max=%d)\n",
                      req, (int)clamped, (int)GRID_SP_MIN_W, (int)GRID_SP_MAX_W);
      }
      line = "";
      return;
    }
    line += c;
    if (line.length() > 32) line = "";
  }
}

// -------- LAN Connect (W5500 + statische IP) --------
void connectLAN() {
  Network.begin();
  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);

  // vorher war 1 MHz -> bringt Stabilität, ist aber langsam.
  // 12 MHz ist meist stabil. Wenn du wieder komische Effekte siehst: auf 1 MHz zurück.
  SPI.setFrequency(12000000);
  Serial.println("[BOOT] SPI started at 12 MHz");

  bool ok = ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);
  Serial.printf("[BOOT] ETH.begin ok=%d\n", ok);

  // statische IP setzen
  ETH.config(LOCAL_IP, GATEWAY, SUBNET, DNS1, DNS2);

  Serial.print("[ETH] Warten auf IP");
  while (!eth_connected) {
    Serial.print(".");
    delay(300);
  }
  Serial.printf("\n[ETH] OK, IP: %s\n", ETH.localIP().toString().c_str());
}

// -------- MQTT --------
void ensureMqtt() {
  if (mqtt.connected()) return;

  Serial.print("[MQTT] Verbinden...");
  if (mqtt.connect("esp32c6-client")) {
    Serial.println("OK");
    mqtt.subscribe(MQTT_TOPIC_SOC, 1);
    mqtt.subscribe(MQTT_TOPIC_VBAT, 1);
    mqtt.subscribe(MQTT_TOPIC_IBAT, 1);
    mqtt.subscribe(MQTT_TOPIC_PBAT, 1);

    sendVictronKeepAlive();
  } else {
    Serial.printf("FAIL rc=%d\n", mqtt.state());
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n[BOOT] start");

  connectLAN();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);

  mb.client();
  mb.connect(MB_SERVER_IP);

  g_grid_setpoint_w = 0;
  bool ok_sp = writeI16_blocking(REG_GRID_SETPOINT, g_grid_setpoint_w);
  Serial.printf("[Modbus] Schreibe Grid-Setpoint 0 W -> %s\n", ok_sp ? "OK" : "ERR");

  Serial.println("[Setup] Bereit: MQTT (SoC) + Modbus (FeedLimit=800)");
  Serial.println("[Setpoint] Tippe Wert in W (-100..300) und drücke Enter.");
}

void loop() {
 
  static uint32_t tLoopPrint = 0;
  if (millis() - tLoopPrint > 2000) {
    tLoopPrint = millis();
    Serial.printf("[LOOP] linkUp=%d ip=%s connected=%d\n",
                  ETH.linkUp(),
                  ETH.localIP().toString().c_str(),
                  eth_connected);
  }

  ensureMqtt();
  mqtt.loop();

  handleSerialSetpoint();

  static uint32_t tKeep = 0;
  if (millis() - tKeep > KEEPALIVE_INTERVAL_MS) {
    tKeep = millis();
    sendVictronKeepAlive();
  }

  mb.task();
  if (!mb.isConnected(MB_SERVER_IP)) mb.connect(MB_SERVER_IP);

  static uint32_t tWrite = 0;
  if (millis() - tWrite > 2000) {
    tWrite = millis();
    bool ok = writeU16_blocking(REG_MAX_FEEDIN, 8);
    Serial.printf("[Modbus] Schreibe Feed-in-Limit 800 W -> %s\n", ok ? "OK" : "ERR");
  }

  static uint32_t tSp = 0;
  if (millis() - tSp > 2000) {
    tSp = millis();
    int16_t sp = g_grid_setpoint_w;
    bool ok = writeI16_blocking(REG_GRID_SETPOINT, sp);
    Serial.printf("[Modbus] Schreibe Grid-Setpoint %d W -> %s\n", (int)sp, ok ? "OK" : "ERR");
  }

  static uint32_t tInfo = 0;
  if (millis() - tInfo > 2000) {
    tInfo = millis();
    Serial.printf(
      "[INFO]\n"
      "  SoC  = %.2f %%\n"
      "  U_bat = %.2f V\n"
      "  I_bat = %.2f A\n"
      "  P_bat = %.2f W\n"
      "  MQTT=%s  MB=%s\n\n",
      g_soc,
      g_vbat,
      g_ibat,
      g_pbat,
      mqtt.connected() ? "OK" : "X",
      mb.isConnected(MB_SERVER_IP) ? "OK" : "X"
    );
  }

  // kleiner yield
  delay(1);
}
