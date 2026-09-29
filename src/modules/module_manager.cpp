#include "pch.h"
#include "core/strcrypt.h"
#include "modules/module_manager.h"
// ── COMBAT ──────────────────────────────────────────────────────────
#include "modules/reach.h"
#include "modules/aimassist.h"
#include "modules/killaura.h"
#include "modules/velocity.h"
#include "modules/triggerbot.h"
#include "modules/autoclicker.h"
#include "modules/criticals.h"
#include "modules/antikb.h"
#include "modules/wtap.h"
#include "modules/hitboxes.h"
#include "modules/bedaura.h"
#include "modules/inventorycleaner.h"
#include "modules/automlg.h"
#include "modules/autofish.h"
#include "modules/nametags.h"
#include "modules/lowfire.h"
#include "modules/nopumpkin.h"
#include "modules/noportal.h"
#include "modules/itemesp.h"
#include "modules/blockoverlay.h"
#include "modules/customcrosshair.h"
#include "modules/mobesp.h"
#include "modules/projectileesp.h"
#include "modules/lightlevel.h"
#include "modules/chunkborders.h"
#include "modules/potionhud.h"
#include "modules/armorhud.h"
#include "modules/enchantglint.h"
#include "modules/hitcolor.h"
// ── MOVEMENT ────────────────────────────────────────────────────────
#include "modules/jesus.h"
#include "modules/scaffold.h"
#include "modules/sprint.h"
#include "modules/noslow.h"
#include "modules/speed.h"
#include "modules/fly.h"
#include "modules/step.h"
#include "modules/timer.h"
#include "modules/dolphin.h"
#include "modules/glide.h"
#include "modules/fastladder.h"
#include "modules/safewalk.h"
#include "modules/airjump.h"
#include "modules/invmove.h"
// ── RENDER ──────────────────────────────────────────────────────────
#include "modules/esp.h"
#include "modules/xray.h"
#include "modules/fullbright.h"
#include "modules/tracers.h"
#include "modules/hitmarkers.h"
#include "modules/nohurtcam.h"
#include "modules/zoom.h"
#include "modules/noweather.h"
// ── PLAYER ──────────────────────────────────────────────────────────
#include "modules/nofall.h"
#include "modules/cheststealer.h"
#include "modules/autototem.h"
#include "modules/autoarmor.h"
#include "modules/fastplace.h"
#include "modules/fastbreak.h"
#include "modules/autotool.h"
#include "modules/autorespawn.h"
#include "modules/freecam.h"
// ── WORLD ───────────────────────────────────────────────────────────
#include "modules/nuker.h"
#include "modules/autofarm.h"
// ── EXPLOIT ─────────────────────────────────────────────────────────
#include "modules/disabler.h"
#include "modules/blink.h"
#include "modules/fakelag.h"
// ── MISC ────────────────────────────────────────────────────────────
#include "modules/antiafk.h"
#include "modules/spammer.h"
#include "modules/chatbypass.h"
// ── SDK ─────────────────────────────────────────────────────────────
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "sdk/world.h"
#include "core/config.h"
#include "core/cursor.h"
#include "render/hud.h"
#include "render/notifications.h"

static bool s_soundEnabled = true;
static int s_interfaceToggleKey = 0;
int& ModuleManager::interfaceToggleKey() { return s_interfaceToggleKey; }

void ModuleManager::init() {
    CMinecraft::initIDs();
    CEntity::initIDs();
    CWorld::initIDs();
    Config::load();
    Notifications::loadConfig();
    HUD::loadConfig();

    auto reg = [&](std::unique_ptr<Module> mod) {
        s_modules.push_back(std::move(mod));
    };

    // COMBAT
    reg(std::make_unique<Reach>());
    reg(std::make_unique<AimAssist>());
    reg(std::make_unique<KillAura>());
    reg(std::make_unique<Velocity>());
    reg(std::make_unique<TriggerBot>());
    reg(std::make_unique<AutoClicker>());
    reg(std::make_unique<Criticals>());
    reg(std::make_unique<AntiKB>());
    reg(std::make_unique<WTap>());
    reg(std::make_unique<HitBoxes>());
    reg(std::make_unique<BedAura>());
    reg(std::make_unique<InventoryCleaner>());
    reg(std::make_unique<AutoMLG>());
    reg(std::make_unique<AutoFish>());
    reg(std::make_unique<NameTags>());
    reg(std::make_unique<LowFire>());
    reg(std::make_unique<NoPumpkin>());
    reg(std::make_unique<NoPortal>());
    reg(std::make_unique<ItemESP>());
    reg(std::make_unique<BlockOverlay>());
    reg(std::make_unique<CustomCrosshair>());
    reg(std::make_unique<MobESP>());
    reg(std::make_unique<ProjectileESP>());
    reg(std::make_unique<LightLevel>());
    reg(std::make_unique<ChunkBorders>());
    reg(std::make_unique<PotionHUD>());
    reg(std::make_unique<ArmorHUD>());
    reg(std::make_unique<EnchantGlint>());
    reg(std::make_unique<HitColor>());

    // MOVEMENT
    reg(std::make_unique<Jesus>());
    reg(std::make_unique<Scaffold>());
    reg(std::make_unique<Sprint>());
    reg(std::make_unique<NoSlow>());
    reg(std::make_unique<Speed>());
    reg(std::make_unique<Fly>());
    reg(std::make_unique<Step>());
    reg(std::make_unique<TimerM>());
    reg(std::make_unique<Dolphin>());
    reg(std::make_unique<Glide>());
    reg(std::make_unique<FastLadder>());
    reg(std::make_unique<SafeWalk>());
    reg(std::make_unique<AirJump>());
    reg(std::make_unique<InvMove>());

    // RENDER
    reg(std::make_unique<ESP>());
    reg(std::make_unique<XRay>());
    reg(std::make_unique<FullBright>());
    reg(std::make_unique<Tracers>());
    reg(std::make_unique<HitMarkers>());
    reg(std::make_unique<NoHurtCam>());
    reg(std::make_unique<Zoom>());
    reg(std::make_unique<NoWeather>());

    // PLAYER
    reg(std::make_unique<NoFall>());
    reg(std::make_unique<ChestStealer>());
    reg(std::make_unique<AutoTotem>());
    reg(std::make_unique<AutoArmor>());
    reg(std::make_unique<FastPlace>());
    reg(std::make_unique<FastBreak>());
    reg(std::make_unique<AutoTool>());
    reg(std::make_unique<AutoRespawn>());
    reg(std::make_unique<Freecam>());

    // WORLD
    reg(std::make_unique<Nuker>());
    reg(std::make_unique<AutoFarm>());

    // EXPLOIT
    reg(std::make_unique<Disabler>());
    reg(std::make_unique<Blink>());
    reg(std::make_unique<FakeLag>());

    // MISC
    reg(std::make_unique<AntiAFK>());
    reg(std::make_unique<Spammer>());
    reg(std::make_unique<ChatBypass>());

    printf(STR_C("[Draxo] %zu modules registered\n"), s_modules.size());
    loadAllSettings();
}

