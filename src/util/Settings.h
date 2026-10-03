#ifndef DMD_SETTINGS_H
#define DMD_SETTINGS_H

// Raspy2DMD settings (Raspy2DMD.cfg sections, set over MQTT with conf|Section|key:value),
// read from ConfigHelper with the original defaults from DMDRenderer_Config.SetDefaultConfig().

#include <string>

#include "render/ClockScene.h"
#include "render/TextScene.h"

namespace settings {

// [ClockRenderer]
ClockSpec clockSpec();
std::string timezone();  // POSIX TZ string for configTzTime()

// [TextRenderer] defaults for every text command.
TextStyle textStyle();

// [Running]
std::string scrollOrder();
uint32_t attractAfterMs();  // attract_mode seconds -> ms, 0 = disabled

}  // namespace settings

#endif
