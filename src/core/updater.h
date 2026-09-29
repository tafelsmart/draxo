#pragma once
#include <string>

/*
 * AutoUpdater — Checks a remote URL for a new version and downloads
 * the updated draxo.dll to replace the current one.
 *
 * The launcher is responsible for calling this BEFORE injection.
 * The DLL-side code provides the version check and download logic.
 */

namespace Updater {

// Current version string (must match what the server returns)
constexpr const char* VERSION = "1.0.0";

// Check if a newer version exists at the given URL
// Returns true if update is available, false if current is latest
bool checkForUpdate(const char* manifestUrl);

// Download the new DLL from url, save to path
// Returns true on success
bool downloadUpdate(const char* downloadUrl, const char* savePath);

// Get the latest version string from the manifest URL
std::string getLatestVersion(const char* manifestUrl);

} // namespace Updater
