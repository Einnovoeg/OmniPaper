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

  std::tm localTime {};
  if (TimeUtils::getLocalTimeWithOffset(localTime, SETTINGS.timezoneOffsetMinutes)) {
    char timeLine[32];
    char dateLine[32];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d", localTime.tm_year + 1900, localTime.tm_mon + 1,
             localTime.tm_mday);
    renderer.drawText(UI_12_FONT_ID, 40, 60, timeLine);
    renderer.drawText(UI_10_FONT_ID, 40, 82, dateLine);
  } else {
    renderer.drawText(UI_12_FONT_ID, 40, 60, "Time not synced");
  }

  const int battery = gpio.getBatteryPercentage();
  char batteryLine[32];
  snprintf(batteryLine, sizeof(batteryLine), "Battery: %d%%", battery);
  renderer.drawText(UI_10_FONT_ID, 40, 110, batteryLine);

  if (WiFi.status() == WL_CONNECTED) {
    const String ssid = WiFi.SSID();
    String ip = WiFi.localIP().toString();
    char wifiLine[64];
    snprintf(wifiLine, sizeof(wifiLine), "WiFi: %s", ssid.c_str());
    renderer.drawText(UI_10_FONT_ID, 40, 132, wifiLine);
    renderer.drawText(UI_10_FONT_ID, 40, 150, ip.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, 40, 132, "WiFi: disconnected");
  }

  int y = 190;
  if (fetching) {
    renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: fetching...");
  } else if (!snapshot.valid) {
    if (!snapshot.location.empty()) {
      renderer.drawText(UI_10_FONT_ID, 40, y, snapshot.location.c_str());
    } else {
      renderer.drawText(UI_10_FONT_ID, 40, y, "Weather: not loaded");
    }
  } else {
    char weatherLine[64];
    snprintf(weatherLine, sizeof(weatherLine), "Weather: %.1f C", snapshot.temperatureC);
    renderer.drawText(UI_10_FONT_ID, 40, y, weatherLine);
    y += 18;
    std::string desc = weatherDescription(snapshot.weatherCode);
    renderer.drawText(UI_10_FONT_ID, 40, y, desc.c_str());
  }

  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - 24, "Confirm: Refresh   Back: Menu");
  renderer.displayBuffer();
}
