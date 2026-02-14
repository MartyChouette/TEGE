#include "Enjin/Networking/NetworkSecurity.h"
#include "Enjin/Logging/Log.h"

#ifdef ENJIN_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fstream>
#endif

namespace Enjin {
namespace Networking {

SessionKey GenerateSessionKey() {
    SessionKey key = {};

#ifdef ENJIN_PLATFORM_WINDOWS
    // Use BCryptGenRandom for cryptographic randomness
    NTSTATUS status = BCryptGenRandom(
        nullptr, key.data(), SESSION_KEY_SIZE,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        ENJIN_LOG_ERROR(Network, "BCryptGenRandom failed (status=0x%08lX), using fallback", status);
        // Fallback: use QueryPerformanceCounter + GetTickCount as entropy sources
        LARGE_INTEGER pc;
        QueryPerformanceCounter(&pc);
        u64 tick = GetTickCount64();
        u8 entropy[16];
        std::memcpy(entropy, &pc.QuadPart, 8);
        std::memcpy(entropy + 8, &tick, 8);
        // Hash the entropy to produce a key (not ideal but better than zeros)
        SHA256 hasher;
        hasher.Update(entropy, sizeof(entropy));
        hasher.Final(key.data());
    }
#else
    // POSIX: read from /dev/urandom
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom.good()) {
        urandom.read(reinterpret_cast<char*>(key.data()), SESSION_KEY_SIZE);
    } else {
        ENJIN_LOG_ERROR(Network, "Failed to open /dev/urandom, using fallback");
        // Fallback: hash current time and address space layout
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        u8 entropy[16];
        std::memcpy(entropy, &ts.tv_sec, 8);
        std::memcpy(entropy + 8, &ts.tv_nsec, 8);
        SHA256 hasher;
        hasher.Update(entropy, sizeof(entropy));
        hasher.Final(key.data());
    }
#endif

    ENJIN_LOG_INFO(Network, "Generated %u-byte session key", SESSION_KEY_SIZE);
    return key;
}

} // namespace Networking
} // namespace Enjin
