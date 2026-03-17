#pragma once

#include <Arduino.h>
#include <Network.h>
#include <ETH.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#ifndef ZIGBEE_MODE_ED
#error "Bitte in Tools -> Zigbee mode auf Zigbee End Device stellen"
#endif

#include "Zigbee.h"

// ==== LAN (W5500) ====
#define ETH_PHY_TYPE   ETH_PHY_W5500
#define ETH_PHY_ADDR   1
#define ETH_PHY_CS     5
#define ETH_PHY_IRQ    4
#define ETH_PHY_RST   -1

#define ETH_SPI_SCK    6
#define ETH_SPI_MISO   2
#define ETH_SPI_MOSI   7

// ==== STATIC IP (ESP32) ====
static const IPAddress LOCAL_IP (192, 168, 0, 50);
static const IPAddress GATEWAY  (192, 168, 0, 1);
static const IPAddress SUBNET   (255, 255, 255, 0);
static const IPAddress DNS1     (192, 168, 0, 1);
static const IPAddress DNS2     (8, 8, 8, 8);

// ==== MQTT ====
#define MQTT_HOST   "192.168.0.106"
#define MQTT_PORT   1883

#define MQTT_TOPIC_VBAT            "N/c0619aba3d93/system/0/Dc/Battery/Voltage"
#define MQTT_TOPIC_IBAT            "N/c0619aba3d93/system/0/Dc/Battery/Current"
#define MQTT_TOPIC_PBAT            "N/c0619aba3d93/system/0/Dc/Battery/Power"
#define MQTT_TOPIC_SOC             "N/c0619aba3d93/system/0/Dc/Battery/Soc"

#define MQTT_TOPIC_GRID_SP_R       "N/c0619aba3d93/settings/0/Settings/CGwacs/AcPowerSetPoint"
#define MQTT_TOPIC_MAX_FEEDIN_R    "N/c0619aba3d93/settings/0/Settings/CGwacs/MaxFeedInPower"

#define MQTT_TOPIC_GRID_SP_W       "W/c0619aba3d93/settings/0/Settings/CGwacs/AcPowerSetPoint"
#define MQTT_TOPIC_MAX_FEEDIN_W    "W/c0619aba3d93/settings/0/Settings/CGwacs/MaxFeedInPower"

#define MQTT_TOPIC_WAKE_SERIAL     "R/c0619aba3d93/system/0/Serial"
#define MQTT_TOPIC_WAKE_KEEPALIVE  "R/c0619aba3d93/keepalive"

#define KEEPALIVE_INTERVAL_MS      30000UL
#define MQTT_DATA_TIMEOUT_MS       70000UL

#define WRITE_RETRY_INTERVAL_MS    3000UL
#define WRITE_CONFIRM_TIMEOUT_MS   10000UL
#define WRITE_DEBOUNCE_MS           500UL

#define GRID_SP_MIN               (-100.0f)
#define GRID_SP_MAX               ( 300.0f)

#define MAX_FEEDIN_MIN            (   0.0f)
#define MAX_FEEDIN_MAX            ( 800.0f)

// ==== Zigbee Endpoints ====
#define ZB_EP_SOC         10
#define ZB_EP_VBAT        11
#define ZB_EP_IBAT        12
#define ZB_EP_PBAT        13
#define ZB_EP_GRID_SP     14
#define ZB_EP_MAX_FEEDIN  15