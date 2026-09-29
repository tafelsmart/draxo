#pragma once
#include "modules/module.h"
#include <string>

/*
 * ChatBypass — Built-in command sender.
 *
 * Draws a small text input + Send button at the bottom of the screen.
 * Type any command (e.g. /lobby, /msg Player hi) and press Send or Enter.
 * The command is sent via JNI on the next game tick — no X-Box restriction
 * can block it because we talk directly to the connection layer.
 */
class ChatBypass : public Module {
public:
    ChatBypass();
    void onRender() override;
    void onUpdate(JNIEnv* env) override;

private:
    static inline char    s_inputBuf[256] = {};
    static inline bool    s_pending = false;

    void send(JNIEnv* env, const std::string& msg);
};
