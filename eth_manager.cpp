#include "eth_manager.h"
#include "mqtt_bridge.h"

volatile bool g_eth_connected = false;

static void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
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
      Serial.print("IP: ");
      Serial.println(ETH.localIP());
      g_eth_connected = true;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[EV] ETH Disconnected");
      g_eth_connected = false;
      mqttForceDisconnect();
      break;

    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("[EV] ETH Lost IP");
      g_eth_connected = false;
      mqttForceDisconnect();
      break;

    default:
      break;
  }
}

bool ethAlive() {
  return g_eth_connected && ETH.linkUp();
}

void connectLAN() {
  Network.begin();
  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  SPI.setFrequency(12000000);
  Serial.println("[BOOT] SPI started at 12 MHz");

  bool ok = ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);
  Serial.printf("[BOOT] ETH.begin ok=%d\n", ok);

  ETH.config(LOCAL_IP, GATEWAY, SUBNET, DNS1, DNS2);

  Serial.print("[ETH] Warten auf IP");
  while (!g_eth_connected) {
    Serial.print(".");
    delay(300);
  }
  Serial.printf("\n[ETH] OK, IP: %s\n", ETH.localIP().toString().c_str());
}