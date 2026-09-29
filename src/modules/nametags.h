#pragma once
#include "pch.h"
#include "modules/module.h"

class NameTags : public Module {
public:
    NameTags();
    void onRender() override;
};
