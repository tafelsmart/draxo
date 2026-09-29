#pragma once
#include "modules/module.h"
class NoPortal : public Module {
public: NoPortal(); void onUpdate(JNIEnv* env) override;
};
