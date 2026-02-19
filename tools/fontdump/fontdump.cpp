#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "EpdFontData.h"
#include "builtinFonts/all.h"

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

static constexpr uint32_t FONT_MAGIC = 0x46504445;  // 'EPDF'
static constexpr uint16_t FONT_VERSION = 1;

struct FontSpec {
  const char* name;
  const EpdFontData* data;
};

static const FontSpec kFonts[] = {
    {"bookerly_12_regular", &bookerly_12_regular},
    {"bookerly_12_bold", &bookerly_12_bold},
    {"bookerly_12_italic", &bookerly_12_italic},
    {"bookerly_12_bolditalic", &bookerly_12_bolditalic},
    {"bookerly_14_regular", &bookerly_14_regular},
    {"bookerly_14_bold", &bookerly_14_bold},
    {"bookerly_14_italic", &bookerly_14_italic},
    {"bookerly_14_bolditalic", &bookerly_14_bolditalic},
    {"bookerly_16_regular", &bookerly_16_regular},
    {"bookerly_16_bold", &bookerly_16_bold},
    {"bookerly_16_italic", &bookerly_16_italic},
    {"bookerly_16_bolditalic", &bookerly_16_bolditalic},
    {"bookerly_18_regular", &bookerly_18_regular},
    {"bookerly_18_bold", &bookerly_18_bold},
    {"bookerly_18_italic", &bookerly_18_italic},
    {"bookerly_18_bolditalic", &bookerly_18_bolditalic},
    {"notosans_8_regular", &notosans_8_regular},
    {"notosans_12_regular", &notosans_12_regular},
    {"notosans_12_bold", &notosans_12_bold},
    {"notosans_12_italic", &notosans_12_italic},
    {"notosans_12_bolditalic", &notosans_12_bolditalic},
    {"notosans_14_regular", &notosans_14_regular},
    {"notosans_14_bold", &notosans_14_bold},
    {"notosans_14_italic", &notosans_14_italic},
    {"notosans_14_bolditalic", &notosans_14_bolditalic},
    {"notosans_16_regular", &notosans_16_regular},
    {"notosans_16_bold", &notosans_16_bold},
    {"notosans_16_italic", &notosans_16_italic},
    {"notosans_16_bolditalic", &notosans_16_bolditalic},
    {"notosans_18_regular", &notosans_18_regular},
    {"notosans_18_bold", &notosans_18_bold},
    {"notosans_18_italic", &notosans_18_italic},
    {"notosans_18_bolditalic", &notosans_18_bolditalic},
    {"opendyslexic_8_regular", &opendyslexic_8_regular},
    {"opendyslexic_8_bold", &opendyslexic_8_bold},
    {"opendyslexic_8_italic", &opendyslexic_8_italic},
    {"opendyslexic_8_bolditalic", &opendyslexic_8_bolditalic},
    {"opendyslexic_10_regular", &opendyslexic_10_regular},
    {"opendyslexic_10_bold", &opendyslexic_10_bold},
    {"opendyslexic_10_italic", &opendyslexic_10_italic},
    {"opendyslexic_10_bolditalic", &opendyslexic_10_bolditalic},
    {"opendyslexic_12_regular", &opendyslexic_12_regular},
    {"opendyslexic_12_bold", &opendyslexic_12_bold},
    {"opendyslexic_12_italic", &opendyslexic_12_italic},
    {"opendyslexic_12_bolditalic", &opendyslexic_12_bolditalic},
    {"opendyslexic_14_regular", &opendyslexic_14_regular},
    {"opendyslexic_14_bold", &opendyslexic_14_bold},
    {"opendyslexic_14_italic", &opendyslexic_14_italic},
    {"opendyslexic_14_bolditalic", &opendyslexic_14_bolditalic},
    {"ubuntu_10_regular", &ubuntu_10_regular},
    {"ubuntu_10_bold", &ubuntu_10_bold},
    {"ubuntu_12_regular", &ubuntu_12_regular},
    {"ubuntu_12_bold", &ubuntu_12_bold},
};

uint32_t computeGlyphCount(const EpdFontData* font) {
  uint32_t maxCount = 0;
  for (uint32_t i = 0; i < font->intervalCount; i++) {
    const auto& interval = font->intervals[i];
    const uint32_t count = interval.offset + (interval.last - interval.first + 1);
    if (count > maxCount) {
      maxCount = count;
    }
  }
  return maxCount;
}

uint32_t computeBitmapSize(const EpdFontData* font, const uint32_t glyphCount) {
  uint32_t maxSize = 0;
  for (uint32_t i = 0; i < glyphCount; i++) {
    const auto& glyph = font->glyph[i];
    const uint32_t end = glyph.dataOffset + glyph.dataLength;
    if (end > maxSize) {
      maxSize = end;
    }
  }
  return maxSize;
}

bool writeFontFile(const char* outDir, const FontSpec& spec) {
  const EpdFontData* font = spec.data;
  if (!font) {
    return false;
  }

  const uint32_t glyphCount = computeGlyphCount(font);
  const uint32_t bitmapSize = computeBitmapSize(font, glyphCount);

  FontFileHeader header{};
  header.magic = FONT_MAGIC;
  header.version = FONT_VERSION;
  header.flags = font->is2Bit ? 1 : 0;
  header.glyphCount = glyphCount;
  header.intervalCount = font->intervalCount;
  header.bitmapSize = bitmapSize;
  header.advanceY = font->advanceY;
  header.ascender = font->ascender;
  header.descender = font->descender;

  char path[256];
  snprintf(path, sizeof(path), "%s/%s.epf", outDir, spec.name);

  FILE* fp = fopen(path, "wb");
  if (!fp) {
    fprintf(stderr, "Failed to open %s\n", path);
    return false;
  }

  bool ok = true;
  ok &= fwrite(&header, sizeof(header), 1, fp) == 1;
  ok &= fwrite(font->intervals, sizeof(EpdUnicodeInterval), font->intervalCount, fp) == font->intervalCount;
  ok &= fwrite(font->glyph, sizeof(EpdGlyph), glyphCount, fp) == glyphCount;
  ok &= fwrite(font->bitmap, sizeof(uint8_t), bitmapSize, fp) == bitmapSize;

  fclose(fp);

  if (!ok) {
    fprintf(stderr, "Failed to write %s\n", path);
  }
  return ok;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <output-dir>\n", argv[0]);
    return 1;
  }

  const char* outDir = argv[1];
  bool ok = true;
  for (const auto& spec : kFonts) {
    if (!writeFontFile(outDir, spec)) {
      ok = false;
    }
  }

  return ok ? 0 : 1;
}
