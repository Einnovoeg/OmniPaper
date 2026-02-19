#pragma once

#include <stddef.h>

#if defined(M5PAPER_HARDWARE)

#include <M5GFX.h>

struct U8g2FontHandle {
  lgfx::U8g2font* font = nullptr;
  uint8_t* data = nullptr;
  size_t size = 0;
  bool loaded = false;
};

namespace M5GfxSdU8g2Loader {
// Load a raw U8G2 font file from SD into PSRAM/heap.
// The file should contain the exact bytes from a U8G2 font array.
bool loadFont(const char* path, U8g2FontHandle& out, size_t maxSize = 8 * 1024 * 1024);
// Free any loaded font data.
void unloadFont(U8g2FontHandle& out);
}

#else

struct U8g2FontHandle {};

namespace M5GfxSdU8g2Loader {
inline bool loadFont(const char*, U8g2FontHandle&, size_t = 0) { return false; }
inline void unloadFont(U8g2FontHandle&) {}
}

#endif
