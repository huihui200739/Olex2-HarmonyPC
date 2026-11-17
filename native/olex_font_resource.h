#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace olex2::harmony {
// Original MetaFont resource syntax: #relative-file:nr__18 / frb_18 / nib_18.
// The file contains the original "ofnt" prefix followed by an ordinary ZIP.
struct OlexResourceFontDescription {
  std::string file, style;
  unsigned points=0;
  bool fixed=false, bold=false, italic=false;
  std::string Entry(unsigned pointSize) const;
};
struct OlexResourceFontData {
  uint16_t width=0,height=0;
  uint16_t compression=0;
  uint32_t crc=0;
  std::string entry;
  // Original TEBitArray order: glyph, top-down row, column; LSB-first bytes.
  std::vector<unsigned char> bits;
};
OlexResourceFontDescription ParseOlexResourceFontDescription(const std::string&);
// Only the requested entry is decompressed and CRC-checked. No files extracted.
OlexResourceFontData DecodeOlexFontResource(const std::vector<unsigned char>&,const std::string& entry);
OlexResourceFontData LoadOlexFontResource(const std::string& path,const std::string& entry);
}
