#pragma once
#include "modules/module.h"
class NoFall : public Module {
public: NoFall(); void onUpdate(JNIEnv* env) override;
};
