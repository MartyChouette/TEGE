#include "Enjin/Assets/SplatLoader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

// stb_image's zlib inflate (compiled once in VulkanImage.cpp; SWFLoader uses
// the same entry points for Flash decompression)
#include "stb_image.h"

namespace Enjin {
namespace Assets {

namespace {

constexpr f32 kShC0 = 0.28209479177387814f;   // SH band-0 basis constant

f32 Sigmoid(f32 v) { return 1.0f / (1.0f + std::exp(-v)); }

// COLMAP (Y down, Z forward) -> engine (Y up, Z toward viewer): conjugate by
// F = diag(1,-1,-1), a 180-degree rotation about X. Positions flip in place;
// rotations go through the matrix so the quaternion sign conventions can't
// bite: R' = F R F, re-extracted with the engine's own FromMatrix.
void FlipSplatYZ(SplatInstance& s) {
    s.py = -s.py;
    s.pz = -s.pz;
    Math::Quaternion q(s.qx, s.qy, s.qz, s.qw);
    Math::Matrix4 m = q.ToMatrix();
    auto M = [&m](usize r, usize c) -> f32& { return m.m[c * 4 + r]; };
    // F R F: negate row 1 and 2, then column 1 and 2 (net: flip signs where
    // exactly one index is in {1,2})
    for (usize c = 0; c < 3; ++c) { M(1, c) = -M(1, c); M(2, c) = -M(2, c); }
    for (usize r = 0; r < 3; ++r) { M(r, 1) = -M(r, 1); M(r, 2) = -M(r, 2); }
    Math::Quaternion fq = Math::Quaternion::FromMatrix(m).Normalized();
    s.qx = fq.x; s.qy = fq.y; s.qz = fq.z; s.qw = fq.w;
}

} // namespace

SplatData SplatLoader::LoadFromFile(const std::string& path, u32 maxSplats, bool flipYZ) {
    SplatData out;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        out.error = "could not open '" + path + "'";
        return out;
    }
    std::vector<u8> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.size() < 16) {
        out.error = "file too small to be a splat file";
        return out;
    }

    // .spz = gzip magic; .ply = "ply\n" text magic
    if (bytes[0] == 0x1f && bytes[1] == 0x8b) {
        return ParseSpz(bytes.data(), bytes.size(), maxSplats, flipYZ);
    }
    if (std::memcmp(bytes.data(), "ply", 3) == 0) {
        return ParsePly(bytes.data(), bytes.size(), maxSplats, flipYZ);
    }
    out.error = "unrecognized splat format (expected 3DGS .ply or .spz)";
    return out;
}

// ---------------------------------------------------------------------------
// .ply (INRIA 3D Gaussian Splatting layout)
// ---------------------------------------------------------------------------

