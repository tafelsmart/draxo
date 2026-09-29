#pragma once
#include "pch.h"
#include "modules/module.h"

class ModuleManager {
public:
    static void init();
    static void shutdown();

    static void tickAll(JNIEnv* env);
    static void renderAll();
    static void handleKey(int vk);

    static Module* getModule(const std::string& name);
    static bool& soundEnabled();

    static void saveAllSettings();
    static void loadAllSettings();

    static void setInterfaceMode(int mode);
    static int& interfaceToggleKey();

    static const std::vector<std::unique_ptr<Module>>& getModules() { return s_modules; }

private:
    static inline std::vector<std::unique_ptr<Module>> s_modules;
};
