#include "zigbee_bridge.h"
#include "state_store.h"
#include "mqtt_bridge.h"

uint8_t button = BOOT_PIN;

ZigbeeAnalog zbSoc(ZB_EP_SOC);
ZigbeeAnalog zbVbat(ZB_EP_VBAT);
ZigbeeAnalog zbIbat(ZB_EP_IBAT);
ZigbeeAnalog zbPbat(ZB_EP_PBAT);
ZigbeeAnalog zbGridSetpoint(ZB_EP_GRID_SP);
ZigbeeAnalog zbMaxFeedIn(ZB_EP_MAX_FEEDIN);

static void onGridSetpointWrite(float value) {
  uint32_t now = millis();

  if (now - g_state.write.grid_sp_last_req_ms < WRITE_DEBOUNCE_MS) {
    Serial.printf("[ZIGBEE] Grid setpoint write ignored (debounce): %.1f W\n", value);
    return;
  }
  g_state.write.grid_sp_last_req_ms = now;

  float clamped = clampFloat(value, GRID_SP_MIN, GRID_SP_MAX);

  if (!floatChanged(g_state.telemetry.grid_sp_requested, clamped, 0.5f) &&
      !floatChanged(g_state.telemetry.grid_sp_actual, clamped, 0.5f) &&
      !g_state.write.grid_sp_pending) {
    Serial.printf("[ZIGBEE] Grid setpoint unchanged: %.1f W\n", clamped);
    return;
  }

  g_state.telemetry.grid_sp_requested = clamped;
  g_state.write.grid_sp_pending = true;
  g_state.write.grid_sp_failed = false;

  Serial.printf("[ZIGBEE] Grid setpoint requested = %.1f W (raw=%.1f)\n", clamped, value);

  if (!trySendGridSetpoint()) {
    Serial.println("[ZIGBEE] Grid setpoint send deferred (MQTT not ready or publish failed)");
  }
}

static void onMaxFeedInWrite(float value) {
  uint32_t now = millis();

  if (now - g_state.write.max_feedin_last_req_ms < WRITE_DEBOUNCE_MS) {
    Serial.printf("[ZIGBEE] Max feed-in write ignored (debounce): %.1f W\n", value);
    return;
  }
  g_state.write.max_feedin_last_req_ms = now;

  float clamped = clampFloat(value, MAX_FEEDIN_MIN, MAX_FEEDIN_MAX);

  if (!floatChanged(g_state.telemetry.max_feedin_requested, clamped, 0.5f) &&
      !floatChanged(g_state.telemetry.max_feedin_actual, clamped, 0.5f) &&
      !g_state.write.max_feedin_pending) {
    Serial.printf("[ZIGBEE] Max feed-in unchanged: %.1f W\n", clamped);
    return;
  }

  g_state.telemetry.max_feedin_requested = clamped;
  g_state.write.max_feedin_pending = true;
  g_state.write.max_feedin_failed = false;

  Serial.printf("[ZIGBEE] Max feed-in requested = %.1f W (raw=%.1f)\n", clamped, value);

  if (!trySendMaxFeedIn()) {
    Serial.println("[ZIGBEE] Max feed-in send deferred (MQTT not ready or publish failed)");
  }
}

static void setupZigbeeEndpoints() {
  zbSoc.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");
  zbVbat.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");
  zbIbat.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");
  zbPbat.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");
  zbGridSetpoint.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");
  zbMaxFeedIn.setManufacturerAndModel("MarkoLab", "ESP32C6_Battery_Bridge");

  zbSoc.addAnalogInput();
  zbSoc.setAnalogInputDescription("Battery SoC");
  zbSoc.setAnalogInputResolution(0.1f);
  zbSoc.setAnalogInputMinMax(0.0f, 100.0f);

  zbVbat.addAnalogInput();
  zbVbat.setAnalogInputDescription("Battery Voltage");
  zbVbat.setAnalogInputResolution(0.01f);
  zbVbat.setAnalogInputMinMax(0.0f, 100.0f);

  zbIbat.addAnalogInput();
  zbIbat.setAnalogInputDescription("Battery Current");
  zbIbat.setAnalogInputResolution(0.01f);
  zbIbat.setAnalogInputMinMax(-300.0f, 300.0f);

  zbPbat.addAnalogInput();
  zbPbat.setAnalogInputDescription("Battery Power");
  zbPbat.setAnalogInputResolution(1.0f);
  zbPbat.setAnalogInputMinMax(-20000.0f, 20000.0f);

  zbGridSetpoint.addAnalogOutput();
  zbGridSetpoint.setAnalogOutputDescription("Grid Setpoint");
  zbGridSetpoint.setAnalogOutputResolution(1.0f);
  zbGridSetpoint.setAnalogOutputMinMax(GRID_SP_MIN, GRID_SP_MAX);
  zbGridSetpoint.onAnalogOutputChange(onGridSetpointWrite);

  zbMaxFeedIn.addAnalogOutput();
  zbMaxFeedIn.setAnalogOutputDescription("Max Grid Feed-In Limit");
  zbMaxFeedIn.setAnalogOutputResolution(1.0f);
  zbMaxFeedIn.setAnalogOutputMinMax(MAX_FEEDIN_MIN, MAX_FEEDIN_MAX);
  zbMaxFeedIn.onAnalogOutputChange(onMaxFeedInWrite);

  Zigbee.addEndpoint(&zbSoc);
  Zigbee.addEndpoint(&zbVbat);
  Zigbee.addEndpoint(&zbIbat);
  Zigbee.addEndpoint(&zbPbat);
  Zigbee.addEndpoint(&zbGridSetpoint);
  Zigbee.addEndpoint(&zbMaxFeedIn);
}

