#include "Section.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <JpegToBmpConverter.h>
#include <SDCardManager.h>
#include <Serialization.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "Page.h"
#include "hyphenation/Hyphenator.h"
#include "parsers/ChapterHtmlSlimParser.h"

namespace {
constexpr uint8_t SECTION_FILE_VERSION = 11;
constexpr uint32_t HEADER_SIZE = sizeof(uint8_t) + sizeof(int) + sizeof(float) + sizeof(bool) + sizeof(uint8_t) +
                                 sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(bool) +
                                 sizeof(uint32_t);

std::string toLower(const std::string& input) {
  std::string out = input;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string stripQueryAndFragment(const std::string& path) {
  size_t end = path.size();
  const size_t q = path.find('?');
  const size_t h = path.find('#');
  if (q != std::string::npos) end = std::min(end, q);
  if (h != std::string::npos) end = std::min(end, h);
  return path.substr(0, end);
}

std::string getDirectoryPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return "";
  }
  return path.substr(0, slash + 1);
}

bool hasAnyExtension(const std::string& path, const std::initializer_list<const char*> exts) {
  const std::string lower = toLower(path);
  for (const char* ext : exts) {
    const size_t extLen = strlen(ext);
    if (lower.size() >= extLen && lower.compare(lower.size() - extLen, extLen, ext) == 0) {
      return true;
    }
  }
  return false;
}

bool getBmpDimensions(const std::string& bmpPath, uint16_t& outW, uint16_t& outH) {
  FsFile f;
  if (!SdMan.openFileForRead("SCT", bmpPath, f)) {
    return false;
  }
  Bitmap bitmap(f);
  const bool ok = bitmap.parseHeaders() == BmpReaderError::Ok;
  if (ok) {
    outW = static_cast<uint16_t>(bitmap.getWidth());
    outH = static_cast<uint16_t>(bitmap.getHeight());
  }
  f.close();
  return ok;
}

bool resolveEpubImageToBmp(const std::shared_ptr<Epub>& epub, const std::string& chapterHref, const std::string& rawSrc,
                           const uint16_t viewportWidth, const uint16_t viewportHeight, std::string& outBmpPath,
                           uint16_t& outWidth, uint16_t& outHeight) {
  outBmpPath.clear();
  outWidth = 0;
  outHeight = 0;

  std::string src = stripQueryAndFragment(rawSrc);
  if (src.empty()) {
    return false;
  }
  if (src.rfind("data:", 0) == 0 || src.rfind("http://", 0) == 0 || src.rfind("https://", 0) == 0) {
    return false;
  }

  std::string resolvedHref;
  if (!src.empty() && src[0] == '/') {
    resolvedHref = FsHelpers::normalisePath(src.substr(1));
  } else {
    resolvedHref = FsHelpers::normalisePath(getDirectoryPath(chapterHref) + src);
  }
  if (resolvedHref.empty()) {
    return false;
  }

  const std::string cacheBase = epub->getCachePath() + "/img_" + std::to_string(std::hash<std::string>{}(resolvedHref));
  const std::string bmpPath = cacheBase + ".bmp";
  if (SdMan.exists(bmpPath.c_str())) {
    if (getBmpDimensions(bmpPath, outWidth, outHeight)) {
      outBmpPath = bmpPath;
      return true;
    }
    SdMan.remove(bmpPath.c_str());
  }

  if (hasAnyExtension(resolvedHref, {".bmp"})) {
    FsFile dst;
    if (!SdMan.openFileForWrite("SCT", bmpPath, dst)) {
      return false;
    }
    const bool copied = epub->readItemContentsToStream(resolvedHref, dst, 1024);
    dst.close();
    if (!copied) {
      SdMan.remove(bmpPath.c_str());
      return false;
    }
    if (getBmpDimensions(bmpPath, outWidth, outHeight)) {
      outBmpPath = bmpPath;
      return true;
    }
    SdMan.remove(bmpPath.c_str());
    return false;
  }

  if (!hasAnyExtension(resolvedHref, {".jpg", ".jpeg"})) {
    return false;
  }

  const std::string jpgTempPath = cacheBase + ".jpg";

  FsFile jpgOut;
  if (!SdMan.openFileForWrite("SCT", jpgTempPath, jpgOut)) {
    return false;
  }
  const bool readOk = epub->readItemContentsToStream(resolvedHref, jpgOut, 1024);
  jpgOut.close();
  if (!readOk) {
    SdMan.remove(jpgTempPath.c_str());
    return false;
  }

  FsFile jpgIn;
  if (!SdMan.openFileForRead("SCT", jpgTempPath, jpgIn)) {
    SdMan.remove(jpgTempPath.c_str());
    return false;
  }

  FsFile bmpOut;
  if (!SdMan.openFileForWrite("SCT", bmpPath, bmpOut)) {
    jpgIn.close();
    SdMan.remove(jpgTempPath.c_str());
    return false;
  }

  const int targetMaxWidth = std::max(80, static_cast<int>(viewportWidth));
  const int targetMaxHeight = std::max(60, static_cast<int>(viewportHeight));
  const bool convertOk =
      JpegToBmpConverter::jpegFileToBmpStreamWithSize(jpgIn, bmpOut, targetMaxWidth, targetMaxHeight);
  bmpOut.close();
  jpgIn.close();
  SdMan.remove(jpgTempPath.c_str());

  if (!convertOk) {
    SdMan.remove(bmpPath.c_str());
    return false;
  }

  if (!getBmpDimensions(bmpPath, outWidth, outHeight)) {
    SdMan.remove(bmpPath.c_str());
    return false;
  }

  outBmpPath = bmpPath;
  return true;
}
}  // namespace

