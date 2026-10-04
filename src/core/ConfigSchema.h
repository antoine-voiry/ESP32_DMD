#ifndef DMD_CORE_CONFIG_SCHEMA_H
#define DMD_CORE_CONFIG_SCHEMA_H

// The Raspy2DMD settings (Raspy2DMD.cfg, DMDRenderer_Config.SetDefaultConfig / GetConfig): every
// section and key in the order the Pi reported them, with their defaults. Used by receipconf, the
// settings web page and standalone mode.

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace dmd {

struct SettingDef {
    const char* section;
    const char* key;  // spelling used by GetConfig() (what receipconf publishes)
    const char* def;
    bool usedOnEsp32;  // false: kept for Raspydarts but has no effect here (HDMI, sound, Pi GPIO timing...)
};

const std::vector<SettingDef>& configSchema();

// configparser lower-cases option names; so do we, for storage and lookups.
std::string normaliseKey(const std::string& key);

// Case-insensitive lookup of a known setting, nullptr if unknown.
const SettingDef* findSetting(const std::string& section, const std::string& key);

// receipconf: one "Section:key:value" line per setting, in schema order.
std::vector<std::string> receipconfLines(
    const std::function<std::string(const SettingDef&)>& valueOf);

// Settings that only take effect after a restart (panel geometry, standalone mode).
bool needsRestart(const std::string& section, const std::string& key);

// Standalone mode (Running.standalone = 1) only accepts the commands the original handled before
// its "if standalone == 0" block.
bool allowedInStandalone(const std::string& action);

struct PanelGeometry {
    int cols;   // pixels per panel
    int rows;
    int chain;  // panels chained horizontally
    int width() const { return cols * chain; }
};
// Parses DMDRenderer.cols / rows / led_chain, falling back to the defaults and clamping to what the
// HUB75 DMA driver supports (cols 16..128, rows 16..64, chain 1..4).
PanelGeometry panelGeometry(const std::string& cols, const std::string& rows, const std::string& chain,
                            const PanelGeometry& defaults);

}  // namespace dmd

#endif