void connectZigbee() {
  pinMode(button, INPUT_PULLUP);

  setupZigbeeEndpoints();

  Serial.println("[ZIGBEE] Starting...");
  if (!Zigbee.begin()) {
    Serial.println("[ZIGBEE] Start FAILED -> reboot");
    delay(1000);
    ESP.restart();
  }

  Serial.println("[ZIGBEE] Waiting for network join...");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  Serial.println("[ZIGBEE] Joined network successfully");

  zbSoc.setAnalogInputReporting(1, 60, 0.2f);
  zbVbat.setAnalogInputReporting(1, 60, 0.05f);
  zbIbat.setAnalogInputReporting(1, 60, 0.10f);
  zbPbat.setAnalogInputReporting(1, 60, 5.0f);

  if (!isnan(g_state.telemetry.soc)) {
    zbSoc.setAnalogInput(g_state.telemetry.soc);
    zbSoc.reportAnalogInput();
  }
  if (!isnan(g_state.telemetry.vbat)) {
    zbVbat.setAnalogInput(g_state.telemetry.vbat);
    zbVbat.reportAnalogInput();
  }
  if (!isnan(g_state.telemetry.ibat)) {
    zbIbat.setAnalogInput(g_state.telemetry.ibat);
    zbIbat.reportAnalogInput();
  }
  if (!isnan(g_state.telemetry.pbat)) {
    zbPbat.setAnalogInput(g_state.telemetry.pbat);
    zbPbat.reportAnalogInput();
  }
  if (!isnan(g_state.telemetry.grid_sp_actual)) {
    zbGridSetpoint.setAnalogOutput(g_state.telemetry.grid_sp_actual);
    zbGridSetpoint.reportAnalogOutput();
  }
  if (!isnan(g_state.telemetry.max_feedin_actual)) {
    zbMaxFeedIn.setAnalogOutput(g_state.telemetry.max_feedin_actual);
    zbMaxFeedIn.reportAnalogOutput();
  }
}

void reportZigbeeValues(bool force) {
  if (!Zigbee.connected()) return;

  bool any = false;

  if (!isnan(g_state.telemetry.soc) && (force || floatChanged(g_state.telemetry.soc, g_state.zb_cache.soc, 0.1f))) {
    zbSoc.setAnalogInput(g_state.telemetry.soc);
    zbSoc.reportAnalogInput();
    g_state.zb_cache.soc = g_state.telemetry.soc;
    any = true;
  }

  if (!isnan(g_state.telemetry.vbat) && (force || floatChanged(g_state.telemetry.vbat, g_state.zb_cache.vbat, 0.01f))) {
    zbVbat.setAnalogInput(g_state.telemetry.vbat);
    zbVbat.reportAnalogInput();
    g_state.zb_cache.vbat = g_state.telemetry.vbat;
    any = true;
  }

  if (!isnan(g_state.telemetry.ibat) && (force || floatChanged(g_state.telemetry.ibat, g_state.zb_cache.ibat, 0.01f))) {
    zbIbat.setAnalogInput(g_state.telemetry.ibat);
    zbIbat.reportAnalogInput();
    g_state.zb_cache.ibat = g_state.telemetry.ibat;
    any = true;
  }

  if (!isnan(g_state.telemetry.pbat) && (force || floatChanged(g_state.telemetry.pbat, g_state.zb_cache.pbat, 1.0f))) {
    zbPbat.setAnalogInput(g_state.telemetry.pbat);
    zbPbat.reportAnalogInput();
    g_state.zb_cache.pbat = g_state.telemetry.pbat;
    any = true;
  }

  if (!isnan(g_state.telemetry.grid_sp_actual) && (force || floatChanged(g_state.telemetry.grid_sp_actual, g_state.zb_cache.grid_sp, 1.0f))) {
    zbGridSetpoint.setAnalogOutput(g_state.telemetry.grid_sp_actual);
    zbGridSetpoint.reportAnalogOutput();
    g_state.zb_cache.grid_sp = g_state.telemetry.grid_sp_actual;
    any = true;
  }

  if (!isnan(g_state.telemetry.max_feedin_actual) && (force || floatChanged(g_state.telemetry.max_feedin_actual, g_state.zb_cache.max_feedin, 1.0f))) {
    zbMaxFeedIn.setAnalogOutput(g_state.telemetry.max_feedin_actual);
    zbMaxFeedIn.reportAnalogOutput();
    g_state.zb_cache.max_feedin = g_state.telemetry.max_feedin_actual;
    any = true;
  }

  if (any) {
    Serial.println("[ZIGBEE] Values reported");
  }
}

void handleZigbeeFactoryResetButton() {
  if (digitalRead(button) == LOW) {
    delay(100);
    uint32_t t0 = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if (millis() - t0 > 3000) {
        Serial.println("[ZIGBEE] Factory reset + reboot");
        delay(500);
        Zigbee.factoryReset();
      }
    }
  }
}

bool zigbeeIsConnected() {
  return Zigbee.connected();
}