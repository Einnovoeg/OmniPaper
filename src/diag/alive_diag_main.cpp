#include <Arduino.h>
#include <WiFi.h>

namespace {
constexpr const char* kApSsid = "M5Paper-AliveDiag";
constexpr const char* kApPass = "m5paperdiag";
constexpr uint8_t kApChannel = 1;

void printPinSnapshot() {
  // Useful M5Paper lines to observe (buttons + EPD busy + strapping pins).
  const int g0 = digitalRead(GPIO_NUM_0);
  const int g2 = digitalRead(GPIO_NUM_2);
  const int g12 = digitalRead(GPIO_NUM_12);
  const int g15 = digitalRead(GPIO_NUM_15);
  const int g27 = digitalRead(GPIO_NUM_27);
  const int g37 = digitalRead(GPIO_NUM_37);
  const int g38 = digitalRead(GPIO_NUM_38);
  const int g39 = digitalRead(GPIO_NUM_39);
  Serial.printf(
      "[ALIVE_DIAG] pins g0=%d g2=%d g12=%d g15=%d g27=%d g37=%d g38=%d g39=%d\n",
      g0, g2, g12, g15, g27, g37, g38, g39);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("[ALIVE_DIAG] boot");

  pinMode(GPIO_NUM_0, INPUT_PULLUP);
  pinMode(GPIO_NUM_2, INPUT_PULLUP);
  pinMode(GPIO_NUM_12, INPUT_PULLUP);
  pinMode(GPIO_NUM_15, INPUT_PULLUP);
  pinMode(GPIO_NUM_27, INPUT_PULLUP);
  pinMode(GPIO_NUM_37, INPUT_PULLUP);
  pinMode(GPIO_NUM_38, INPUT_PULLUP);
  pinMode(GPIO_NUM_39, INPUT_PULLUP);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(kApSsid, kApPass, kApChannel, false, 1);
  Serial.printf("[ALIVE_DIAG] wifi_ap=%d ssid=%s ip=%s\n", apOk ? 1 : 0, kApSsid,
                WiFi.softAPIP().toString().c_str());

  printPinSnapshot();
}

void loop() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs >= 1000) {
    lastMs = millis();
    Serial.printf("[ALIVE_DIAG] heartbeat ms=%lu free_heap=%u\n", static_cast<unsigned long>(millis()),
                  static_cast<unsigned int>(ESP.getFreeHeap()));
    printPinSnapshot();
  }
  delay(10);
}
