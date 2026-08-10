#pragma once
// png_write.h — minimal, self-contained PNG encoder (8-bit RGB, no alpha).
// No external dependencies (no libpng/zlib): DEFLATE data is written as
// uncompressed blocks, which is valid per the zlib/DEFLATE spec and keeps
// this file dependency-free at the cost of larger output files.
//
// Public API:
//   png_write_rgb8(path, pixels, width, height)
//   pixels : row-major packed R G B bytes, no padding, no alpha
//             total = width * height * 3 bytes

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace png_detail {

// ---- CRC-32 (ISO 3309, reflected polynomial 0xEDB88320) --------------------
// Builds the 256-entry table once and reuses it.
static const uint32_t* crc32_table() {
    static uint32_t T[256] = {};
    static bool     ready  = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            T[i] = c;
        }
        ready = true;
    }
    return T;
}

static inline uint32_t crc32_update(uint32_t crc,
                                    const uint8_t* data, size_t len) {
    const uint32_t* T = crc32_table();
    for (size_t i = 0; i < len; ++i)
        crc = T[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

// Full CRC-32 of a buffer.
static inline uint32_t crc32(const uint8_t* data, size_t len) {
    return crc32_update(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

// ---- Adler-32 (used as the zlib checksum over the raw data) ----------------
static inline uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t s1 = 1u, s2 = 0u;
    for (size_t i = 0; i < len; ++i) {
        s1 = (s1 + data[i]) % 65521u;
        s2 = (s2 + s1)      % 65521u;
    }
    return (s2 << 16) | s1;
}

// ---- Little-endian / big-endian helpers ------------------------------------
static inline void put_u32be(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFFu; p[1] = (v >> 16) & 0xFFu;
    p[2] = (v >>  8) & 0xFFu; p[3] =  v        & 0xFFu;
}
static inline void put_u16le(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFFu; p[1] = (v >> 8) & 0xFFu;
}

// ---- Wrap raw bytes in a zlib stream using uncompressed DEFLATE blocks -----
// Header: CMF=0x78, FLG=0x01  ->  0x7801, (0x78*256+0x01) % 31 = 0 (valid)
// Each DEFLATE block carries at most 65535 bytes of uncompressed payload.
// Trailer: Adler-32 of the original uncompressed data, big-endian.
static std::vector<uint8_t> make_zlib(const uint8_t* raw, size_t len) {
    std::vector<uint8_t> z;
    z.reserve(2 + (len / 65535 + 1) * 5 + len + 4);

    z.push_back(0x78u); // CMF: deflate, 32 KB window
    z.push_back(0x01u); // FLG: FCHECK=1, no dict, default compression

    // DEFLATE uncompressed blocks (BTYPE = 00)
    const size_t MAX_BLOCK = 65535;
    size_t pos = 0;
    do {
        size_t   blk  = std::min(MAX_BLOCK, len - pos);
        bool     last = (pos + blk >= len);
        uint8_t  hdr[5];
        hdr[0] = last ? 0x01u : 0x00u;          // BFINAL | (BTYPE=00 << 1)
        put_u16le(hdr + 1, (uint16_t)blk);       // LEN
        put_u16le(hdr + 3, (uint16_t)(~(uint16_t)blk)); // NLEN = one's complement
        z.insert(z.end(), hdr, hdr + 5);
        z.insert(z.end(), raw + pos, raw + pos + blk);
        pos += blk;
    } while (pos < len);

    // Adler-32 of the uncompressed content, big-endian
    uint8_t adl[4];
    put_u32be(adl, adler32(raw, len));
    z.insert(z.end(), adl, adl + 4);
    return z;
}

} // namespace png_detail

// ============================================================================
// png_write_rgb8 — write an 8-bit RGB PNG file.
//
//   path   : output file path
//   pixels : row-major R G B packed bytes, no alpha, no padding
//             layout: pixels[ (y*width + x)*3 + {0,1,2} ] = R,G,B
//   width, height : image dimensions in pixels
// ============================================================================
inline void png_write_rgb8(const std::string& path,
                            const uint8_t*     pixels,
                            int width, int height)
{
    using namespace png_detail;

    // Build PNG image data: prepend filter byte 0 (None) to each scanline.
    size_t row_bytes = (size_t)width * 3;
    size_t raw_len   = (size_t)height * (1 + row_bytes);
    std::vector<uint8_t> raw(raw_len);
    for (int y = 0; y < height; ++y) {
        uint8_t* dst = raw.data() + (size_t)y * (1 + row_bytes);
        dst[0] = 0u; // filter type = None
        std::memcpy(dst + 1, pixels + (size_t)y * row_bytes, row_bytes);
    }

    // Compress (uncompressed DEFLATE — fast write, valid output)
    std::vector<uint8_t> idat = make_zlib(raw.data(), raw_len);

    // Open output file
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open PNG output: " + path);

    // Helper: write one PNG chunk [length][type][data][CRC]
    auto write_chunk = [&](const char* type,
                            const uint8_t* data, uint32_t dlen) {
        uint8_t lb[4]; put_u32be(lb, dlen);
        f.write((char*)lb, 4);
        f.write(type, 4);
        if (dlen > 0) f.write((char*)data, dlen);
        // CRC covers type field + data field
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32_update(crc, (const uint8_t*)type, 4);
        if (dlen > 0) crc = crc32_update(crc, data, dlen);
        crc ^= 0xFFFFFFFFu;
        uint8_t cb[4]; put_u32be(cb, crc);
        f.write((char*)cb, 4);
    };

    // PNG file signature
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    f.write((char*)sig, 8);

    // IHDR chunk (13 bytes)
    uint8_t ihdr[13] = {};
    put_u32be(ihdr + 0, (uint32_t)width);
    put_u32be(ihdr + 4, (uint32_t)height);
    ihdr[8]  = 8u; // bit depth per channel
    ihdr[9]  = 2u; // colour type: truecolour (RGB)
    ihdr[10] = 0u; // compression: deflate/inflate (only method defined)
    ihdr[11] = 0u; // filter method: adaptive (type 0 used for all rows here)
    ihdr[12] = 0u; // interlace: none
    write_chunk("IHDR", ihdr, 13);

    // IDAT chunk (compressed image data)
    write_chunk("IDAT", idat.data(), (uint32_t)idat.size());

    // IEND chunk (empty — marks end of PNG file)
    write_chunk("IEND", nullptr, 0);
}
