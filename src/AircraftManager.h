#pragma once

#include <map>

#include "models/TrackedAircraft.h"
#include "ConfigurationWebServer.h"
#include "OpenSkyAuthTokenHandler.h"
#include "LGFX.h"

class AircraftManager
{
private:
    double lat = 0.0;
    double lon = 0.0;
    double rad = 0.2;
    std::map<String, TrackedAircraft> trackedAircraft;

    bool displayInfoText = true;
    bool displayTriangles = true;
    bool displayScanline = true;

    unsigned long fetchInterval = 0;
    unsigned long lastFetch = 999999;
    bool forceFetch = false;

    ConfigurationWebServer& configServer;
    OpenSkyAuthTokenHandler& authHandler;
    HttpRequestManager& http;
    LGFX& tft;

    void DrawRadarCircles(LGFX_Sprite& backbuffer) const;
    std::pair<int, int> ProjectCoordinateToScreen(float predLat, float predLon) const;
    void DrawAircraftInfo(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const;
    void DrawAircraftMarker(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const;

public:
    AircraftManager(ConfigurationWebServer& config, OpenSkyAuthTokenHandler& auth, HttpRequestManager& httpManager, LGFX& tftGfx)
        : configServer(config), authHandler(auth), http(httpManager), tft(tftGfx)
    {
    }
    ~AircraftManager() = default;

    void Initialise();
    void ReloadSettings(); // re-read location/radius/display options without a reboot
    void Update();
    void Draw(LGFX_Sprite& backbuffer);

    // runtime controls (Pico Display buttons) - session-only, don't persist
    void ToggleScanline();
    void ToggleInfoText();
    void AdjustRadius(double delta);
};