#include "TimeUtils.h"

#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#ifdef PLATFORM_M5PAPER
#include <M5Unified.h>
#endif

namespace TimeUtils {
namespace {
constexpr time_t kMinValidEpoch = 1577836800;  // 2020-01-01

#ifdef PLATFORM_M5PAPER
void syncRtcFromSystemClock() {
  if (!M5.Rtc.isEnabled()) {
    return;
  }

  const time_t now = time(nullptr);
  if (now < kMinValidEpoch) {
    return;
  }

  std::tm utcTm {};
  gmtime_r(&now, &utcTm);
  M5.Rtc.setDateTime(&utcTm);
}
#endif
}

bool isTimeValid() {
  const time_t now = time(nullptr);
  return now >= kMinValidEpoch;
}

bool syncTimeWithNtp(const char* server, const uint32_t timeoutMs) {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }

  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, server);
  esp_sntp_init();

  const uint32_t maxRetries = timeoutMs / 100;
  uint32_t retry = 0;
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < maxRetries) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    retry++;
  }

  const bool synced = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
#ifdef PLATFORM_M5PAPER
  if (synced) {
    syncRtcFromSystemClock();
  }
#endif
  return synced;
}

bool getLocalTimeWithOffset(std::tm& out, const int16_t offsetMinutes) {
  const time_t now = time(nullptr);
  if (now < kMinValidEpoch) {
    return false;
  }
  const time_t local = now + static_cast<time_t>(offsetMinutes) * 60;
  gmtime_r(&local, &out);
  return true;
}
}  // namespace TimeUtils
