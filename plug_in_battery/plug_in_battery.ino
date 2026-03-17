#include "config.h"
#include "state_store.h"
#include "eth_manager.h"
#include "mqtt_bridge.h"
#include "zigbee_bridge.h"

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n[BOOT] Test 5 modular start");

  connectLAN();
  mqttSetup();
  connectZigbee();

  Serial.println("[Setup] Bereit: LAN + MQTT + Zigbee (modular)");
}

void loop() {
  ensureMqtt();
  mqttLoop();

  handleZigbeeFactoryResetButton();

  static uint32_t tKeep = 0;
  if (millis() - tKeep > KEEPALIVE_INTERVAL_MS) {
    tKeep = millis();
    sendVictronKeepAlive();
  }

  if (g_state.mqtt.data_valid && (millis() - g_state.mqtt.last_rx_ms > MQTT_DATA_TIMEOUT_MS)) {
    g_state.mqtt.live = false;
  }

  handlePendingWrites();

  static uint32_t tZb = 0;
  if (millis() - tZb > 5000) {
    tZb = millis();
    reportZigbeeValues(false);
  }

  static uint32_t tZbForce = 0;
  if (millis() - tZbForce > 30000) {
    tZbForce = millis();
    reportZigbeeValues(true);
  }

  static uint32_t tInfo = 0;
  if (millis() - tInfo > 2000) {
    tInfo = millis();
    Serial.printf(
      "[INFO]\n"
      "  SoC              = %.2f %%\n"
      "  U_bat            = %.2f V\n"
      "  I_bat            = %.2f A\n"
      "  P_bat            = %.2f W\n"
      "  GridSP actual    = %.2f W\n"
      "  GridSP req       = %.2f W\n"
      "  GridSP state     = %s\n"
      "  MaxFeedIn actual = %.2f W\n"
      "  MaxFeedIn req    = %.2f W\n"
      "  MaxFeedIn state  = %s\n"
      "  MQTT=%s  ZB=%s\n"
      "  MQTT values state: %s\n\n",
      g_state.telemetry.soc,
      g_state.telemetry.vbat,
      g_state.telemetry.ibat,
      g_state.telemetry.pbat,
      g_state.telemetry.grid_sp_actual,
      g_state.telemetry.grid_sp_requested,
      writeStateText(g_state.write.grid_sp_pending, g_state.write.grid_sp_failed),
      g_state.telemetry.max_feedin_actual,
      g_state.telemetry.max_feedin_requested,
      writeStateText(g_state.write.max_feedin_pending, g_state.write.max_feedin_failed),
      (mqttIsConnected() && g_state.mqtt.live) ? "OK" : "X",
      zigbeeIsConnected() ? "OK" : "X",
      mqttValueStateText()
    );
  }

  delay(1);
}