uint32_t Section::onPageComplete(std::unique_ptr<Page> page) {
  if (!file) {
    Serial.printf("[%lu] [SCT] File not open for writing page %d\n", millis(), pageCount);
    return 0;
  }

  const uint32_t position = file.position();
  if (!page->serialize(file)) {
    Serial.printf("[%lu] [SCT] Failed to serialize page %d\n", millis(), pageCount);
    return 0;
  }
  Serial.printf("[%lu] [SCT] Page %d processed\n", millis(), pageCount);

  pageCount++;
  return position;
}

void Section::writeSectionFileHeader(const int fontId, const float lineCompression, const bool extraParagraphSpacing,
                                     const uint8_t paragraphAlignment, const uint16_t viewportWidth,
                                     const uint16_t viewportHeight, const bool hyphenationEnabled) {
  if (!file) {
    Serial.printf("[%lu] [SCT] File not open for writing header\n", millis());
    return;
  }
  static_assert(HEADER_SIZE == sizeof(SECTION_FILE_VERSION) + sizeof(fontId) + sizeof(lineCompression) +
                                   sizeof(extraParagraphSpacing) + sizeof(paragraphAlignment) + sizeof(viewportWidth) +
                                   sizeof(viewportHeight) + sizeof(pageCount) + sizeof(hyphenationEnabled) +
                                   sizeof(uint32_t),
                "Header size mismatch");
  serialization::writePod(file, SECTION_FILE_VERSION);
  serialization::writePod(file, fontId);
  serialization::writePod(file, lineCompression);
  serialization::writePod(file, extraParagraphSpacing);
  serialization::writePod(file, paragraphAlignment);
  serialization::writePod(file, viewportWidth);
  serialization::writePod(file, viewportHeight);
  serialization::writePod(file, hyphenationEnabled);
  serialization::writePod(file, pageCount);  // Placeholder for page count (will be initially 0 when written)
  serialization::writePod(file, static_cast<uint32_t>(0));  // Placeholder for LUT offset
}

bool Section::loadSectionFile(const int fontId, const float lineCompression, const bool extraParagraphSpacing,
                              const uint8_t paragraphAlignment, const uint16_t viewportWidth,
                              const uint16_t viewportHeight, const bool hyphenationEnabled) {
  if (!SdMan.openFileForRead("SCT", filePath, file)) {
    return false;
  }

  // Match parameters
  {
    uint8_t version;
    serialization::readPod(file, version);
    if (version != SECTION_FILE_VERSION) {
      file.close();
      Serial.printf("[%lu] [SCT] Deserialization failed: Unknown version %u\n", millis(), version);
      clearCache();
      return false;
    }

    int fileFontId;
    uint16_t fileViewportWidth, fileViewportHeight;
    float fileLineCompression;
    bool fileExtraParagraphSpacing;
    uint8_t fileParagraphAlignment;
    bool fileHyphenationEnabled;
    serialization::readPod(file, fileFontId);
    serialization::readPod(file, fileLineCompression);
    serialization::readPod(file, fileExtraParagraphSpacing);
    serialization::readPod(file, fileParagraphAlignment);
    serialization::readPod(file, fileViewportWidth);
    serialization::readPod(file, fileViewportHeight);
    serialization::readPod(file, fileHyphenationEnabled);

    if (fontId != fileFontId || lineCompression != fileLineCompression ||
        extraParagraphSpacing != fileExtraParagraphSpacing || paragraphAlignment != fileParagraphAlignment ||
        viewportWidth != fileViewportWidth || viewportHeight != fileViewportHeight ||
        hyphenationEnabled != fileHyphenationEnabled) {
      file.close();
      Serial.printf("[%lu] [SCT] Deserialization failed: Parameters do not match\n", millis());
      clearCache();
      return false;
    }
  }

  serialization::readPod(file, pageCount);
  file.close();
  Serial.printf("[%lu] [SCT] Deserialization succeeded: %d pages\n", millis(), pageCount);
  return true;
}

// Your updated class method (assuming you are using the 'SD' object, which is a wrapper for a specific filesystem)
bool Section::clearCache() const {
  if (!SdMan.exists(filePath.c_str())) {
    Serial.printf("[%lu] [SCT] Cache does not exist, no action needed\n", millis());
    return true;
  }

  if (!SdMan.remove(filePath.c_str())) {
    Serial.printf("[%lu] [SCT] Failed to clear cache\n", millis());
    return false;
  }

  Serial.printf("[%lu] [SCT] Cache cleared successfully\n", millis());
  return true;
}

