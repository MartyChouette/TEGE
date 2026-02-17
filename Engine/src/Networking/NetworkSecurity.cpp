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
        // C2 fix: fail hard instead of using weak predictable fallback.
        // A weak session key compromises all HMAC authentication.
        ENJIN_LOG_FATAL(Network, "BCryptGenRandom failed (status=0x%08lX) — cannot generate secure session key", status);
        return key; // Returns zero key; caller must check and refuse to authenticate
    }
#else
    // POSIX: read from /dev/urandom
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom.good()) {
        urandom.read(reinterpret_cast<char*>(key.data()), SESSION_KEY_SIZE);
    } else {
        // C2 fix: fail hard instead of using weak predictable fallback.
        ENJIN_LOG_FATAL(Network, "Failed to open /dev/urandom — cannot generate secure session key");
        return key; // Returns zero key; caller must check and refuse to authenticate
    }
#endif

    ENJIN_LOG_INFO(Network, "Generated %u-byte session key", SESSION_KEY_SIZE);
    return key;
}

} // namespace Networking
} // namespace Enjin
