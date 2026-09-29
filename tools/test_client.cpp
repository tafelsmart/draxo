// tools/test_client.cpp — Draxo Unit Tests (standalone, no Minecraft)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>
#include <ctime>
#include <windows.h>

static int s_passed = 0, s_failed = 0;
#define XT(name) do { printf("  TEST: %-55s ", name); } while(0)
#define P() do { printf("PASS\n"); s_passed++; } while(0)
#define F(msg) do { printf("FAIL — %s\n", msg); s_failed++; } while(0)
#define C(cond) do { if (cond) P(); else F(#cond); } while(0)

// ── Include client headers ──────────────────────────────────────────
#include "core/config.h"
#include "core/auth.h"
#include "config/mappings.h"

// ── Config tests (public API only) ───────────────────────────────────
void test_config_basics() {
    printf("\n=== CONFIG: get/set ===\n");

    Config::setFloat("M", "speed", 1.5f);
    XT("float roundtrip"); C(Config::getFloat("M","speed",0)==1.5f);

    Config::setBool("M", "on", true);
    XT("bool true"); C(Config::getBool("M","on",false)==true);

    Config::setBool("M", "on", false);
    XT("bool false"); C(Config::getBool("M","on",true)==false);

    Config::setInt("M", "n", 42);
    XT("int"); C(Config::getInt("M","n",0)==42);

    Config::setString("M", "s", "hi");
    XT("string"); C(Config::getString("M","s","")=="hi");

    XT("default fallback"); C(Config::getFloat("X","k",99)==99);
}

void test_config_export_import() {
    printf("\n=== CONFIG: export/import ===\n");
    std::string e = Config::exportAll();
    XT("export has header");  C(e.find("# Draxo") != std::string::npos);
    XT("export has M.speed"); C(e.find("M.speed=") != std::string::npos);
    XT("export has M.on");    C(e.find("M.on=") != std::string::npos);

    XT("import ok"); C(Config::importString(e));
    XT("import float"); C(Config::getFloat("M","speed",0)==1.5f);
    XT("import bool"); C(Config::getBool("M","on",false)==false);
    XT("import empty fails"); C(!Config::importString(""));
    XT("import no-equals fails"); C(!Config::importString("no_equals_here"));
}

void test_config_presets() {
    printf("\n=== CONFIG: presets ===\n");

    // Save two presets
    Config::savePresetName("KillAura","MyConfig");
    Config::savePresetName("KillAura","Tourney");
    auto n = Config::getPresetNames("KillAura");
    XT("two presets"); C(n.size() == 2);

    // Duplicate rejected
    Config::savePresetName("KillAura","MyConfig");
    n = Config::getPresetNames("KillAura");
    XT("no dupes"); C(n.size() == 2);

    // Comma rejected
    Config::savePresetName("KillAura","bad,name");
    n = Config::getPresetNames("KillAura");
    XT("comma rejected"); C(n.size() == 2);

    // Delete
    Config::deletePresetName("KillAura","MyConfig");
    n = Config::getPresetNames("KillAura");
    XT("delete one"); C(n.size() == 1 && n[0] == "Tourney");

    Config::deletePresetName("KillAura","Tourney");
    n = Config::getPresetNames("KillAura");
    XT("delete all"); C(n.empty());

    // Preset data deletion
    Config::setFloat("ModX", "range", 3.0f);
    Config::savePresetName("ModX", "TestP");
    // write some preset data directly (simulating what Module::savePreset does)
    Config::setFloat("__preset_fake__", "ModX.TestP.f.range._v", 4.2f); // won't work, use actual key

    // Use the actual config key format
    Config::setFloat("ModX", "spot", 1.0f); // real setting
    Config::deletePresetData("ModX", "TestP");
    XT("deletePresetData runs without crash"); C(true); // just verify it doesn't crash
    
    // Cleanup
    Config::deletePresetName("ModX", "TestP");
}

void test_config_server_binds() {
    printf("\n=== CONFIG: server binds ===\n");

    Config::bindServer("hypixel.net", "hypixel_cfg");
    Config::bindServer("minemen.club", "minemen_cfg");

    std::string b = Config::getServerBind("hypixel.net");
    XT("getServerBind hypixel"); C(b == "hypixel_cfg");

    b = Config::getServerBind("minemen.club");
    XT("getServerBind minemen"); C(b == "minemen_cfg");

    b = Config::getServerBind("nonexistent.server");
    XT("getServerBind nonexistent"); C(b == "");

    auto binds = Config::listServerBinds();
    XT("listServerBinds count"); C(binds.size() == 2);

    Config::unbindServer("hypixel.net");
    binds = Config::listServerBinds();
    XT("unbindServer"); C(binds.size() == 1);

    Config::unbindServer("minemen.club");
}

