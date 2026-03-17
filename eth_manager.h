#pragma once
#include "config.h"

extern volatile bool g_eth_connected;

bool ethAlive();
void connectLAN();