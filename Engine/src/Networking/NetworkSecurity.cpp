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
    if (!urandom.good()) {
        // C2 fix: fail hard instead of using weak predictable fallback.
        ENJIN_LOG_FATAL(Network, "Failed to open /dev/urandom — cannot generate secure session key");
        return key; // Returns zero key; caller must check and refuse to authenticate
    }
    urandom.read(reinterpret_cast<char*>(key.data()), SESSION_KEY_SIZE);
    // The open succeeding is not the same as the read filling the buffer. A
    // short read leaves the tail of the key zero, and every byte we did not get
    // is a byte an attacker does not have to guess. The Windows branch checks
    // its status and fails hard; this one only checked before reading.
    if (urandom.gcount() != static_cast<std::streamsize>(SESSION_KEY_SIZE)) {
        ENJIN_LOG_FATAL(Network, "Short read from /dev/urandom (%lld of %u bytes) — cannot generate secure session key",
                        static_cast<long long>(urandom.gcount()), SESSION_KEY_SIZE);
        key = {};
        return key; // Returns zero key; caller must check and refuse to authenticate
    }
#endif

    ENJIN_LOG_INFO(Network, "Generated %u-byte session key", SESSION_KEY_SIZE);
    return key;
}

} // namespace Networking
} // namespace Enjin
