#ifndef DMD_SETTINGS_H
#define DMD_SETTINGS_H

// Raspy2DMD settings (Raspy2DMD.cfg sections, set over MQTT with conf|Section|key:value),
// read from ConfigHelper with the original defaults from DMDRenderer_Config.SetDefaultConfig().

#include <string>

#include "core/Online.h"
#include "render/ClockScene.h"
#include "render/TextScene.h"

namespace settings {

// [ClockRenderer]
ClockSpec clockSpec();
std::string timezone();  // POSIX TZ string for configTzTime()

// [TextRenderer] defaults for every text command.
TextStyle textStyle();

// [DMDRenderer] center_images (default 1).
bool centerImages();

// [OpenWeatherMap]
dmd::OwmConfig owmConfig();

// [Running]
std::string scrollOrder();
uint32_t attractAfterMs();
bool standalone();       // Running.standalone == 1
bool showWebAddress();   // Running.default != 0: RenderFirstStart() shows the web address  // attract_mode seconds -> ms, 0 = disabled

}  // namespace settings

#endif
