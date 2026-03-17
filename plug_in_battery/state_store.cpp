#include "state_store.h"
#include <math.h>

AppState g_state;

bool floatChanged(float a, float b, float eps) {
  if (isnan(a) && isnan(b)) return false;
  if (isnan(a) || isnan(b)) return true;
  return fabs(a - b) > eps;
}

bool floatEqual(float a, float b, float eps) {
  if (isnan(a) || isnan(b)) return false;
  return fabs(a - b) <= eps;
}

float clampFloat(float x, float mn, float mx) {
  if (isnan(x)) return x;
  if (x < mn) return mn;
  if (x > mx) return mx;
  return x;
}

const char* mqttValueStateText() {
  if (!g_state.mqtt.data_valid) return "no values yet";
  if (g_state.mqtt.live) return "live values";
  return "last monitored values";
}

const char* writeStateText(bool pending, bool failed) {
  if (pending) return "PENDING";
  if (failed)  return "FAILED";
  return "IDLE/CONFIRMED";
}