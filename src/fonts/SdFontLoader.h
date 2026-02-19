#pragma once

#include <EpdFontFamily.h>

class GfxRenderer;

namespace SdFontLoader {
// Loads fonts from SD ("/fonts/*.epf") and registers them with the renderer.
// If a font cannot be loaded, the provided fallback font family is used instead.
// Returns true if all fonts were loaded from SD successfully.
bool registerFonts(GfxRenderer& renderer, const EpdFontFamily& fallback);
}  // namespace SdFontLoader