bool Section::createSectionFile(const int fontId, const float lineCompression, const bool extraParagraphSpacing,
                                const uint8_t paragraphAlignment, const uint16_t viewportWidth,
                                const uint16_t viewportHeight, const bool hyphenationEnabled,
                                const std::function<void()>& popupFn) {
  const auto localPath = epub->getSpineItem(spineIndex).href;
  const auto tmpHtmlPath = epub->getCachePath() + "/.tmp_" + std::to_string(spineIndex) + ".html";

  // Create cache directory if it doesn't exist
  {
    const auto sectionsDir = epub->getCachePath() + "/sections";
    SdMan.mkdir(sectionsDir.c_str());
  }

  // Retry logic for SD card timing issues
  bool success = false;
  uint32_t fileSize = 0;
  for (int attempt = 0; attempt < 3 && !success; attempt++) {
    if (attempt > 0) {
      Serial.printf("[%lu] [SCT] Retrying stream (attempt %d)...\n", millis(), attempt + 1);
      delay(50);  // Brief delay before retry
    }

    // Remove any incomplete file from previous attempt before retrying
    if (SdMan.exists(tmpHtmlPath.c_str())) {
      SdMan.remove(tmpHtmlPath.c_str());
    }

    FsFile tmpHtml;
    if (!SdMan.openFileForWrite("SCT", tmpHtmlPath, tmpHtml)) {
      continue;
    }
    success = epub->readItemContentsToStream(localPath, tmpHtml, 1024);
    fileSize = tmpHtml.size();
    tmpHtml.close();

    // If streaming failed, remove the incomplete file immediately
    if (!success && SdMan.exists(tmpHtmlPath.c_str())) {
      SdMan.remove(tmpHtmlPath.c_str());
      Serial.printf("[%lu] [SCT] Removed incomplete temp file after failed attempt\n", millis());
    }
  }

  if (!success) {
    Serial.printf("[%lu] [SCT] Failed to stream item contents to temp file after retries\n", millis());
    return false;
  }

  Serial.printf("[%lu] [SCT] Streamed temp HTML to %s (%d bytes)\n", millis(), tmpHtmlPath.c_str(), fileSize);

  if (!SdMan.openFileForWrite("SCT", filePath, file)) {
    return false;
  }
  writeSectionFileHeader(fontId, lineCompression, extraParagraphSpacing, paragraphAlignment, viewportWidth,
                         viewportHeight, hyphenationEnabled);
  std::vector<uint32_t> lut = {};

  ChapterHtmlSlimParser visitor(
      tmpHtmlPath, renderer, fontId, lineCompression, extraParagraphSpacing, paragraphAlignment, viewportWidth,
      viewportHeight, hyphenationEnabled,
      [this, &lut](std::unique_ptr<Page> page) { lut.emplace_back(this->onPageComplete(std::move(page))); }, popupFn,
      [this, localPath, viewportWidth, viewportHeight](const std::string& src, std::string& outBmpPath, uint16_t& outW,
                                                       uint16_t& outH) {
        return resolveEpubImageToBmp(epub, localPath, src, viewportWidth, viewportHeight, outBmpPath, outW, outH);
      });
  Hyphenator::setPreferredLanguage(epub->getLanguage());
  success = visitor.parseAndBuildPages();

  SdMan.remove(tmpHtmlPath.c_str());
  if (!success) {
    Serial.printf("[%lu] [SCT] Failed to parse XML and build pages\n", millis());
    file.close();
    SdMan.remove(filePath.c_str());
    return false;
  }

  const uint32_t lutOffset = file.position();
  bool hasFailedLutRecords = false;
  // Write LUT
  for (const uint32_t& pos : lut) {
    if (pos == 0) {
      hasFailedLutRecords = true;
      break;
    }
    serialization::writePod(file, pos);
  }

  if (hasFailedLutRecords) {
    Serial.printf("[%lu] [SCT] Failed to write LUT due to invalid page positions\n", millis());
    file.close();
    SdMan.remove(filePath.c_str());
    return false;
  }

  // Go back and write LUT offset
  file.seek(HEADER_SIZE - sizeof(uint32_t) - sizeof(pageCount));
  serialization::writePod(file, pageCount);
  serialization::writePod(file, lutOffset);
  file.close();
  return true;
}

std::unique_ptr<Page> Section::loadPageFromSectionFile() {
  if (!SdMan.openFileForRead("SCT", filePath, file)) {
    return nullptr;
  }

  file.seek(HEADER_SIZE - sizeof(uint32_t));
  uint32_t lutOffset;
  serialization::readPod(file, lutOffset);
  file.seek(lutOffset + sizeof(uint32_t) * currentPage);
  uint32_t pagePos;
  serialization::readPod(file, pagePos);
  file.seek(pagePos);

  auto page = Page::deserialize(file);
  file.close();
  return page;
}
