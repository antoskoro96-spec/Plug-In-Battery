#include "mqtt_bridge.h"
#include "eth_manager.h"
#include "state_store.h"

NetworkClient g_netClient;
PubSubClient g_mqtt(g_netClient);

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.printf("[MQTT] JSON parse error on %s: %s\n", topic, err.c_str());
    return;
  }

  if (!doc.containsKey("value")) {
    Serial.printf("[MQTT] No 'value' field on %s\n", topic);
    return;
  }

  float value = doc["value"].as<float>();
  String t(topic);

  if (t.endsWith("/Dc/Battery/Soc")) {
    g_state.telemetry.soc = value;
  } else if (t.endsWith("/Dc/Battery/Voltage")) {
    g_state.telemetry.vbat = value;
  } else if (t.endsWith("/Dc/Battery/Current")) {
    g_state.telemetry.ibat = value;
  } else if (t.endsWith("/Dc/Battery/Power")) {
    g_state.telemetry.pbat = value;
  } else if (t.endsWith("/Settings/CGwacs/AcPowerSetPoint")) {
    g_state.telemetry.grid_sp_actual = clampFloat(value, GRID_SP_MIN, GRID_SP_MAX);

    if (g_state.write.grid_sp_pending &&
        !isnan(g_state.telemetry.grid_sp_requested) &&
        floatEqual(g_state.telemetry.grid_sp_actual, g_state.telemetry.grid_sp_requested, 1.0f)) {
      g_state.write.grid_sp_pending = false;
      g_state.write.grid_sp_failed = false;
      Serial.printf("[MQTT] Grid setpoint CONFIRMED: %.1f W\n", g_state.telemetry.grid_sp_actual);
    }
  } else if (t.endsWith("/Settings/CGwacs/MaxFeedInPower")) {
    g_state.telemetry.max_feedin_actual = clampFloat(value, MAX_FEEDIN_MIN, MAX_FEEDIN_MAX);

    if (g_state.write.max_feedin_pending &&
        !isnan(g_state.telemetry.max_feedin_requested) &&
        floatEqual(g_state.telemetry.max_feedin_actual, g_state.telemetry.max_feedin_requested, 1.0f)) {
      g_state.write.max_feedin_pending = false;
      g_state.write.max_feedin_failed = false;
      Serial.printf("[MQTT] Max feed-in CONFIRMED: %.1f W\n", g_state.telemetry.max_feedin_actual);
    }
  }

  g_state.mqtt.last_rx_ms = millis();
  g_state.mqtt.data_valid = true;
  g_state.mqtt.live = true;

  Serial.printf("[MQTT] RX %s = %.2f\n", topic, value);
}

void mqttSetup() {
  g_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  g_mqtt.setCallback(onMqttMessage);
  g_mqtt.setKeepAlive(10);
  g_mqtt.setSocketTimeout(2);
}

bool mqttPublishFloatValue(const char* topic, float value, uint8_t decimals) {
  if (!g_mqtt.connected()) return false;

  StaticJsonDocument<64> doc;
  doc["value"] = value;

  char payload[64];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  bool ok = g_mqtt.publish(topic, (const uint8_t*)payload, len, false);

  Serial.printf("[MQTT] TX %s = %.*f -> %s\n", topic, decimals, value, ok ? "OK" : "FAIL");
  return ok;
}

bool trySendGridSetpoint() {
  if (!g_state.write.grid_sp_pending || isnan(g_state.telemetry.grid_sp_requested)) return false;

  bool ok = mqttPublishFloatValue(MQTT_TOPIC_GRID_SP_W, g_state.telemetry.grid_sp_requested, 1);
  if (ok) {
    g_state.write.grid_sp_last_write_ms = millis();
    g_state.write.grid_sp_failed = false;
  } else {
    g_state.write.grid_sp_failed = true;
  }
  return ok;
}

bool trySendMaxFeedIn() {
  if (!g_state.write.max_feedin_pending || isnan(g_state.telemetry.max_feedin_requested)) return false;

  bool ok = mqttPublishFloatValue(MQTT_TOPIC_MAX_FEEDIN_W, g_state.telemetry.max_feedin_requested, 1);
  if (ok) {
    g_state.write.max_feedin_last_write_ms = millis();
    g_state.write.max_feedin_failed = false;
  } else {
    g_state.write.max_feedin_failed = true;
  }
  return ok;
}

void sendVictronKeepAlive() {
  if (!g_mqtt.connected()) return;
  g_mqtt.publish(MQTT_TOPIC_WAKE_SERIAL, "{}", false);
  g_mqtt.publish(MQTT_TOPIC_WAKE_KEEPALIVE, "{\"value\":1}", false);
}

void ensureMqtt() {
  if (!ethAlive()) {
    mqttForceDisconnect();
    return;
  }

  if (g_mqtt.connected()) return;

  Serial.print("[MQTT] Verbinden...");
  if (g_mqtt.connect("esp32c6-battery-bridge")) {
    Serial.println("OK");

    g_mqtt.subscribe(MQTT_TOPIC_SOC, 1);
    g_mqtt.subscribe(MQTT_TOPIC_VBAT, 1);
    g_mqtt.subscribe(MQTT_TOPIC_IBAT, 1);
    g_mqtt.subscribe(MQTT_TOPIC_PBAT, 1);
    g_mqtt.subscribe(MQTT_TOPIC_GRID_SP_R, 1);
    g_mqtt.subscribe(MQTT_TOPIC_MAX_FEEDIN_R, 1);

    sendVictronKeepAlive();
  } else {
    Serial.printf("FAIL rc=%d\n", g_mqtt.state());
    delay(1000);
  }
}

void mqttLoop() {
  g_mqtt.loop();
}

void mqttForceDisconnect() {
  if (g_mqtt.connected()) g_mqtt.disconnect();
  g_state.mqtt.live = false;
}

bool mqttIsConnected() {
  return g_mqtt.connected();
}

void handlePendingWrites() {
  uint32_t now = millis();

  if (g_state.write.grid_sp_pending) {
    if (!g_mqtt.connected()) {
      g_state.write.grid_sp_failed = true;
    } else if (g_state.write.grid_sp_last_write_ms == 0 ||
               now - g_state.write.grid_sp_last_write_ms >= WRITE_RETRY_INTERVAL_MS) {
      trySendGridSetpoint();
    }

    if (g_state.write.grid_sp_last_write_ms != 0 &&
        now - g_state.write.grid_sp_last_write_ms > WRITE_CONFIRM_TIMEOUT_MS) {
      g_state.write.grid_sp_failed = true;
    }
  }

  if (g_state.write.max_feedin_pending) {
    if (!g_mqtt.connected()) {
      g_state.write.max_feedin_failed = true;
    } else if (g_state.write.max_feedin_last_write_ms == 0 ||
               now - g_state.write.max_feedin_last_write_ms >= WRITE_RETRY_INTERVAL_MS) {
      trySendMaxFeedIn();
    }

    if (g_state.write.max_feedin_last_write_ms != 0 &&
        now - g_state.write.max_feedin_last_write_ms > WRITE_CONFIRM_TIMEOUT_MS) {
      g_state.write.max_feedin_failed = true;
    }
  }
}