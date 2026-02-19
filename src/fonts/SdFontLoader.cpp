#include "SdFontLoader.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <esp32-hal-psram.h>

#include <cstring>

#include "fontIds.h"

namespace {
constexpr uint32_t FONT_MAGIC = 0x46504445;  // 'EPDF'
constexpr uint16_t FONT_VERSION = 1;
constexpr char FONT_DIR[] = "/fonts";

#pragma pack(push, 1)
struct FontFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t glyphCount;
  uint32_t intervalCount;
  uint32_t bitmapSize;
  int32_t advanceY;
  int32_t ascender;
  int32_t descender;
};
#pragma pack(pop)

static_assert(sizeof(FontFileHeader) == 32, "Unexpected font header size");

struct LoadedFont {
  EpdFontData data{};
  EpdFont font;
  uint8_t* bitmap = nullptr;
  EpdGlyph* glyphs = nullptr;
  EpdUnicodeInterval* intervals = nullptr;
  bool loaded = false;

  LoadedFont() : font(&data) {}
};

struct LoadedFontFamily {
  LoadedFont regular;
  LoadedFont bold;
  LoadedFont italic;
  LoadedFont boldItalic;
  bool hasRegular = false;
  bool hasBold = false;
  bool hasItalic = false;
  bool hasBoldItalic = false;
};

void* fontAlloc(const size_t size) {
  void* ptr = ps_malloc(size);
  if (!ptr) {
    ptr = malloc(size);
  }
  return ptr;
}

void resetLoadedFont(LoadedFont& font) {
  if (font.bitmap) {
    free(font.bitmap);
  }
  if (font.glyphs) {
    free(font.glyphs);
  }
  if (font.intervals) {
    free(font.intervals);
  }
  font.bitmap = nullptr;
  font.glyphs = nullptr;
  font.intervals = nullptr;
  font.loaded = false;
}

bool readExact(FsFile& file, void* out, const size_t size) {
  return file.read(out, size) == static_cast<int>(size);
}

bool loadFontFile(const char* name, LoadedFont& out) {
  resetLoadedFont(out);

  char path[128];
  snprintf(path, sizeof(path), "%s/%s.epf", FONT_DIR, name);

  FsFile f;
  if (!SdMan.openFileForRead("FONT", path, f)) {
    Serial.printf("[%lu] [FONT] Missing font file: %s\n", millis(), path);
    return false;
  }

  FontFileHeader header{};
  if (!readExact(f, &header, sizeof(header))) {
    Serial.printf("[%lu] [FONT] Failed to read font header: %s\n", millis(), path);
    f.close();
    return false;
  }

  if (header.magic != FONT_MAGIC || header.version != FONT_VERSION) {
    Serial.printf("[%lu] [FONT] Invalid font header: %s\n", millis(), path);
    f.close();
    return false;
  }

  if (header.glyphCount == 0 || header.intervalCount == 0 || header.bitmapSize == 0) {
    Serial.printf("[%lu] [FONT] Empty font data: %s\n", millis(), path);
    f.close();
    return false;
  }

  if (header.glyphCount > 20000 || header.intervalCount > 4096 || header.bitmapSize > (4 * 1024 * 1024)) {
    Serial.printf("[%lu] [FONT] Font too large: %s\n", millis(), path);
    f.close();
    return false;
  }

  out.intervals = static_cast<EpdUnicodeInterval*>(fontAlloc(header.intervalCount * sizeof(EpdUnicodeInterval)));
  out.glyphs = static_cast<EpdGlyph*>(fontAlloc(header.glyphCount * sizeof(EpdGlyph)));
  out.bitmap = static_cast<uint8_t*>(fontAlloc(header.bitmapSize));

  if (!out.intervals || !out.glyphs || !out.bitmap) {
    Serial.printf("[%lu] [FONT] Out of memory loading %s\n", millis(), path);
    f.close();
    resetLoadedFont(out);
    return false;
  }

  if (!readExact(f, out.intervals, header.intervalCount * sizeof(EpdUnicodeInterval)) ||
      !readExact(f, out.glyphs, header.glyphCount * sizeof(EpdGlyph)) ||
      !readExact(f, out.bitmap, header.bitmapSize)) {
    Serial.printf("[%lu] [FONT] Failed to read font data: %s\n", millis(), path);
    f.close();
    resetLoadedFont(out);
    return false;
  }

  f.close();

  out.data.bitmap = out.bitmap;
  out.data.glyph = out.glyphs;
  out.data.intervals = out.intervals;
  out.data.intervalCount = header.intervalCount;
  out.data.advanceY = header.advanceY < 0 ? 0 : (header.advanceY > 255 ? 255 : static_cast<uint8_t>(header.advanceY));
  out.data.ascender = static_cast<int>(header.ascender);
  out.data.descender = static_cast<int>(header.descender);
  out.data.is2Bit = (header.flags & 0x1) != 0;
  out.loaded = true;

  return true;
}

