#pragma once
#include "config.h"

extern ZigbeeAnalog zbSoc;
extern ZigbeeAnalog zbVbat;
extern ZigbeeAnalog zbIbat;
extern ZigbeeAnalog zbPbat;
extern ZigbeeAnalog zbGridSetpoint;
extern ZigbeeAnalog zbMaxFeedIn;

void connectZigbee();
void reportZigbeeValues(bool force = false);
void handleZigbeeFactoryResetButton();
bool zigbeeIsConnected();