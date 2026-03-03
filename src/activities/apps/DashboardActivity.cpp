#include "DashboardActivity.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <string>

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

DashboardActivity::DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, HalGPIO& gpio,
                                     const std::function<void()>& onExit)
    : Activity("Dashboard", renderer, mappedInput), gpio(gpio), onExit(onExit) {}

void DashboardActivity::onEnter() {
  Activity::onEnter();
  needsRender = true;
  snapshot.valid = false;
  fetching = false;
}

void DashboardActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onExit) {
      onExit();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    updateWeather();
  }

  if (needsRender) {
    render();
    needsRender = false;
  }
}

void DashboardActivity::updateWeather() {
  fetching = true;
  needsRender = true;

  if (WiFi.status() != WL_CONNECTED) {
    snapshot.valid = false;
    snapshot.location = "WiFi disconnected";
    fetching = false;
    return;
  }

  LocationInfo loc;
  if (!fetchLocation(loc)) {
    snapshot.valid = false;
    snapshot.location = "Location lookup failed";
    fetching = false;
    return;
  }

  if (!fetchWeather(loc, snapshot)) {
    snapshot.valid = false;
    snapshot.location = loc.city;
    fetching = false;
    return;
  }

  snapshot.location = loc.city;
  snapshot.valid = true;
  fetching = false;
}

bool DashboardActivity::fetchLocation(LocationInfo& out) {
  HTTPClient http;
  if (!http.begin("http://ip-api.com/json")) {
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const auto err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  const char* status = doc["status"] | "";
  if (strcmp(status, "success") != 0) {
    return false;
  }

  out.latitude = doc["lat"] | 0.0;
  out.longitude = doc["lon"] | 0.0;
  out.city = std::string(doc["city"] | "Unknown");
  out.valid = true;
  return true;
}

bool DashboardActivity::fetchWeather(const LocationInfo& loc, WeatherSnapshot& out) {
  WiFiClientSecure client;
  client.setInsecure();

  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true&timezone=auto",
           loc.latitude, loc.longitude);

  HTTPClient http;
  if (!http.begin(client, url)) {
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const auto err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  JsonObject current = doc["current_weather"];
  if (current.isNull()) {
    return false;
  }

  out.temperatureC = current["temperature"] | 0.0f;
  out.weatherCode = current["weathercode"] | 0;
  return true;
}

std::string DashboardActivity::weatherDescription(const int code) const {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 80: return "Rain showers";
    case 95: return "Thunder";
    default: return "Unknown";
  }
}

void DashboardActivity::render() {
  renderer.clearScreen();
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);

  renderer.drawCenteredText(UI_12_FONT_ID, 16, "Dashboard");

  int y = 58;
  std::tm localTime {};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char timeLine[32];
    char dateLine[32];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d", localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday);
    renderer.drawText(UI_12_FONT_ID, 40, y, timeLine);
    y += 22;
    renderer.drawText(UI_10_FONT_ID, 40, y, dateLine);
  } else {
    renderer.drawText(UI_12_FONT_ID, 40, y, "Time not synced");
    y += 22;
  }

  y += 8;
  char line[96];
  snprintf(line, sizeof(line), "Battery: %d%%", gpio.getBatteryPercentage());
  renderer.drawText(UI_10_FONT_ID, 40, y, line);
  y += 18;

#ifdef PLATFORM_M5PAPER
  const int16_t batteryMv = M5.Power.getBatteryVoltage();
  const bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  snprintf(line, sizeof(line), "Battery mV: %d   Charging: %s", batteryMv, charging ? "Yes" : "No");
  renderer.drawText(SMALL_FONT_ID, 40, y, line);
  y += 16;
#endif

  if (WiFi.status() == WL_CONNECTED) {
    const String ssid = WiFi.SSID();
    const String ip = WiFi.localIP().toString();
    snprintf(line, sizeof(line), "WiFi: %s", ssid.c_str());
    renderer.drawText(SMALL_FONT_ID, 40, y, line);
    y += 16;
    renderer.drawText(SMALL_FONT_ID, 40, y, ip.c_str());
    y += 16;
  } else {
    renderer.drawText(SMALL_FONT_ID, 40, y, "WiFi: disconnected");
    y += 16;
  }

#if defined(PLATFORM_M5PAPERS3)
  renderer.drawText(SMALL_FONT_ID, 40, y, static_cast<bool>(Serial) ? "USB OTG/CDC: Connected" : "USB OTG/CDC: Idle");
  y += 16;
#endif

  y += 6;
  if (fetching) {
    renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: fetching...");
  } else if (!snapshot.valid) {
    if (!snapshot.location.empty()) {
      renderer.drawText(UI_10_FONT_ID, 40, y, snapshot.location.c_str());
    } else {
      renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: not loaded");
    }
  } else {
    snprintf(line, sizeof(line), "Weather: %.1f C", snapshot.temperatureC);
    renderer.drawText(UI_10_FONT_ID, 40, y, line);
    y += 18;
    const std::string desc = weatherDescription(snapshot.weatherCode);
    renderer.drawText(UI_10_FONT_ID, 40, y, desc.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Refresh   Back: Menu");
  renderer.displayBuffer();
}