SplatData SplatLoader::ParsePly(const u8* data, usize size, u32 maxSplats, bool flipYZ) {
    SplatData out;

    // Header is ASCII lines up to "end_header"
    const char* text = reinterpret_cast<const char*>(data);
    usize headerEnd = 0;
    {
        const char* marker = "end_header";
        for (usize i = 0; i + 10 < size && i < 65536; ++i) {
            if (std::memcmp(text + i, marker, 10) == 0) {
                headerEnd = i + 10;
                while (headerEnd < size && text[headerEnd] != '\n') ++headerEnd;
                ++headerEnd;   // past the newline
                break;
            }
        }
        if (headerEnd == 0) { out.error = "ply: no end_header"; return out; }
    }

    std::istringstream header(std::string(text, headerEnd));
    std::string line;
    u64 vertexCount = 0;
    bool binaryLE = false;
    bool inVertexElement = false;
    struct Prop { std::string name; usize offset; usize size; };
    std::vector<Prop> props;
    usize stride = 0;

    auto propSize = [](const std::string& t) -> usize {
        if (t == "float" || t == "float32" || t == "int" || t == "int32" || t == "uint" || t == "uint32") return 4;
        if (t == "double" || t == "float64") return 8;
        if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
        if (t == "char" || t == "uchar" || t == "int8" || t == "uint8") return 1;
        return 0;
    };

    while (std::getline(header, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string tok;
        ls >> tok;
        if (tok == "format") {
            std::string fmt; ls >> fmt;
            binaryLE = (fmt == "binary_little_endian");
        } else if (tok == "element") {
            std::string ename; u64 count = 0;
            ls >> ename >> count;
            inVertexElement = (ename == "vertex");
            if (inVertexElement) vertexCount = count;
        } else if (tok == "property" && inVertexElement) {
            std::string type, pname;
            ls >> type >> pname;
            if (type == "list") { out.error = "ply: list properties are not a splat layout"; return out; }
            usize sz = propSize(type);
            if (sz == 0) { out.error = "ply: unknown property type " + type; return out; }
            props.push_back({pname, stride, sz});
            stride += sz;
        }
    }

    if (!binaryLE) { out.error = "ply: only binary_little_endian is supported"; return out; }
    if (vertexCount == 0 || stride == 0) { out.error = "ply: no vertex element"; return out; }

    auto find = [&props](const char* n) -> const Prop* {
        for (const auto& p : props) if (p.name == n) return &p;
        return nullptr;
    };

    // A splat ply is identified by its gaussian properties; a mesh ply (plain
    // x/y/z + faces) gets a clear message instead of garbage splats.
    const Prop* px = find("x"); const Prop* py = find("y"); const Prop* pz = find("z");
    const Prop* dc0 = find("f_dc_0"); const Prop* dc1 = find("f_dc_1"); const Prop* dc2 = find("f_dc_2");
    const Prop* pop = find("opacity");
    const Prop* s0 = find("scale_0"); const Prop* s1 = find("scale_1"); const Prop* s2 = find("scale_2");
    const Prop* r0 = find("rot_0"); const Prop* r1 = find("rot_1"); const Prop* r2 = find("rot_2"); const Prop* r3 = find("rot_3");
    if (!dc0 || !pop || !s0 || !r0) {
        out.error = "ply: not a 3D Gaussian splat file (missing f_dc_0/opacity/scale_0/rot_0 - "
                    "a mesh .ply imports through the normal model importer)";
        return out;
    }
    if (!px || !py || !pz || !dc1 || !dc2 || !s1 || !s2 || !r1 || !r2 || !r3) {
        out.error = "ply: incomplete splat property set";
        return out;
    }

    const u8* body = data + headerEnd;
    usize bodySize = size - headerEnd;
    u64 available = static_cast<u64>(bodySize / stride);
    u64 count = std::min<u64>(vertexCount, available);
    if (count < vertexCount) {
        ENJIN_LOG_WARN(Asset, "SplatLoader: ply truncated (%llu of %llu splats present)",
                       (unsigned long long)count, (unsigned long long)vertexCount);
    }
    u64 kept = std::min<u64>(count, maxSplats);
    if (kept < count) {
        ENJIN_LOG_WARN(Asset, "SplatLoader: capping %llu splats at %u (maxSplats)",
                       (unsigned long long)count, maxSplats);
    }

    auto readF32 = [](const u8* base, const Prop* p) -> f32 {
        // splat plys store everything as float32; tolerate float64 defensively
        if (p->size == 4) { f32 v; std::memcpy(&v, base + p->offset, 4); return v; }
        if (p->size == 8) { double v; std::memcpy(&v, base + p->offset, 8); return static_cast<f32>(v); }
        return 0.0f;
    };

    out.splats.reserve(static_cast<usize>(kept));
    for (u64 i = 0; i < kept; ++i) {
        const u8* rec = body + i * stride;
        SplatInstance s{};
        s.px = readF32(rec, px); s.py = readF32(rec, py); s.pz = readF32(rec, pz);
        s.opacity = Sigmoid(readF32(rec, pop));
        s.r = Math::Clamp(0.5f + kShC0 * readF32(rec, dc0), 0.0f, 1.0f);
        s.g = Math::Clamp(0.5f + kShC0 * readF32(rec, dc1), 0.0f, 1.0f);
        s.b = Math::Clamp(0.5f + kShC0 * readF32(rec, dc2), 0.0f, 1.0f);
        s.sx = std::exp(readF32(rec, s0));
        s.sy = std::exp(readF32(rec, s1));
        s.sz = std::exp(readF32(rec, s2));
        // INRIA stores rot_0 as the real part: (w, x, y, z)
        f32 qw = readF32(rec, r0), qx = readF32(rec, r1), qy = readF32(rec, r2), qz = readF32(rec, r3);
        f32 len = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
        if (len < 1e-6f) { qw = 1.0f; qx = qy = qz = 0.0f; len = 1.0f; }
        s.qx = qx / len; s.qy = qy / len; s.qz = qz / len; s.qw = qw / len;
        if (flipYZ) FlipSplatYZ(s);
        out.splats.push_back(s);
    }

    ENJIN_LOG_INFO(Asset, "SplatLoader: loaded %zu splats from ply", out.splats.size());
    return out;
}

// ---------------------------------------------------------------------------
// .spz (Niantic) - gzip around a packed little-endian payload
// ---------------------------------------------------------------------------

SplatData SplatLoader::ParseSpz(const u8* data, usize size, u32 maxSplats, bool flipYZ) {
    SplatData out;
    // Minimal gzip framing: 10-byte header (+optional FEXTRA/FNAME/FCOMMENT),
    // deflate stream, 8-byte trailer (crc32 + isize). stbi inflates the raw
    // deflate stream; isize gives the output size (mod 2^32).
    if (size < 18 || data[0] != 0x1f || data[1] != 0x8b || data[2] != 8) {
        out.error = "spz: not a gzip stream";
        return out;
    }
    u8 flags = data[3];
    usize pos = 10;
    if (flags & 0x04) {   // FEXTRA
        if (pos + 2 > size) { out.error = "spz: truncated gzip header"; return out; }
        u16 xlen; std::memcpy(&xlen, data + pos, 2);
        pos += 2 + xlen;
    }
    if (flags & 0x08) { while (pos < size && data[pos] != 0) ++pos; ++pos; }   // FNAME
    if (flags & 0x10) { while (pos < size && data[pos] != 0) ++pos; ++pos; }   // FCOMMENT
    if (flags & 0x02) pos += 2;                                                 // FHCRC
    if (pos + 8 >= size) { out.error = "spz: truncated gzip stream"; return out; }

    int outLen = 0;
    char* inflated = stbi_zlib_decode_noheader_malloc(
        reinterpret_cast<const char*>(data + pos), static_cast<int>(size - pos - 8), &outLen);
    if (!inflated || outLen <= 0) {
        if (inflated) free(inflated);
        out.error = "spz: gzip decompression failed";
        return out;
    }
    out = ParseSpzRaw(reinterpret_cast<const u8*>(inflated), static_cast<usize>(outLen),
                      maxSplats, flipYZ);
    free(inflated);
    return out;
}

SplatData SplatLoader::ParseSpzRaw(const u8* data, usize size, u32 maxSplats, bool flipYZ) {
    SplatData out;
    // Header (16 bytes): magic 'NGSP', version, numPoints, shDegree,
    // fractionalBits, flags, reserved
    if (size < 16) { out.error = "spz: payload too small"; return out; }
    u32 magic, version, numPoints;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&numPoints, data + 8, 4);
    u8 shDegree = data[12];
    u8 fractionalBits = data[13];
    if (magic != 0x5053474e) { out.error = "spz: bad magic"; return out; }
    if (version < 1 || version > 3) { out.error = "spz: unsupported version " + std::to_string(version); return out; }
    if (shDegree > 3) { out.error = "spz: bad SH degree"; return out; }

    // Section layout: positions (int24 x3), alphas (u8), colors (u8 x3),
    // scales (u8 x3), rotations (u8 x3), then SH rest (skipped)
    u64 n = numPoints;
    u64 need = 16 + n * (9 + 1 + 3 + 3 + 3);
    if (size < need) { out.error = "spz: payload truncated"; return out; }
    const u8* positions = data + 16;
    const u8* alphas    = positions + n * 9;
    const u8* colors    = alphas + n;
    const u8* scales    = colors + n * 3;
    const u8* rotations = scales + n * 3;

    u64 kept = std::min<u64>(n, maxSplats);
    if (kept < n) {
        ENJIN_LOG_WARN(Asset, "SplatLoader: capping %llu splats at %u (maxSplats)",
                       (unsigned long long)n, maxSplats);
    }
    f32 posScale = 1.0f / static_cast<f32>(1 << fractionalBits);

    auto int24 = [](const u8* p) -> i32 {
        i32 v = p[0] | (p[1] << 8) | (p[2] << 16);
        if (v & 0x800000) v |= ~0xFFFFFF;   // sign-extend
        return v;
    };

    out.splats.reserve(static_cast<usize>(kept));
    for (u64 i = 0; i < kept; ++i) {
        SplatInstance s{};
        const u8* pp = positions + i * 9;
        s.px = int24(pp + 0) * posScale;
        s.py = int24(pp + 3) * posScale;
        s.pz = int24(pp + 6) * posScale;
        s.opacity = alphas[i] / 255.0f;
        // colors quantize the SH DC band: stored = sh * 0.15 * 255 + 127.5
        for (int c = 0; c < 3; ++c) {
            f32 sh = (colors[i * 3 + c] / 255.0f - 0.5f) / 0.15f;
            f32 v = Math::Clamp(0.5f + kShC0 * sh, 0.0f, 1.0f);
            if (c == 0) s.r = v; else if (c == 1) s.g = v; else s.b = v;
        }
        // scales: log-encoded, v/16 - 10
        s.sx = std::exp(scales[i * 3 + 0] / 16.0f - 10.0f);
        s.sy = std::exp(scales[i * 3 + 1] / 16.0f - 10.0f);
        s.sz = std::exp(scales[i * 3 + 2] / 16.0f - 10.0f);
        // rotations: xyz components quantized to u8, w reconstructed
        f32 qx = rotations[i * 3 + 0] / 127.5f - 1.0f;
        f32 qy = rotations[i * 3 + 1] / 127.5f - 1.0f;
        f32 qz = rotations[i * 3 + 2] / 127.5f - 1.0f;
        f32 ww = 1.0f - (qx * qx + qy * qy + qz * qz);
        f32 qw = ww > 0.0f ? std::sqrt(ww) : 0.0f;
        f32 len = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
        if (len < 1e-6f) { qw = 1.0f; qx = qy = qz = 0.0f; len = 1.0f; }
        s.qx = qx / len; s.qy = qy / len; s.qz = qz / len; s.qw = qw / len;
        if (flipYZ) FlipSplatYZ(s);
        out.splats.push_back(s);
    }

    ENJIN_LOG_INFO(Asset, "SplatLoader: loaded %zu splats from spz (SH degree %u ignored beyond DC)",
                   out.splats.size(), shDegree);
    return out;
}

} // namespace Assets
} // namespace Enjin
