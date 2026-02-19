#include <HalGPIO.h>
#include <SPI.h>
#include <esp_sleep.h>

#ifdef PLATFORM_M5PAPER
namespace {
constexpr uint32_t kDisplayWaitTimeoutMs = 4000;

void waitDisplayWithTimeout() {
  const uint32_t start = millis();
  while (M5.Display.displayBusy()) {
    if (millis() - start >= kDisplayWaitTimeoutMs) {
      if (Serial) {
        Serial.println("[M5Paper] displayBusy timeout");
      }
      break;
    }
    delay(1);
  }
}

bool ensureM5PaperDisplay() {
  if (M5.Display.isEPD() && M5.Display.width() > 0 && M5.Display.height() > 0) {
    return true;
  }

  // Retry M5GFX autodetect/init once more after M5.begin().
  if (Serial) {
    Serial.printf("[M5Paper] Display not ready after M5.begin() (board=%d, w=%d, h=%d). Retrying init...\n",
                  static_cast<int>(M5.Display.getBoard()), M5.Display.width(), M5.Display.height());
  }
  const bool ok = M5.Display.init();
  if (Serial) {
    Serial.printf("[M5Paper] Retry init result=%d, board=%d, w=%d, h=%d, epd=%d\n", ok ? 1 : 0,
                  static_cast<int>(M5.Display.getBoard()), M5.Display.width(), M5.Display.height(),
                  M5.Display.isEPD() ? 1 : 0);
  }
  return ok && M5.Display.isEPD() && M5.Display.width() > 0 && M5.Display.height() > 0;
}

void configureM5PaperDisplay() {
  if (!M5.Display.isEPD()) {
    return;
  }
  // The project assumes a 960x540 logical canvas.
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  }
  M5.Display.setEpdMode(lgfx::epd_quality);
  M5.Display.setAutoDisplay(false);
}

void forceBootRefresh() {
  if (!M5.Display.isEPD()) {
    return;
  }
  configureM5PaperDisplay();
  // M5EPD-style clear sequence to reduce ghosting and force a visible hardware refresh.
  M5.Display.fillScreen(0x000000);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  waitDisplayWithTimeout();
  M5.Display.fillScreen(0xFFFFFF);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  waitDisplayWithTimeout();
  M5.Display.drawRect(0, 0, M5.Display.width(), M5.Display.height(), 0x000000);
  M5.Display.display(0, 0, M5.Display.width(), M5.Display.height());
  waitDisplayWithTimeout();
}
}  // namespace
#endif

void HalGPIO::begin() {
#ifdef PLATFORM_M5PAPER
  // Keep EPD and SD deselected/high before M5GFX autodetect.
  pinMode(GPIO_NUM_2, OUTPUT);
  digitalWrite(GPIO_NUM_2, HIGH);
  pinMode(GPIO_NUM_4, OUTPUT);
  digitalWrite(GPIO_NUM_4, HIGH);
  pinMode(GPIO_NUM_15, OUTPUT);
  digitalWrite(GPIO_NUM_15, HIGH);

  // Initialize M5Paper core (buttons, touch, power)
  auto cfg = M5.config();
  // Lock fallback to the selected M5Paper family target so autodetect mismatches do not break input/display.
#ifdef PLATFORM_M5PAPERS3
  cfg.fallback_board = m5::board_t::board_M5PaperS3;
#else
  cfg.fallback_board = m5::board_t::board_M5Paper;
#endif
  cfg.serial_baudrate = 115200;
  cfg.clear_display = false;
  M5.begin(cfg);

  if (ensureM5PaperDisplay()) {
    configureM5PaperDisplay();
    // Trigger a guaranteed visible update right after reset/flash.
    forceBootRefresh();
  }

  // Ensure touch class is bound to the primary display on M5Paper.
  if (!M5.Touch.isEnabled() && M5.Display.touch()) {
    M5.Touch.begin(&M5.Display);
  }

  // Explicit button pin modes for raw GPIO fallback reads.
#ifdef PLATFORM_M5PAPERS3
  pinMode(GPIO_NUM_42, INPUT_PULLUP);
  pinMode(GPIO_NUM_41, INPUT_PULLUP);
#else
  pinMode(GPIO_NUM_37, INPUT_PULLUP);
  pinMode(GPIO_NUM_38, INPUT_PULLUP);
  pinMode(GPIO_NUM_39, INPUT_PULLUP);
#endif

  inputMgr.begin();
#else
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  pinMode(BAT_GPIO0, INPUT);
  pinMode(UART0_RXD, INPUT);
#endif
}

void HalGPIO::update() { inputMgr.update(); }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

void HalGPIO::startDeepSleep() {
#ifdef PLATFORM_M5PAPER
  // Use M5Unified power flow to configure board-specific wake sources.
  M5.Power.deepSleep(0, true);
#else
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (inputMgr.isPressed(BTN_POWER)) {
    delay(50);
    inputMgr.update();
  }
  // Arm the wakeup trigger *after* the button is released
  esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  // Enter Deep Sleep
  esp_deep_sleep_start();
#endif
}

int HalGPIO::getBatteryPercentage() const {
#ifdef PLATFORM_M5PAPER
  int level = M5.Power.getBatteryLevel();
  if (level < 0) {
    level = 0;
  }
  if (level > 100) {
    level = 100;
  }
  return level;
#else
  static const BatteryMonitor battery = BatteryMonitor(BAT_GPIO0);
  return battery.readPercentage();
#endif
}

bool HalGPIO::isUsbConnected() const {
#ifdef PLATFORM_M5PAPER
  // M5Paper doesn't expose a simple USB-detect pin; assume connected for debugging
  return true;
#else
  // U0RXD/GPIO20 reads HIGH when USB is connected
  return digitalRead(UART0_RXD) == HIGH;
#endif
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
#ifdef PLATFORM_M5PAPER
  // M5Paper power/touch wake handling is managed by M5Unified.
  // Keep setup logic on the generic path (no power-button calibration).
  return WakeupReason::Other;
#else
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
#endif
}
