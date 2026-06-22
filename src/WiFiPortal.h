#pragma once

#include "ConfigStore.h"
#include "LGFX.h"

// Connects to WiFi using credentials stored in ConfigStore. If none are stored
// (or the connection fails) it brings up a captive setup portal on a temporary
// access point, collects credentials, saves them, and reboots.
//
// Replaces the ESP-only tzapu/WiFiManager.
class WiFiPortal
{
private:
    ConfigStore& config;
    LGFX& tft;

    bool TryConnect(const String& ssid, const String& pass, uint32_t timeoutMs);
    void RunConfigPortal();
    void ShowStatus(const char* line1, const String& line2 = "", const String& line3 = "");

public:
    static constexpr const char* PortalName = "MicroRadar-Setup";

    WiFiPortal(ConfigStore& cfg, LGFX& display) : config(cfg), tft(display) {}

    void AutoConnect();

    // Call periodically from loop(): non-blocking reconnect if the link drops.
    void MaintainConnection();
};