void ModuleManager::shutdown() {
    // Nur speichern wenn Auto-Save an ist (auto_save in Config)
    if (Config::getBool("Client", "auto_save", false)) {
        saveAllSettings();
        Config::save();
    }
    for (auto& mod : s_modules)
        if (mod->isEnabled()) mod->setEnabled(false);
    s_modules.clear();
}

void ModuleManager::tickAll(JNIEnv* env) {
    static int s_serverCheck = 0;
    if (++s_serverCheck >= 30) {
        s_serverCheck = 0;
        // Server-wechsel Config auto-load
        static std::string s_lastIp, s_lastLoadedIp;
        std::string ip = CMinecraft::getServerIp();
        if (ip != s_lastIp) {
            s_lastIp = ip;
            if (ip.empty() || ip == "singleplayer") {
                s_lastLoadedIp = "";
            } else {
                std::string cfg = Config::getServerBind(ip);
                if (!cfg.empty() && s_lastLoadedIp != ip) {
                    s_lastLoadedIp = ip;
                    Config::loadNamed(cfg);
                    loadAllSettings();
                    Notifications::push("Server Config", "Loaded '" + cfg + "' for " + ip);
                }
            }
        }
    }

    static int s_frameCounter = 0;
    s_frameCounter++;
    for (auto& mod : s_modules) {
        mod->clearDetectWarnings();
        if (!mod->isEnabled()) continue;
        if (s_frameCounter % mod->tickInterval() != 0) continue;
        mod->onUpdate(env);
    }
}

void ModuleManager::saveAllSettings() {
    Config::setInt("Client", "interface_mode", Module::interfaceMode());
    for (auto& mod : s_modules) {
        mod->writeToConfig();
        mod->setSavedEnabled(mod->isEnabled());
    }
    Config::setInt("Client", "interface_toggle_key", s_interfaceToggleKey);
    Config::setBool("Client", "sound", s_soundEnabled);
    Config::setBool("Client", "cursor", Cursor::enabled());
    HUD::saveConfig();
    Notifications::saveConfig();
}

void ModuleManager::loadAllSettings() {
    Module::interfaceMode() = Config::getInt("Client", "interface_mode", 1);
    for (auto& mod : s_modules)
        mod->readFromConfig();
    s_interfaceToggleKey = Config::getInt("Client", "interface_toggle_key", 0);
    s_soundEnabled = Config::getBool("Client", "sound", true);
    Cursor::enabled() = Config::getBool("Client", "cursor", true);
    HUD::loadConfig();
    Notifications::loadConfig();
    for (auto& mod : s_modules) {
        bool saved = Config::getBool(mod->getName(), "_enabled", false);
        mod->setSavedEnabled(saved);
        if (saved) mod->setEnabled(true);
    }
    if (Module::isSimpleMode()) {
        for (auto& mod : s_modules)
            if (mod->hasPresets()) mod->applyStoredPreset();
    }
}

void ModuleManager::setInterfaceMode(int mode) {
    Module::interfaceMode() = mode;
    if (Module::isSimpleMode()) {
        for (auto& mod : s_modules)
            if (mod->hasPresets()) mod->applyStoredPreset();
    }
    Config::setInt("Client", "interface_mode", mode);
}

void ModuleManager::renderAll() {
    for (auto& mod : s_modules)
        if (mod->isEnabled()) mod->onRender();
}

void ModuleManager::handleKey(int vk) {
    for (auto& mod : s_modules) {
        if (mod->getKeyBind() == vk) {
            mod->toggle();
            Notifications::push(mod->getName(), mod->isEnabled() ? "Enabled" : "Disabled");
            if (Config::getBool("Client", "auto_save", false)) {
                saveAllSettings();
                Config::save();
            }
            if (s_soundEnabled)
                MessageBeep(mod->isEnabled() ? MB_ICONASTERISK : MB_ICONHAND);
        }
    }
}

bool& ModuleManager::soundEnabled() { return s_soundEnabled; }

Module* ModuleManager::getModule(const std::string& name) {
    for (auto& mod : s_modules)
        if (mod->getName() == name) return mod.get();
    return nullptr;
}
