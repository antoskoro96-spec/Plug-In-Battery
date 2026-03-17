#pragma once
#include "config.h"

struct TelemetryState {
  float soc = NAN;
  float vbat = NAN;
  float ibat = NAN;
  float pbat = NAN;

  float grid_sp_actual = NAN;
  float max_feedin_actual = NAN;

  float grid_sp_requested = NAN;
  float max_feedin_requested = NAN;
};

struct WriteState {
  bool grid_sp_pending = false;
  bool max_feedin_pending = false;

  bool grid_sp_failed = false;
  bool max_feedin_failed = false;

  uint32_t grid_sp_last_write_ms = 0;
  uint32_t max_feedin_last_write_ms = 0;

  uint32_t grid_sp_last_req_ms = 0;
  uint32_t max_feedin_last_req_ms = 0;
};

struct MqttState {
  uint32_t last_rx_ms = 0;
  bool data_valid = false;
  bool live = false;
};

struct ZigbeeCacheState {
  float soc = NAN;
  float vbat = NAN;
  float ibat = NAN;
  float pbat = NAN;
  float grid_sp = NAN;
  float max_feedin = NAN;
};

struct AppState {
  TelemetryState telemetry;
  WriteState write;
  MqttState mqtt;
  ZigbeeCacheState zb_cache;
};

extern AppState g_state;

bool floatChanged(float a, float b, float eps);
bool floatEqual(float a, float b, float eps);
float clampFloat(float x, float mn, float mx);
const char* mqttValueStateText();
const char* writeStateText(bool pending, bool failed);