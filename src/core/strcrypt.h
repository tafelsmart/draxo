#pragma once
#include <cstdint>

/*
 * STRCRYPT — Compile-time String Encryption
 *
 * Every sensitive string in the DLL that an anticheat could scan for
 * (e.g. "KillAura", "Scaffold", "draxodll") MUST be wrapped in STR().
 * The XOR key is derived from __TIME__ at compile time, making it
 * unique per build. Strings are decrypted on first access and cached
 * in a thread-safe local static.
 *
 * Usage:
 *   std::string s = STR("KillAura");  // encrypted at rest, decrypted at runtime
 *   const char* c = STR_C("Scaffold"); // const char* version
 *
 * ⚠️ NEVER use raw string literals for module names, search tags,
 *    or any text that identifies this as a cheat client.
 */

namespace strcrypt {

// Compile-time XOR key derived from __TIME__ (changes every build)
constexpr uint32_t g_key = ((__TIME__[0] * 31 + __TIME__[1]) * 31 + __TIME__[3]) * 31 +
                            __TIME__[4] + __TIME__[6] * 7 + __TIME__[7] * 13;

// Simple FNV-1a mixed with our key for per-string uniqueness
consteval uint64_t fnv64(const char* s, size_t len, uint64_t seed = 14695981039346656037ULL) {
    for (size_t i = 0; i < len; ++i)
        seed = (seed ^ static_cast<unsigned char>(s[i])) * 1099511628211ULL;
    return seed;
}

template<size_t N>
struct EncryptedString {
    uint8_t data[N]{};
    uint64_t hash = 0;

    consteval EncryptedString(const char(&src)[N]) : hash(fnv64(src, N - 1)) {
        // XOR each byte with (hash ^ key) — different per string AND per build
        uint8_t key_byte = static_cast<uint8_t>(hash >> (N & 7));
        for (size_t i = 0; i < N - 1; ++i) {
            uint8_t kb = static_cast<uint8_t>(((hash >> (i & 7)) ^ g_key) & 0xFF);
            data[i] = static_cast<uint8_t>(src[i]) ^ kb ^ key_byte;
        }
        data[N - 1] = 0;
    }

    // Decrypt into a mutable buffer
    void decrypt(char* out) const {
        uint8_t key_byte = static_cast<uint8_t>(hash >> (N & 7));
        for (size_t i = 0; i < N - 1; ++i) {
            uint8_t kb = static_cast<uint8_t>(((hash >> (i & 7)) ^ g_key) & 0xFF);
            out[i] = static_cast<char>(data[i] ^ kb ^ key_byte);
        }
        out[N - 1] = 0;
    }
};

} // namespace strcrypt

// ── Convenience macros ───────────────────────────────────────────────

// STR("text") → std::string (decrypted once, cached in thread-safe static)
#define STR(s) ([]{ \
    static constexpr auto e = strcrypt::EncryptedString(s); \
    static std::string cached; \
    if (cached.empty()) { \
        char buf[sizeof(s)]; \
        e.decrypt(buf); \
        cached = buf; \
    } \
    return cached; \
}())

// STR_C("text") → const char* (decrypted once, cached)
#define STR_C(s) ([]{ \
    static constexpr auto e = strcrypt::EncryptedString(s); \
    static char buf[sizeof(s)] = {}; \
    if (buf[0] == 0) e.decrypt(buf); \
    return buf; \
}())

// STR_DIRECT(s, buf) — decrypt directly into a caller-provided buffer
// Useful for sprintf/fprintf where you need the raw bytes inline.
#define STR_DECRYPT(s, buf) do { \
    static constexpr auto _e = strcrypt::EncryptedString(s); \
    _e.decrypt(buf); \
} while(0)
