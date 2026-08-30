#include "Enjin/Editor/GifRecorder.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

namespace {

// Bit-packing writer for LZW code streams (LSB-first per the GIF spec),
// flushing into 255-byte sub-blocks.
struct BitPacker {
    std::vector<u8> bytes;
    u32 acc = 0;
    u32 bits = 0;
    void Put(u32 code, u32 width) {
        acc |= code << bits;
        bits += width;
        while (bits >= 8) {
            bytes.push_back(static_cast<u8>(acc & 0xFF));
            acc >>= 8;
            bits -= 8;
        }
    }
    void Flush() {
        if (bits > 0) { bytes.push_back(static_cast<u8>(acc & 0xFF)); acc = 0; bits = 0; }
    }
};

void WriteU16(std::ofstream& f, u16 v) {
    u8 b[2] = { static_cast<u8>(v & 0xFF), static_cast<u8>(v >> 8) };
    f.write(reinterpret_cast<const char*>(b), 2);
}

// GIF-standard LZW with an 8-bit minimum code size (palette is always 256
// entries here). Dictionary as a flat prefix-tree table: 4096 codes max,
// children looked up via [code][nextByte] hash map replaced by a 4096x256
// array being too big - use open addressing on (prefix, byte).
void LzwEncode(std::ofstream& f, const std::vector<u8>& indices) {
    constexpr u32 kMinCodeSize = 8;
    constexpr u32 kClear = 1u << kMinCodeSize;        // 256
    constexpr u32 kEnd = kClear + 1;                  // 257
    constexpr u32 kMaxCode = 4096;

    // Hash table: key = (prefixCode << 8) | byte, value = code. Sized to
    // comfortably hold 4096 entries.
    constexpr u32 kHashSize = 1u << 13;               // 8192 slots
    static thread_local std::vector<u32> keys, vals;
    keys.assign(kHashSize, 0xFFFFFFFFu);
    vals.assign(kHashSize, 0);
    auto hash = [](u32 key) { return (key * 2654435761u) & (kHashSize - 1); };

    BitPacker packer;
    u32 codeSize = kMinCodeSize + 1;
    u32 nextCode = kEnd + 1;
    packer.Put(kClear, codeSize);

    u32 prefix = 0xFFFFFFFFu;
    for (u8 idx : indices) {
        if (prefix == 0xFFFFFFFFu) { prefix = idx; continue; }
        u32 key = (prefix << 8) | idx;
        u32 slot = hash(key);
        bool found = false;
        while (keys[slot] != 0xFFFFFFFFu) {
            if (keys[slot] == key) { prefix = vals[slot]; found = true; break; }
            slot = (slot + 1) & (kHashSize - 1);
        }
        if (found) continue;

        packer.Put(prefix, codeSize);
        if (nextCode < kMaxCode) {
            keys[slot] = key;
            vals[slot] = nextCode;
            if (nextCode == (1u << codeSize) && codeSize < 12) ++codeSize;
            ++nextCode;
        } else {
            // Dictionary full: clear and restart.
            packer.Put(kClear, codeSize);
            keys.assign(kHashSize, 0xFFFFFFFFu);
            codeSize = kMinCodeSize + 1;
            nextCode = kEnd + 1;
        }
        prefix = idx;
    }
    if (prefix != 0xFFFFFFFFu) packer.Put(prefix, codeSize);
    packer.Put(kEnd, codeSize);
    packer.Flush();

    // Minimum code size byte, then 255-byte sub-blocks, then terminator.
    u8 mcs = static_cast<u8>(kMinCodeSize);
    f.write(reinterpret_cast<const char*>(&mcs), 1);
    usize off = 0;
    while (off < packer.bytes.size()) {
        u8 n = static_cast<u8>(std::min<usize>(255, packer.bytes.size() - off));
        f.write(reinterpret_cast<const char*>(&n), 1);
        f.write(reinterpret_cast<const char*>(packer.bytes.data() + off), n);
        off += n;
    }
    u8 zero = 0;
    f.write(reinterpret_cast<const char*>(&zero), 1);
}

} // namespace

bool GifRecorder::Start(const std::string& path, u32 width, u32 height, u32 downscaleShift) {
    Stop();
    if (width == 0 || height == 0) return false;
    m_File.open(path, std::ios::binary);
    if (!m_File.is_open()) {
        ENJIN_LOG_ERROR(Editor, "GIF: cannot open %s", path.c_str());
        return false;
    }
    m_Path = path;
    m_SrcW = width;
    m_SrcH = height;
    m_Shift = std::min(downscaleShift, 3u);
    m_W = std::max(1u, width >> m_Shift);
    m_H = std::max(1u, height >> m_Shift);
    m_Frames = 0;

    // Header + logical screen descriptor (no global color table).
    m_File.write("GIF89a", 6);
    WriteU16(m_File, static_cast<u16>(m_W));
    WriteU16(m_File, static_cast<u16>(m_H));
    u8 lsd[3] = { 0x00, 0x00, 0x00 };
    m_File.write(reinterpret_cast<const char*>(lsd), 3);

    // NETSCAPE2.0 loop-forever extension.
    const u8 loop[] = { 0x21, 0xFF, 0x0B,
                        'N','E','T','S','C','A','P','E','2','.','0',
                        0x03, 0x01, 0x00, 0x00, 0x00 };
    m_File.write(reinterpret_cast<const char*>(loop), sizeof(loop));
    return true;
}

