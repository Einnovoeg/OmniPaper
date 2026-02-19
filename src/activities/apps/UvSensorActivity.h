#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Wire.h>

#include <AS7331.h>

#include "../ActivityWithSubactivity.h"

class UvSensorActivity final : public ActivityWithSubactivity {
 public:
  UvSensorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  std::function<void()> onExitCb;
  TwoWire uvWire;
  std::unique_ptr<AS7331> sensor;
  bool sensorReady = false;
  bool autoSample = true;
  uint8_t i2cAddress = 0x74;
  unsigned long lastSampleMs = 0;
  unsigned long sampleIntervalMs = 2000;
  float uva = 0.0f;
  float uvb = 0.0f;
  float uvc = 0.0f;
  float tempC = 0.0f;
  std::string statusMessage;
  bool needsRender = true;

  void initSensor();
  void takeSample();
  bool logSample();
  void promptAddress();
  bool parseHexAddress(const std::string& text, uint8_t& out) const;
  void render();
};
