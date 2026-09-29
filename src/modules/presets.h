#pragma once
#include "modules/module.h"

/*
 * ModulePresets — Central Legit/Rage preset registry for ALL modules.
 *
 * Simple interface mode: every module shows ONLY a Legit/Rage selector.
 * The values behind those two options live here, tuned for undetectability:
 *   - "Legit" = human-like, conservative, safe on strict AC servers
 *   - "Rage"  = aggressive, but with AC-bypass features still enabled
 *
 * Advanced interface mode: full settings UI, plus buttons to apply these
 * same presets to overwrite your custom values.
 *
 * A single source of truth keeps every module consistent and makes tuning
 * one setting trivially easy.
 */
namespace ModulePresets {
    // True if this module has at least one preset registered.
    bool hasPreset(const Module* mod);

    // Apply "Legit" or "Rage" values into the module's settings.
    // Only writes keys the module actually owns — no phantom settings.
    void apply(Module* mod, const char* name);

    // Apply whatever simplePreset() currently selects on the module.
    void applyStored(Module* mod);
}
