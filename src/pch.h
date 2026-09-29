#pragma once

// Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <GL/gl.h>

// STL
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cmath>

// JNI
#include <jni.h>

// MinHook
#include <MinHook.h>

// ImGui
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

// Log: Alle printf-Ausgaben landen zusaetzlich im Debug-Ringpuffer
#include "core/log.h"
#define printf Log::write
