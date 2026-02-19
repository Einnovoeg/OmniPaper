#pragma once

#include <functional>
#include <string>

#include "../Activity.h"

class HalGPIO;

class DashboardActivity final : public Activity {
 public:
  DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                    const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct WeatherSnapshot {
    bool valid = false;
    std::string location;
    float temperatureC = 0.0f;
    int weatherCode = 0;
  };

  struct LocationInfo {
    bool valid = false;
    float latitude = 0.0f;
    float longitude = 0.0f;
    std::string city;
  };

  HalGPIO& gpio;
  std::function<void()> onExit;
  WeatherSnapshot snapshot;
  bool needsRender = true;
  bool fetching = false;

  void updateWeather();
  bool fetchLocation(LocationInfo& out);
  bool fetchWeather(const LocationInfo& loc, WeatherSnapshot& out);
  std::string weatherDescription(int code) const;
  void render();
};
