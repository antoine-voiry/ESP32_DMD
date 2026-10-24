#ifndef WIFI_MANAGER_HELPER_H
#define WIFI_MANAGER_HELPER_H

// Joins the saved Wi-Fi network, or opens the DMD_CONFIG_WIFI setup portal where the user enters the
// network and the MQTT settings. The portal is protected by a random password shown on the panel.
// Holding the BOOT button for 1.5 s while the board starts opens the portal even when the saved
// network works (press it just after reset: held during reset, it starts the flashing mode).

#include <functional>
#include <string>

class WifiManagerHelper {
public:
    static constexpr const char* kPortalName = "DMD_CONFIG_WIFI";

    // Called once the portal is up, so the panel can show how to join it.
    using PortalNotice = std::function<void(const std::string& ssid, const std::string& password)>;

    // Returns once connected. Restarts the board if the portal fails or times out.
    void connect(bool forcePortal, const PortalNotice& notice);

    // 10 characters, no look-alike letters; a new one each time the portal opens.
    static std::string newPortalPassword();

    // True when the BOOT button (GPIO0) stays pressed for kBootButtonHoldMs.
    static bool bootButtonHeld();

private:
    bool _saveRequested = false;
};

#endif
