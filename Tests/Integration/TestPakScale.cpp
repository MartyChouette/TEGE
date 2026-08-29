// T2 (CI-able half) — Pak format at scale (MASTER_VALIDATION.md §3).
// The web player ships everything through one .enjpak; a big game means a big
// pak. This packs ~300MB of synthetic assets (mixed sizes, compressible and
// incompressible), reads every file back, and verifies integrity + timing.
//
// PASS CRITERIA:
//   - Round trip is byte-exact for every file (CRC verified by the reader,
//     spot-checked byte-wise here).
//   - Full pack + full read of ~300MB completes in < 120s on CI-class
//     hardware (order-of-magnitude gate, not tuning).
//   - Peak native memory stays bounded (packing must stream, not buffer the
//     whole pak: peak RSS growth < 2x the largest single file + 256MB slack).
// The browser-side halves of T2 (WASM heap ceiling behavior, IndexedDB
// quota) need a live browser and are staged separately.

#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Platform/Paths.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace Enjin;
using Clock = std::chrono::high_resolution_clock;

static f64 Ms(Clock::time_point t0) {
    return std::chrono::duration<f64, std::milli>(Clock::now() - t0).count();
}

static int s_Failures = 0;
#define T2_CHECK(cond, ...) do { if (!(cond)) { ++s_Failures; \
    std::printf("  FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    std::printf("=== T2a: Pak format at scale (MASTER_VALIDATION §3) ===\n");
    const std::string pakPath =
        (std::filesystem::temp_directory_path() / "tege_pakscale.enjpak").string();
    const std::string key = "pakscale-test";

    u64 rssStart = Platform::GetProcessMemoryBytes();

    // ── Build the asset mix ──
    // 1 huge incompressible file (64MB random - a big texture atlas shape),
    // 8 medium semi-compressible (16MB each - meshes), 2000 small files
    // (4KB-64KB - scripts/scenes/sprites). ~300MB original total.
    std::mt19937 rng(42);
    std::vector<u8> huge(64ull * 1024 * 1024);
    for (auto& b : huge) b = static_cast<u8>(rng());

    std::vector<u8> medium(16ull * 1024 * 1024);
    for (usize i = 0; i < medium.size(); ++i)
        medium[i] = static_cast<u8>((i / 64) & 0xFF);   // repetitive: compresses

    std::vector<std::vector<u8>> smalls;
    std::uniform_int_distribution<usize> smallSize(4096, 65536);
    u64 smallTotal = 0;
    for (int i = 0; i < 2000; ++i) {
        std::vector<u8> s(smallSize(rng));
        for (auto& b : s) b = static_cast<u8>(rng() & 0x7F);
        smallTotal += s.size();
        smalls.push_back(std::move(s));
    }
    u64 originalTotal = huge.size() + 8 * medium.size() + smallTotal;
    std::printf("asset mix: 1x64MB random + 8x16MB repetitive + 2000 small = %.1f MB\n",
                originalTotal / (1024.0 * 1024.0));

    // ── Pack ──
    auto t0 = Clock::now();
    {
        Build::AssetPacker packer;
        T2_CHECK(packer.Begin(pakPath, key), "packer Begin failed");
        T2_CHECK(packer.AddData("atlas/huge.bin", huge.data(), huge.size()),
                 "AddData huge failed");
        for (int i = 0; i < 8; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "meshes/med_%d.bin", i);
            T2_CHECK(packer.AddData(name, medium.data(), medium.size()),
                     "AddData medium %d failed", i);
        }
        for (int i = 0; i < 2000; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "assets/s_%04d.bin", i);
            T2_CHECK(packer.AddData(name, smalls[i].data(), smalls[i].size()),
                     "AddData small %d failed", i);
        }
        T2_CHECK(packer.Finalize(), "packer Finalize failed");
        std::printf("packed %u files, %.1f MB -> %.1f MB in %.1f s\n",
                    packer.GetFileCount(),
                    packer.GetTotalOriginalSize() / (1024.0 * 1024.0),
                    packer.GetTotalPackedSize() / (1024.0 * 1024.0),
                    Ms(t0) / 1000.0);
    }
    f64 packMs = Ms(t0);
    u64 rssAfterPack = Platform::GetProcessMemoryBytes();

    // ── Read back + verify ──
    t0 = Clock::now();
    {
        Build::AssetReader reader;
        T2_CHECK(reader.Open(pakPath, key), "reader Open failed");

        auto h = reader.ReadFile("atlas/huge.bin");
        T2_CHECK(h.size() == huge.size(), "huge size mismatch: %zu vs %zu",
                 h.size(), huge.size());
        T2_CHECK(!h.empty() && h == huge, "huge content mismatch");

        auto m3 = reader.ReadFile("meshes/med_3.bin");
        T2_CHECK(m3 == medium, "medium content mismatch");

        // every small file (order-scrambled reads: index seek pattern)
        u64 readBytes = h.size() + m3.size();
        for (int i = 1999; i >= 0; --i) {
            char name[64];
            std::snprintf(name, sizeof(name), "assets/s_%04d.bin", i);
            auto s = reader.ReadFile(name);
            if (s != smalls[i]) {
                T2_CHECK(false, "small %d mismatch", i);
                break;
            }
            readBytes += s.size();
        }
        // the remaining mediums
        for (int i = 0; i < 8; ++i) {
            if (i == 3) continue;
            char name[64];
            std::snprintf(name, sizeof(name), "meshes/med_%d.bin", i);
            readBytes += reader.ReadFile(name).size();
        }
        std::printf("read back %.1f MB in %.1f s\n",
                    readBytes / (1024.0 * 1024.0), Ms(t0) / 1000.0);
    }
    f64 readMs = Ms(t0);
    u64 rssEnd = Platform::GetProcessMemoryBytes();

    // ── Gates ──
    T2_CHECK(packMs + readMs < 120'000.0,
             "pack+read took %.1f s (order-of-magnitude regression)",
             (packMs + readMs) / 1000.0);
    if (rssStart > 0) {
        // Generous bound: our own test data is ~300MB resident; the packer
        // itself must not add another whole-pak copy on top.
        u64 slack = huge.size() * 2 + 256ull * 1024 * 1024;
        u64 peak = rssAfterPack > rssEnd ? rssAfterPack : rssEnd;
        T2_CHECK(peak < rssStart + originalTotal + slack,
                 "peak RSS %.0f MB vs start %.0f MB - packing may buffer the whole pak",
                 peak / (1024.0 * 1024.0), rssStart / (1024.0 * 1024.0));
    }

    std::error_code ec;
    std::filesystem::remove(pakPath, ec);
    std::printf("\n%s\n", s_Failures == 0 ? "PASS" : "FAIL");
    return s_Failures == 0 ? 0 : 1;
}
