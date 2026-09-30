#pragma once
#include "pch.h"
#include <string>

/*
 * AUTH v2 — Ed25519-Grant (siehe auth.cpp für die ausführliche Begründung)
 *
 * Kein Geheimnis im Binary mehr: der Server signiert, der Launcher prüft.
 * Diese Datei liest die Grant-Struktur (Ablauf, HWID-Bindung) und meldet
 * sie; die Signatur wird derzeit nicht kryptografisch geprüft.
 */

namespace auth {

// ── Status enum for UI display ──────────────────────────────────────
enum AuthState { VALID, LOCKED, EXPIRED };
struct AuthInfo {
    AuthState state = LOCKED;
    std::string expiryStr;   // human-readable (or "Permanent"/"—")
    std::string hwid;        // current machine HWID
    std::string keyPreview;  // first 20 chars of stored key (or "")
};

// True if license is valid and not expired. Checks 3-slot consensus.
bool isAuthorized();

// Get full status info for the UI (state + expiry + HWID)
AuthInfo getStatus();

// Force-lock all 3 auth slots (called on integrity/debug detection)
void lock();

// Periodic re-check: validates key hasn't expired. Call from tickAll.
// Returns false if auth was lost (key expired / tampered).
bool tick();

// Module-level guard: refuse module enable if unauthorized.
// Returns true if module may be enabled.
bool moduleGuard();

// Compute HWID once (cached after first call)
std::string getHWID();

// Validate a license key against the current machine's HWID
bool validateKey(const std::string& key);

// Debug: extract the HWID prefix the key was generated for (24 hex chars)
// Returns "" if the key is malformed. Used by the menu for live match preview.
std::string getKeyHWID(const std::string& key);

// Compute sha256(machineHWID) and return first 24 hex chars.
// Used alongside getKeyHWID to compare grant vs machine.
std::string hashHWID12(const std::string& hwid);

// Grants werden ausschliesslich vom Server signiert. Es gibt hier
// bewusst keine generateKey()-Funktion: sie braeuchte den privaten
// Schluessel, und der gehoert nicht in diese DLL.

// Read/write key in Config
std::string getStoredKey();
void storeKey(const std::string& key);

// Time-limited key support
uint32_t getKeyExpiry(const std::string& key);
bool isKeyExpired(const std::string& key);
std::string getExpiryString(const std::string& key);

// Legacy (called once at init, kept for dllmain compatibility)
bool check();

} // namespace auth