void GifRecorder::AddFrame(const u8* rgba, f32 delayMs) {
    if (!m_File.is_open() || !rgba) return;

    // Downscale by box filter (2^shift x 2^shift average).
    static thread_local std::vector<u8> small;
    small.resize(static_cast<usize>(m_W) * m_H * 3);
    const u32 step = 1u << m_Shift;
    for (u32 y = 0; y < m_H; ++y) {
        for (u32 x = 0; x < m_W; ++x) {
            u32 r = 0, g = 0, b = 0, cnt = 0;
            for (u32 dy = 0; dy < step; ++dy) {
                u32 sy = y * step + dy;
                if (sy >= m_SrcH) break;
                const u8* row = rgba + (static_cast<usize>(sy) * m_SrcW) * 4;
                for (u32 dx = 0; dx < step; ++dx) {
                    u32 sx = x * step + dx;
                    if (sx >= m_SrcW) break;
                    r += row[sx * 4 + 0]; g += row[sx * 4 + 1]; b += row[sx * 4 + 2];
                    ++cnt;
                }
            }
            if (cnt == 0) cnt = 1;
            u8* out = &small[(static_cast<usize>(y) * m_W + x) * 3];
            out[0] = static_cast<u8>(r / cnt);
            out[1] = static_cast<u8>(g / cnt);
            out[2] = static_cast<u8>(b / cnt);
        }
    }

    // Per-frame palette: popularity in a 32x32x32 histogram - the 256 most
    // used 5-bit color cells become the palette, every cell maps to its
    // nearest palette entry (so no per-pixel nearest-neighbor search).
    constexpr u32 kCells = 32 * 32 * 32;
    static thread_local std::vector<u32> hist;
    hist.assign(kCells, 0);
    const usize pixCount = static_cast<usize>(m_W) * m_H;
    auto cellOf = [](const u8* p) {
        return ((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3);
    };
    for (usize i = 0; i < pixCount; ++i) hist[cellOf(&small[i * 3])]++;

    // Top 256 cells by count.
    static thread_local std::vector<u32> order;
    order.resize(kCells);
    for (u32 i = 0; i < kCells; ++i) order[i] = i;
    std::partial_sort(order.begin(), order.begin() + 256, order.end(),
                      [&](u32 a, u32 b) { return hist[a] > hist[b]; });

    u8 palette[256 * 3];
    for (u32 i = 0; i < 256; ++i) {
        u32 cell = order[i];
        // Cell center: 5-bit channel scaled back to 8-bit.
        palette[i * 3 + 0] = static_cast<u8>((((cell >> 10) & 31) << 3) | 4);
        palette[i * 3 + 1] = static_cast<u8>((((cell >> 5) & 31) << 3) | 4);
        palette[i * 3 + 2] = static_cast<u8>((((cell >> 0) & 31) << 3) | 4);
    }

    // Cell -> palette index map: used cells map to their own entry when in
    // the top 256, otherwise to the nearest palette color (computed lazily).
    static thread_local std::vector<i16> cellMap;
    cellMap.assign(kCells, -1);
    for (u32 i = 0; i < 256; ++i) cellMap[order[i]] = static_cast<i16>(i);

    static thread_local std::vector<u8> indices;
    indices.resize(pixCount);
    for (usize i = 0; i < pixCount; ++i) {
        u32 cell = cellOf(&small[i * 3]);
        i16 m = cellMap[cell];
        if (m < 0) {
            // Lazy nearest-palette for cells that missed the cut.
            const u8* p = &small[i * 3];
            u32 best = 0, bestD = 0xFFFFFFFFu;
            for (u32 k = 0; k < 256; ++k) {
                i32 dr = p[0] - palette[k * 3 + 0];
                i32 dg = p[1] - palette[k * 3 + 1];
                i32 db = p[2] - palette[k * 3 + 2];
                u32 dsq = static_cast<u32>(dr * dr + dg * dg * 2 + db * db);
                if (dsq < bestD) { bestD = dsq; best = k; }
            }
            m = static_cast<i16>(best);
            cellMap[cell] = m;
        }
        indices[i] = static_cast<u8>(m);
    }

    WriteFrame(indices, palette, delayMs);
    ++m_Frames;
}

void GifRecorder::WriteFrame(const std::vector<u8>& indices, const u8* palette, f32 delayMs) {
    // Graphic control extension: delay in 10ms units.
    u16 delay = static_cast<u16>(std::clamp(delayMs / 10.0f, 2.0f, 6553.0f));
    u8 gce[8] = { 0x21, 0xF9, 0x04, 0x00,
                  static_cast<u8>(delay & 0xFF), static_cast<u8>(delay >> 8),
                  0x00, 0x00 };
    m_File.write(reinterpret_cast<const char*>(gce), 8);

    // Image descriptor with a 256-entry local color table.
    u8 desc[10] = { 0x2C, 0, 0, 0, 0,
                    static_cast<u8>(m_W & 0xFF), static_cast<u8>(m_W >> 8),
                    static_cast<u8>(m_H & 0xFF), static_cast<u8>(m_H >> 8),
                    0x87 };   // local table, 2^(7+1) = 256 entries
    m_File.write(reinterpret_cast<const char*>(desc), 10);
    m_File.write(reinterpret_cast<const char*>(palette), 256 * 3);

    LzwEncode(m_File, indices);
}

void GifRecorder::Stop() {
    if (!m_File.is_open()) return;
    u8 trailer = 0x3B;
    m_File.write(reinterpret_cast<const char*>(&trailer), 1);
    m_File.close();
    ENJIN_LOG_INFO(Editor, "GIF: wrote %u frames (%ux%u) -> %s",
                   m_Frames, m_W, m_H, m_Path.c_str());
}

} // namespace Editor
} // namespace Enjin
