#pragma once

#include <cstdint>
#include <ctime>

namespace TimeUtils {
// Returns true if system time looks valid (after Jan 1, 2020).
bool isTimeValid();

// Sync time using NTP. Returns true on success.
bool syncTimeWithNtp(const char* server = "pool.ntp.org", uint32_t timeoutMs = 5000);

// Get local time adjusted by offset minutes. Returns false if time is invalid.
bool getLocalTimeWithOffset(std::tm& out, int16_t offsetMinutes);
}  // namespace TimeUtils
