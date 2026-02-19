#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>

#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "dev"
#endif

namespace {
constexpr const char* kApSsid = "OmniPaper-LilyGo";
constexpr const char* kApPassword = "omnipaper";
constexpr uint32_t kStatusPeriodMs = 5000;

uint32_t lastStatus = 0;

void printStatus() {
  Serial.printf("[OmniPaper][LilyGo] uptime=%lus free_heap=%u SD=%s AP_IP=%s\n", millis() / 1000UL,
                ESP.getFreeHeap(), SD.cardType() == CARD_NONE ? "down" : "ok",
                WiFi.softAPIP().toString().c_str());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("OmniPaper LilyGo-EPD47 build: %s\n", CROSSPOINT_VERSION);

  // Bring up basic services for board validation even before display integration.
  const bool sdOk = SD.begin();
  Serial.printf("SD init: %s\n", sdOk ? "ok" : "failed");

  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(kApSsid, kApPassword);
  Serial.printf("SoftAP: %s (%s)\n", apOk ? "ok" : "failed", kApSsid);

  printStatus();
  lastStatus = millis();
}

void loop() {
  if (millis() - lastStatus >= kStatusPeriodMs) {
    printStatus();
    lastStatus = millis();
  }
  delay(20);
}
