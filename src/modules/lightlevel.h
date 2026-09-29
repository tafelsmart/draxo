#pragma once
#include "modules/module.h"
class LightLevel : public Module {
public: LightLevel(); void onRender() override;
private: static inline jclass s_lvl=nullptr; static inline jmethodID s_getBright=nullptr;
    static inline jclass s_mbp=nullptr; static inline jmethodID s_mbpInit=nullptr;
};
