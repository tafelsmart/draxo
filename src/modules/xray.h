#pragma once
#include "modules/module.h"
#include <vector>
#include <mutex>
#include <thread>
#include <string>

class XRay : public Module {
public:
    XRay();
    void onEnable() override;
    void onDisable() override;
    void onRender() override;

private:
    void searchLoop();

    struct BlockData {
        int x, y, z;
        std::string name;
    };

    std::vector<BlockData> m_blocks;
    std::mutex m_mutex;
    std::thread m_searchThread;
    bool m_running = false;
};
