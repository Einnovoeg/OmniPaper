#pragma once

#include <functional>
#include <string>

#include "../Activity.h"

class WeatherActivity final : public Activity {
 public:
  WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::function<void()>& onExit);

  void onEnter() override;
  void loop() override;

 private:
  struct WeatherSnapshot {
    bool valid = false;
    std::string location;
    float temperatureC = 0.0f;
    float windSpeed = 0.0f;
    int weatherCode = 0;
    unsigned long fetchedAt = 0;
  };

  struct LocationInfo {
    bool valid = false;
    float latitude = 0.0f;
    float longitude = 0.0f;
    std::string city;
  };

  std::function<void()> onExit;
  WeatherSnapshot snapshot;
  bool needsRender = true;
  bool fetching = false;
  unsigned long lastUpdate = 0;

  void render();
  void updateWeather();
  bool fetchLocation(LocationInfo& out);
  bool fetchWeather(const LocationInfo& loc, WeatherSnapshot& out);
  std::string weatherDescription(int code) const;
};
