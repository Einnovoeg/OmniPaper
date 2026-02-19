#include <Arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_IT8951.hpp>

namespace {
constexpr uint32_t kBusyTimeoutMs = 8000;

void printStep(const char* step, bool pass, const char* detail = nullptr) {
  if (detail && detail[0] != '\0') {
    Serial.printf("[EPD_DIAG] %-18s : %s (%s)\n", step, pass ? "PASS" : "FAIL", detail);
  } else {
    Serial.printf("[EPD_DIAG] %-18s : %s\n", step, pass ? "PASS" : "FAIL");
  }
}

bool waitDisplayReady(const char* phase) {
  const uint32_t start = millis();
  while (M5.Display.displayBusy()) {
    if (millis() - start >= kBusyTimeoutMs) {
      Serial.printf("[EPD_DIAG] %s busy timeout after %lu ms\n", phase, static_cast<unsigned long>(kBusyTimeoutMs));
      return false;
    }
    delay(1);
  }
  return true;
}

bool probeIt8951Vcom(uint16_t& vcomOut) {
  if (M5.Display.getBoard() != m5gfx::board_t::board_M5Paper) {
    return false;
  }
  auto* panelBase = M5.Display.getPanel();
  if (!panelBase) {
    return false;
  }

  auto* it8951 = static_cast<lgfx::Panel_IT8951*>(panelBase);
  vcomOut = it8951->getVCOM();

  return (vcomOut != 0) && (vcomOut != 0xFFFF);
}

bool runRefreshProbe() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();

  M5.Display.setAutoDisplay(false);
  M5.Display.setEpdMode(lgfx::epd_quality);

  M5.Display.fillScreen(0x000000);
  M5.Display.display(0, 0, w, h);
  const bool blackOk = waitDisplayReady("black refresh");
  printStep("refresh_black", blackOk);

  M5.Display.fillScreen(0xFFFFFF);
  M5.Display.display(0, 0, w, h);
  const bool whiteOk = waitDisplayReady("white refresh");
  printStep("refresh_white", whiteOk);

  M5.Display.setTextColor(0x000000, 0xFFFFFF);
  M5.Display.setTextSize(2);
  M5.Display.drawString("EPD DIAG", 24, 24);
  M5.Display.setTextSize(1);
  M5.Display.drawString("If you can read this, refresh works.", 24, 64);
  M5.Display.drawRect(8, 8, w - 16, h - 16, 0x000000);
  M5.Display.display(0, 0, w, h);
  const bool drawOk = waitDisplayReady("draw refresh");
  printStep("refresh_draw", drawOk);

  return blackOk && whiteOk && drawOk;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialStart = millis();
  while (!Serial && (millis() - serialStart) < 1500) {
    delay(10);
  }

  Serial.println();
  Serial.println("[EPD_DIAG] Starting IT8951 probe");

  // Keep EPD and SD chip-select lines deselected before M5 init.
  pinMode(GPIO_NUM_2, OUTPUT);
  digitalWrite(GPIO_NUM_2, HIGH);
  pinMode(GPIO_NUM_4, OUTPUT);
  digitalWrite(GPIO_NUM_4, HIGH);
  pinMode(GPIO_NUM_15, OUTPUT);
  digitalWrite(GPIO_NUM_15, HIGH);

  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5Paper;
  cfg.serial_baudrate = 115200;
  cfg.clear_display = false;
  M5.begin(cfg);

  const bool boardOk = (M5.getBoard() == m5::board_t::board_M5Paper);
  const bool displayBoardOk = (M5.Display.getBoard() == m5gfx::board_t::board_M5Paper);
  const bool epdOk = M5.Display.isEPD() && (M5.Display.width() > 0) && (M5.Display.height() > 0);

  Serial.printf("[EPD_DIAG] M5 board=%d display_board=%d epd=%d size=%dx%d rot=%d\n", static_cast<int>(M5.getBoard()),
                static_cast<int>(M5.Display.getBoard()), M5.Display.isEPD() ? 1 : 0, M5.Display.width(),
                M5.Display.height(), M5.Display.getRotation());

  printStep("board_m5paper", boardOk);
  printStep("display_board", displayBoardOk);
  printStep("epd_present", epdOk);

  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  }

  const bool busyOk = waitDisplayReady("initial");
  printStep("initial_busy", busyOk);

  uint16_t vcom = 0;
  const bool vcomOk = probeIt8951Vcom(vcom);
  char vcomDetail[32];
  snprintf(vcomDetail, sizeof(vcomDetail), "vcom=%u", static_cast<unsigned int>(vcom));
  printStep("it8951_vcom", vcomOk, vcomDetail);

  const bool refreshOk = epdOk ? runRefreshProbe() : false;
  if (!epdOk) {
    printStep("refresh_probe", false, "skipped (no EPD)");
  }

  const bool finalPass = boardOk && displayBoardOk && epdOk && busyOk && vcomOk && refreshOk;
  printStep("FINAL", finalPass);

  if (finalPass) {
    Serial.println("[EPD_DIAG] RESULT: PASS");
  } else {
    Serial.println("[EPD_DIAG] RESULT: FAIL");
    Serial.println("[EPD_DIAG] If official firmware also fails, hardware fault is likely.");
  }
}

void loop() {
  M5.update();
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    Serial.printf("[EPD_DIAG] heartbeat busy=%d\n", M5.Display.displayBusy() ? 1 : 0);
  }
  delay(10);
}
