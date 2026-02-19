#include "M5GfxSdU8g2Loader.h"

#if defined(M5PAPER_HARDWARE)

#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <esp32-hal-psram.h>

namespace {
void* fontAlloc(size_t size) {
  void* ptr = ps_malloc(size);
  if (!ptr) {
    ptr = malloc(size);
  }
  return ptr;
}
}

namespace M5GfxSdU8g2Loader {

bool loadFont(const char* path, U8g2FontHandle& out, size_t maxSize) {
  unloadFont(out);

  if (!SdMan.ready()) {
    Serial.printf("[%lu] [M5GFX] SD not ready for font: %s\n", millis(), path);
    return false;
  }

  FsFile file;
  if (!SdMan.openFileForRead("M5GFX", path, file)) {
    Serial.printf("[%lu] [M5GFX] Missing font file: %s\n", millis(), path);
    return false;
  }

  const size_t size = file.size();
  if (size == 0 || size > maxSize) {
    Serial.printf("[%lu] [M5GFX] Invalid font size (%u) for %s\n", millis(), static_cast<unsigned>(size), path);
    file.close();
    return false;
  }

  uint8_t* buffer = static_cast<uint8_t*>(fontAlloc(size));
  if (!buffer) {
    Serial.printf("[%lu] [M5GFX] Out of memory loading %s\n", millis(), path);
    file.close();
    return false;
  }

  const int readBytes = file.read(buffer, size);
  file.close();
  if (readBytes != static_cast<int>(size)) {
    Serial.printf("[%lu] [M5GFX] Failed to read font data: %s\n", millis(), path);
    free(buffer);
    return false;
  }

  out.data = buffer;
  out.size = size;
  out.font = new lgfx::U8g2font(out.data);
  out.loaded = true;
  return true;
}

void unloadFont(U8g2FontHandle& out) {
  if (out.font) {
    delete out.font;
  }
  if (out.data) {
    free(out.data);
  }
  out.font = nullptr;
  out.data = nullptr;
  out.size = 0;
  out.loaded = false;
}

}  // namespace M5GfxSdU8g2Loader

#endif
