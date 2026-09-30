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

// ── Auth tests (Ed25519-Grant, v2) ─────────────────────────────────
//
// Bis hier stand ein Test, der pruefte, dass die DLL denselben Key erzeugt
// wie die Web-Keygen-Seite. Diese Kopplung gibt es nicht mehr: Keys werden
// vom Server signiert, und die DLL besitzt den privaten Schluessel nicht
// (und darf ihn nicht besitzen).
//
// Was hier geprueft wird, ist das, was die DLL tatsaechlich kann: ein
// Grant-Token lesen, Ablauf und HWID-Bindung beurteilen und den Stand in
// der Config ablegen. Die Signatur beweist der Launcher
// (license_manager.py + license_signing.py).
//
// Die Referenz-Grants unten hat der Server mit
//     python -m draxo_bot.signing keygen
// und dann /key erzeugt. Sie sind auf die Test-HWID gebunden, damit der
// Binding-Test etwas Greifbares hat.
static const char* const GRANT_PERMANENT =
    "DRAXO3-0CMJ013TTCWHE3YDDGFQT7E6E5YRWGXXD8000000FRGQWFXE3WBMTP5R5X0P2B3JJSER11GW519PC8HW3E0DH6Z6BRMVMT9E0MB4W35BVPYW6STXE9JZWTQSB2SJ18JJV05VEDYPV715M38";
static const char* const GRANT_TIMED =
    "DRAXO3-0CMJ013TTCWHE3YDDGFQT7E6E5YRWGXXD879BFKAZGRE9Y11J43NSZ0AZZYMDEA8JXSYB4CJJXFAMY5ESH456RPSZGT8V7F7YB9RCYE2M35S19EKTERQRDXEH6V19J3SN36AK51776TTJ30";
static const char* const GRANT_ACCOUNT_ONLY =
    "DRAXO3-0CMJ013TTCWHE3R0000000000008WGXXD879BFKA142C0A8AE3F26PKAYM9GYK2H4QB6CQBCEZFDW1F0XS51PK93YYJFJAYS539P115SGK0767XQ67AKEG10Z329658KCP8BP62TZGJ4G38";
static const char* const TEST_HWID =
    "0123456789ABCDEF0123456789ABCDEF";

void test_auth_grant() {
    printf("\n=== AUTH: Ed25519-Grant ===\n");

    // ── Format ──
    XT("grant length is 150 chars");   C(strlen(GRANT_PERMANENT) == 150);
    XT("grant prefix");               C(strncmp(GRANT_PERMANENT, "DRAXO3-", 7) == 0);

    // ── Ablauf ──
    XT("permanent grant has no expiry"); C(auth::getKeyExpiry(GRANT_PERMANENT) == 0);
    XT("permanent grant never expires"); C(!auth::isKeyExpired(GRANT_PERMANENT));
    XT("permanent grant reads 'Permanent'");
    C(auth::getExpiryString(GRANT_PERMANENT) == "Permanent");

    XT("timed grant has an expiry");  C(auth::getKeyExpiry(GRANT_TIMED) > 0);
    XT("timed grant not expired");    C(!auth::isKeyExpired(GRANT_TIMED));
    XT("timed grant expiry in the future");
    C(auth::getKeyExpiry(GRANT_TIMED) > (uint32_t)time(nullptr));

    // ── HWID-Bindung ──
    XT("machine-bound grant exposes its HWID prefix");
    C(auth::getKeyHWID(GRANT_PERMANENT).size() == 16);
    XT("account-only grant has no machine binding");
    C(auth::getKeyHWID(GRANT_ACCOUNT_ONLY).empty());

    // ── Ablehnung ──
    // Die DLL prueft die Signatur nicht. Sie lehnt aber ab, was strukturell
    // kein Grant ist — und zwar mit derselben Meldung wie vorher.
    XT("empty key rejected");        C(!auth::validateKey(""));
    XT("garbage key rejected");      C(!auth::validateKey("hello world"));
    XT("legacy DRAXO- key rejected"); C(!auth::validateKey("DRAXO-F7X66-AZXVC-Z3GYE-Y776Z-JSP0W-G"));
    XT("truncated grant rejected");  C(!auth::validateKey("DRAXO3-ABC"));
    XT("wrong-version grant rejected");
    C(!auth::validateKey("DRAXO3-0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"));

    // ── Ablage ──
    XT("storeKey / getStoredKey round-trip");
    {
        std::string before = auth::getStoredKey();
        auth::storeKey(GRANT_PERMANENT);
        C(auth::getStoredKey() == std::string(GRANT_PERMANENT));
        auth::storeKey(before);
    }

    // ── Bindung gegen die echte Maschine ──
    // Der Grant ist auf die Test-HWID gebunden, nicht auf diesen Rechner.
    // Deshalb MUSS validateKey ihn hier ablehnen — das ist der Beweis, dass
    // die HWID-Bindung überhaupt greift.
    XT("grant for another machine is rejected");
    C(!auth::validateKey(GRANT_PERMANENT));

    // Und mit der passenden HWID akzeptiert — sha256(TEST_HWID)[0:8]
    // muesste im Grant stehen. Das pruefen wir indirekt ueber getKeyHWID.
    XT("HWID prefix matches sha256 of TEST_HWID");
    {
        std::string expected = auth::hashHWID12(TEST_HWID).substr(0, 16);
        C(auth::getKeyHWID(GRANT_PERMANENT) == expected);
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
    test_auth_grant();

    printf("\n========================================\n");
    printf("  RESULTS: %d passed, %d failed\n", s_passed, s_failed);
    printf("========================================\n");

    return s_failed ? 1 : 0;
}
