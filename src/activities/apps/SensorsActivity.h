#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include <HalGPIO.h>

class SensorsActivity final : public Activity {
 public:
  enum class Mode { BuiltIn, External };

  SensorsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio, Mode mode,
                  const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct SensorSnapshot {
    float temperatureC = 0.0f;
    float humidity = 0.0f;
    bool hasSht30 = false;

    bool hasImu = false;
    float accelX = 0.0f;
    float accelY = 0.0f;
    float accelZ = 0.0f;
    float gyroX = 0.0f;
    float gyroY = 0.0f;
    float gyroZ = 0.0f;

    int batteryPercent = 0;
    int batteryMv = 0;
    int32_t batteryCurrentMa = 0;
    int vbusMv = 0;
    bool usbCablePresent = false;
    bool charging = false;
    bool rtcAvailable = false;
    bool speakerAvailable = false;
    bool usbSerialOpen = false;
    uint8_t touchCount = 0;
    int touchX = 0;
    int touchY = 0;
    int wifiRssi = 0;
    unsigned long uptimeMs = 0;
  };

  HalGPIO& gpio;
  Mode mode;
  std::function<void()> onExit;
  SensorSnapshot snapshot;
  std::vector<uint8_t> externalI2cDevices;
  bool needsRender = true;
  unsigned long lastUpdate = 0;

  void updateData();
  void scanI2c();
  void render();
  void renderBuiltIn();
#if defined(PLATFORM_M5PAPERS3)
  void renderPaperS3BuiltIn();
#endif
  void renderExternal();
};
