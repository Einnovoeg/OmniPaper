#include "OtaUpdater.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <string>

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_wifi.h"

namespace {
#ifndef OMNIPAPER_GITHUB_REPOSITORY
#define OMNIPAPER_GITHUB_REPOSITORY ""
#endif

constexpr char githubApiBaseUrl[] = "https://api.github.com/repos/";
constexpr char legacyFirmwareAssetName[] = "firmware.bin";

/* This buffer stores the GitHub Releases API response for the current OTA check. */
char* local_buf;
int output_len;

/*
 * When esp_crt_bundle.h included, it is pointing wrong header file
 * which is something under WifiClientSecure because of our framework based on arduno platform.
 * To manage this obstacle, don't include anything, just extern and it will point correct one.
 */
extern "C" {
extern esp_err_t esp_crt_bundle_attach(void* conf);
}

esp_err_t http_client_set_header_cb(esp_http_client_handle_t http_client) {
  return esp_http_client_set_header(http_client, "User-Agent", "OmniPaper-ESP32-" CROSSPOINT_VERSION);
}

std::string getReleaseRepository() { return OMNIPAPER_GITHUB_REPOSITORY; }

std::string buildLatestReleaseUrl(const std::string& repository) {
  return std::string(githubApiBaseUrl) + repository + "/releases/latest";
}

bool endsWith(const std::string& value, const std::string& suffix) {
  if (suffix.size() > value.size()) {
    return false;
  }

  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string expectedFirmwareAssetSuffix() {
#if defined(PLATFORM_M5PAPERS3)
  return "-m5papers3-firmware.bin";
#elif defined(PLATFORM_M5PAPER)
  return "-m5paper-firmware.bin";
#else
  return "-firmware.bin";
#endif
}

std::string normalizeVersion(std::string version) {
  if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) {
    version.erase(0, 1);
  }

  const auto firstNonVersionChar = version.find_first_not_of("0123456789.");
  if (firstNonVersionChar != std::string::npos) {
    version.erase(firstNonVersionChar);
  }

  return version;
}

bool parseSemanticVersion(const std::string& rawVersion, int& major, int& minor, int& patch) {
  const auto normalized = normalizeVersion(rawVersion);
  return sscanf(normalized.c_str(), "%d.%d.%d", &major, &minor, &patch) == 3;
}

esp_err_t event_handler(esp_http_client_event_t* event) {
  /* We do interested in only HTTP_EVENT_ON_DATA event only */
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;

  if (!esp_http_client_is_chunked_response(event->client)) {
    int content_len = esp_http_client_get_content_length(event->client);
    int copy_len = 0;

    if (local_buf == NULL) {
      /* local_buf life span is tracked by caller checkForUpdate */
      local_buf = static_cast<char*>(calloc(content_len + 1, sizeof(char)));
      output_len = 0;
      if (local_buf == NULL) {
        Serial.printf("[%lu] [OTA] HTTP Client Out of Memory Failed, Allocation %d\n", millis(), content_len);
        return ESP_ERR_NO_MEM;
      }
    }
    copy_len = min(event->data_len, (content_len - output_len));
    if (copy_len) {
      memcpy(local_buf + output_len, event->data, copy_len);
    }
    output_len += copy_len;
  } else {
    /* Code might be hits here, It happened once (for version checking) but I need more logs to handle that */
    int chunked_len;
    esp_http_client_get_chunk_length(event->client, &chunked_len);
    Serial.printf("[%lu] [OTA] esp_http_client_is_chunked_response failed, chunked_len: %d\n", millis(), chunked_len);
  }

  return ESP_OK;
} /* event_handler */
} /* namespace */

OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() {
  JsonDocument filter;
  esp_err_t esp_err;
  JsonDocument doc;
  const auto releaseRepository = getReleaseRepository();
  if (releaseRepository.empty()) {
    Serial.printf("[%lu] [OTA] GitHub release repository is not configured for this build\n", millis());
    return NO_UPDATE;
  }

  const auto latestReleaseUrl = buildLatestReleaseUrl(releaseRepository);
  const auto expectedAssetSuffix = expectedFirmwareAssetSuffix();
  updateAvailable = false;
  latestVersion.clear();
  otaUrl.clear();
  otaSize = 0;
  processedSize = 0;
  totalSize = 0;
  local_buf = NULL;
  output_len = 0;

  esp_http_client_config_t client_config = {
      .url = latestReleaseUrl.c_str(),
      .event_handler = event_handler,
      /* Default HTTP client buffer size 512 byte only */
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  /* To track life time of local_buf, dtor will be called on exit from that function */
  struct localBufCleaner {
    char** bufPtr;
    ~localBufCleaner() {
      if (*bufPtr) {
        free(*bufPtr);
        *bufPtr = NULL;
      }
    }
  } localBufCleaner = {&local_buf};

  esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
  if (!client_handle) {
    Serial.printf("[%lu] [OTA] HTTP Client Handle Failed\n", millis());
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_set_header(client_handle, "User-Agent", "OmniPaper-ESP32-" CROSSPOINT_VERSION);
  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] esp_http_client_set_header Failed : %s\n", millis(), esp_err_to_name(esp_err));
    esp_http_client_cleanup(client_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_perform(client_handle);
  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] esp_http_client_perform Failed : %s\n", millis(), esp_err_to_name(esp_err));
    esp_http_client_cleanup(client_handle);
    return HTTP_ERROR;
  }

  /* esp_http_client_close will be called inside cleanup as well*/
  esp_err = esp_http_client_cleanup(client_handle);
  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] esp_http_client_cleanupp Failed : %s\n", millis(), esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  filter["assets"][0]["size"] = true;
  const DeserializationError error = deserializeJson(doc, local_buf, DeserializationOption::Filter(filter));
  if (error) {
    Serial.printf("[%lu] [OTA] JSON parse failed: %s\n", millis(), error.c_str());
    return JSON_PARSE_ERROR;
  }

  if (!doc["tag_name"].is<std::string>()) {
    Serial.printf("[%lu] [OTA] No tag_name found\n", millis());
    return JSON_PARSE_ERROR;
  }

  if (!doc["assets"].is<JsonArray>()) {
    Serial.printf("[%lu] [OTA] No assets found\n", millis());
    return JSON_PARSE_ERROR;
  }

  latestVersion = doc["tag_name"].as<std::string>();
  JsonObject fallbackAsset;

  for (int i = 0; i < doc["assets"].size(); i++) {
    const JsonObject asset = doc["assets"][i].as<JsonObject>();
    if (!asset["name"].is<const char*>()) {
      continue;
    }

    const std::string assetName = asset["name"].as<std::string>();
    if (assetName == legacyFirmwareAssetName) {
      fallbackAsset = asset;
    }

    if (!endsWith(assetName, expectedAssetSuffix)) {
      continue;
    }

    otaUrl = asset["browser_download_url"].as<std::string>();
    otaSize = asset["size"].as<size_t>();
    totalSize = otaSize;
    updateAvailable = true;
    break;
  }

  if (!updateAvailable && !fallbackAsset.isNull()) {
    otaUrl = fallbackAsset["browser_download_url"].as<std::string>();
    otaSize = fallbackAsset["size"].as<size_t>();
    totalSize = otaSize;
    updateAvailable = true;
  }

  if (!updateAvailable) {
    Serial.printf("[%lu] [OTA] No firmware asset found for suffix %s\n", millis(), expectedAssetSuffix.c_str());
    return NO_UPDATE;
  }

  Serial.printf("[%lu] [OTA] Found update: %s\n", millis(), latestVersion.c_str());
  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == CROSSPOINT_VERSION) {
    return false;
  }

  int currentMajor = 0;
  int currentMinor = 0;
  int currentPatch = 0;
  int latestMajor = 0;
  int latestMinor = 0;
  int latestPatch = 0;

  const auto currentVersion = std::string(CROSSPOINT_VERSION);
  if (!parseSemanticVersion(latestVersion, latestMajor, latestMinor, latestPatch) ||
      !parseSemanticVersion(currentVersion, currentMajor, currentMinor, currentPatch)) {
    Serial.printf("[%lu] [OTA] Failed to parse semantic versions: latest=%s current=%s\n", millis(),
                  latestVersion.c_str(), currentVersion.c_str());
    return false;
  }

  /*
   * Compare major versions.
   * If they differ, return true if latest major version greater than current major version
   * otherwise return false.
   */
  if (latestMajor != currentMajor) return latestMajor > currentMajor;

  /*
   * Compare minor versions.
   * If they differ, return true if latest minor version greater than current minor version
   * otherwise return false.
   */
  if (latestMinor != currentMinor) return latestMinor > currentMinor;

  /*
   * Check patch versions.
   */
  return latestPatch > currentPatch;
}

const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate() {
  if (!isUpdateNewer()) {
    return UPDATE_OLDER_ERROR;
  }

  esp_https_ota_handle_t ota_handle = NULL;
  esp_err_t esp_err;
  /* Signal for OtaUpdateActivity */
  render = false;

  esp_http_client_config_t client_config = {
      .url = otaUrl.c_str(),
      .timeout_ms = 15000,
      /* Default HTTP client buffer size 512 byte only
       * not sufficent to handle URL redirection cases or
       * parsing of large HTTP headers.
       */
      .buffer_size = 8192,
      .buffer_size_tx = 8192,
      .skip_cert_common_name_check = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &client_config,
      .http_client_init_cb = http_client_set_header_cb,
  };

  /* For better timing and connectivity, we disable power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_err = esp_https_ota_begin(&ota_config, &ota_handle);
  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] HTTP OTA Begin Failed: %s\n", millis(), esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  do {
    esp_err = esp_https_ota_perform(ota_handle);
    processedSize = esp_https_ota_get_image_len_read(ota_handle);
    /* Sent signal to  OtaUpdateActivity */
    render = true;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  } while (esp_err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  /* Return back to default power saving for WiFi in case of failing */
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] esp_https_ota_perform Failed: %s\n", millis(), esp_err_to_name(esp_err));
    esp_https_ota_finish(ota_handle);
    return HTTP_ERROR;
  }

  if (!esp_https_ota_is_complete_data_received(ota_handle)) {
    Serial.printf("[%lu] [OTA] esp_https_ota_is_complete_data_received Failed: %s\n", millis(),
                  esp_err_to_name(esp_err));
    esp_https_ota_finish(ota_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_https_ota_finish(ota_handle);
  if (esp_err != ESP_OK) {
    Serial.printf("[%lu] [OTA] esp_https_ota_finish Failed: %s\n", millis(), esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  Serial.printf("[%lu] [OTA] Update completed\n", millis());
  return OK;
}
