#pragma once
#include "modules/module.h"

class Sprint : public Module {
public:
    Sprint();
    void onUpdate(JNIEnv* env) override;
    void onDisable() override;
private:
    bool m_wasSprinting = false;
};