bool loadFontFamily(const char* baseName, LoadedFontFamily& family) {
  char name[96];

  snprintf(name, sizeof(name), "%s_regular", baseName);
  family.hasRegular = loadFontFile(name, family.regular);
  if (!family.hasRegular) {
    return false;
  }

  snprintf(name, sizeof(name), "%s_bold", baseName);
  family.hasBold = loadFontFile(name, family.bold);

  snprintf(name, sizeof(name), "%s_italic", baseName);
  family.hasItalic = loadFontFile(name, family.italic);

  snprintf(name, sizeof(name), "%s_bolditalic", baseName);
  family.hasBoldItalic = loadFontFile(name, family.boldItalic);

  return true;
}

EpdFontFamily buildFamily(const LoadedFontFamily& family) {
  const EpdFont* regular = &family.regular.font;
  const EpdFont* bold = family.hasBold ? &family.bold.font : regular;
  const EpdFont* italic = family.hasItalic ? &family.italic.font : regular;
  const EpdFont* boldItalic = family.hasBoldItalic ? &family.boldItalic.font : bold;
  return EpdFontFamily(regular, bold, italic, boldItalic);
}

struct FontSpec {
  int id;
  const char* baseName;
  LoadedFontFamily* storage;
};

LoadedFontFamily bookerly12;
LoadedFontFamily bookerly14;
LoadedFontFamily bookerly16;
LoadedFontFamily bookerly18;
LoadedFontFamily notosans12;
LoadedFontFamily notosans14;
LoadedFontFamily notosans16;
LoadedFontFamily notosans18;
LoadedFontFamily opendyslexic8;
LoadedFontFamily opendyslexic10;
LoadedFontFamily opendyslexic12;
LoadedFontFamily opendyslexic14;
LoadedFontFamily user12;
LoadedFontFamily user14;
LoadedFontFamily user16;
LoadedFontFamily user18;
LoadedFontFamily ubuntu10;
LoadedFontFamily ubuntu12;
LoadedFontFamily notosans8;

const FontSpec kFonts[] = {
    {BOOKERLY_12_FONT_ID, "bookerly_12", &bookerly12},
    {BOOKERLY_14_FONT_ID, "bookerly_14", &bookerly14},
    {BOOKERLY_16_FONT_ID, "bookerly_16", &bookerly16},
    {BOOKERLY_18_FONT_ID, "bookerly_18", &bookerly18},
    {NOTOSANS_12_FONT_ID, "notosans_12", &notosans12},
    {NOTOSANS_14_FONT_ID, "notosans_14", &notosans14},
    {NOTOSANS_16_FONT_ID, "notosans_16", &notosans16},
    {NOTOSANS_18_FONT_ID, "notosans_18", &notosans18},
    {OPENDYSLEXIC_8_FONT_ID, "opendyslexic_8", &opendyslexic8},
    {OPENDYSLEXIC_10_FONT_ID, "opendyslexic_10", &opendyslexic10},
    {OPENDYSLEXIC_12_FONT_ID, "opendyslexic_12", &opendyslexic12},
    {OPENDYSLEXIC_14_FONT_ID, "opendyslexic_14", &opendyslexic14},
    {USER_12_FONT_ID, "user_12", &user12},
    {USER_14_FONT_ID, "user_14", &user14},
    {USER_16_FONT_ID, "user_16", &user16},
    {USER_18_FONT_ID, "user_18", &user18},
    {UI_10_FONT_ID, "ubuntu_10", &ubuntu10},
    {UI_12_FONT_ID, "ubuntu_12", &ubuntu12},
    {SMALL_FONT_ID, "notosans_8", &notosans8},
};

bool registerFont(GfxRenderer& renderer, const FontSpec& spec, const EpdFontFamily& fallback) {
  if (!loadFontFamily(spec.baseName, *spec.storage)) {
    renderer.insertFont(spec.id, fallback);
    return false;
  }

  renderer.insertFont(spec.id, buildFamily(*spec.storage));
  return true;
}
}  // namespace

namespace SdFontLoader {

bool registerFonts(GfxRenderer& renderer, const EpdFontFamily& fallback) {
  bool allOk = true;

  if (!SdMan.ready()) {
    Serial.printf("[%lu] [FONT] SD not ready; using fallback fonts\n", millis());
    for (const auto& spec : kFonts) {
      renderer.insertFont(spec.id, fallback);
    }
    return false;
  }

  for (const auto& spec : kFonts) {
    const bool ok = registerFont(renderer, spec, fallback);
    if (!ok) {
      allOk = false;
    }
  }

  return allOk;
}

}  // namespace SdFontLoader
