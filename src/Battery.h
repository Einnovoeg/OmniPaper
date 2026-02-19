#pragma once

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>

class M5PaperBattery {
 public:
  int readPercentage() const {
    return M5.Power.getBatteryLevel();
  }
};

static M5PaperBattery battery;
#else
#include <BatteryMonitor.h>

#define BAT_GPIO0 0  // Battery voltage

static BatteryMonitor battery(BAT_GPIO0);
#endif
