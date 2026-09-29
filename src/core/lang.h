#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Lang {
void init();
void shutdown();
const char* get(const char* key);
void setLanguage(const char* lang);
const char* currentLanguage();
std::vector<std::string> availableLanguages();
void add(const char* key, const char* value);
}
