#pragma once
#include "config.h"

extern NetworkClient g_netClient;
extern PubSubClient g_mqtt;

void mqttSetup();
void ensureMqtt();
void mqttLoop();
void sendVictronKeepAlive();
void handlePendingWrites();
bool mqttPublishFloatValue(const char* topic, float value, uint8_t decimals = 1);
bool trySendGridSetpoint();
bool trySendMaxFeedIn();
void mqttForceDisconnect();
bool mqttIsConnected();