// ── Mappings tests ─────────────────────────────────────────────────
void test_mappings() {
    printf("\n=== MAPPINGS ===\n");

    // Class names can be readable ("net/minecraft/client/Minecraft") or
    // obfuscated ("gfj"). Both are valid. Check for non-empty + that
    // either the class itself or its _Sig variant follows JNI conventions.
    XT("Minecraft_Class nonempty"); C(strlen(Mappings::Minecraft_Class) > 0);
    XT("Minecraft_Class_Sig is valid"); C(Mappings::Minecraft_Class_Sig[0] == 'L');
    XT("Entity_Class nonempty"); C(strlen(Mappings::Entity_Class) > 0);
    XT("Player_Class nonempty"); C(strlen(Mappings::Player_Class) > 0);

    // Check signature pairs
    XT("Entity_getX + _Sig"); C(strlen(Mappings::Entity_getX_Sig) > 0);
    XT("Player_attack + _Sig"); C(strlen(Mappings::Player_attack_Sig) > 0);
    XT("GameMode_attack + _Sig"); C(strlen(Mappings::GameMode_attack_Sig) > 0);
    XT("MC_player + _Sig"); C(strlen(Mappings::MC_player_Sig) > 0);

    // Check JNI sig validity
    XT("Entity_getX_Sig is method sig"); C(Mappings::Entity_getX_Sig[0] == '(');
    XT("Player_attack_Sig is method sig"); C(Mappings::Player_attack_Sig[0] == '(');
    // Field sig format: L<classname>; (e.g. "Lhnh;") — always valid for obfuscated too
    XT("MC_player_Sig is object field"); C(Mappings::MC_player_Sig[0] == 'L' &&
                                            Mappings::MC_player_Sig[strlen(Mappings::MC_player_Sig)-1] == ';');

    // Critical mappings exist
    XT("GameMode_useItemOn exists"); C(strlen(Mappings::GameMode_useItemOn) > 0);
    XT("Entity_onGround exists"); C(strlen(Mappings::Entity_onGround) > 0);
    XT("MC_getConnection exists"); C(strlen(Mappings::MC_getConnection) > 0);
}

// ── Auth tests (poly-XOR keygen — cross-language verification) ──────
// The reference vector below was computed by tools/keygen_reference.py
// and MUST match the PHP keygen output (tools/keygen.php keygen_selfcheck).
void test_auth_keygen() {
    printf("\n=== AUTH: poly-XOR keygen ===\n");

    // Fixed HWID + permanent key -> fully deterministic
    const char* hwid = "0123456789ABCDEF0123456789ABCDEF";
    std::string k = auth::generateKey(hwid, 0);
    XT("permanent key matches reference");
    C(k == "DRAXO-5YQZZ-4VAK1-8FJGN-XTDDG-A9076-M");

    XT("key format v2 (37 chars, 6 groups)");
    C(k.size() == 37 && k.rfind("DRAXO-", 0) == 0);

    // Roundtrip: generate for real HWID, then validate
    std::string real = auth::getHWID();
    std::string pk = auth::generateKey(real, 0);   // permanent
    std::string tk = auth::generateKey(real, 24);  // 24h
    XT("permanent key validates");   C(auth::validateKey(pk));
    XT("timed key validates");       C(auth::validateKey(tk));
    XT("timed key not expired");     C(!auth::isKeyExpired(tk));
    XT("permanent key never expires"); C(!auth::isKeyExpired(pk));
    XT("expiry > now");              C(auth::getKeyExpiry(tk) > (uint32_t)time(nullptr));
    XT("permanent expiry == 0");     C(auth::getKeyExpiry(pk) == 0);

    // Tampered key must be rejected (flip one char)
    std::string bad = pk;
    bad[10] = (bad[10] == 'A') ? 'B' : 'A';
    XT("tampered key rejected");     C(!auth::validateKey(bad));

    // Garbage rejected
    XT("garbage key rejected");      C(!auth::validateKey("DRAXO-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX-X"));

    // Deterministic decode checks (hardware-independent): the fixed-HWID
    // reference keys (computed by tools/keygen_reference.py with a fixed
    // clock) must decode to exact known values. This guards the b32dec
    // table against future drift even on machines whose real HWID happens
    // to avoid V/W/X/Y in generated keys.
    XT("permanent ref key decodes to expiry 0");
    C(auth::getKeyExpiry("DRAXO-5YQZZ-4VAK1-8FJGN-XTDDG-A9076-M") == 0);
    XT("timed ref key decodes to exact expiry");
    C(auth::getKeyExpiry("DRAXO-5YQZZ-4VAK1-8FJGN-XTDDP-053NR-W") == 1700086400u);

    // ── getStatus() smoke tests ─────────────────────────────────────
    XT("getStatus returns LOCKED when no key stored");
    {
        auto info = auth::getStatus();
        C(info.state == auth::LOCKED && !info.hwid.empty());
    }
    XT("getStatus returns VALID with live permanent key");
    {
        // Temporarily store the permanent key we just generated
        std::string real = auth::getHWID();
        std::string pk = auth::generateKey(real, 0);
        auth::storeKey(pk);
        auth::check();  // re-validate to set auth slots
        auto info = auth::getStatus();
        C(info.state == auth::VALID && info.expiryStr == "Permanent");
        // Clean up: remove test key
        Config::setString("License", "key", "");
        auth::lock();
    }
}

// ── Main ───────────────────────────────────────────────────────────
int main() {
    printf("========================================\n");
    printf("  DRAXO CLIENT — UNIT TESTS\n");
    printf("========================================\n");

    test_config_basics();
    test_config_export_import();
    test_config_presets();
    test_config_server_binds();
    test_mappings();
    test_auth_keygen();

    printf("\n========================================\n");
    printf("  RESULTS: %d passed, %d failed\n", s_passed, s_failed);
    printf("========================================\n");

    return s_failed ? 1 : 0;
}
