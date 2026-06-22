#include "AircraftManager.h"
#include "RadarLayout.h"
#include "DrawHelpers.h"
#include "Sync.h"

#include <ArduinoJson.h>

void AircraftManager::Initialise()
{
    ReloadSettings();

    // calculate how often we can call OpenSky API before being rate limited
    constexpr int MS_PER_DAY = 24 * 60 * 60 * 1000;
    constexpr int ANONYMOUS_TOKENS_PER_DAY = 400;
    constexpr int AUTHED_TOKENS_PER_DAY = 4000;
    constexpr int TOKEN_BUFFER = 3;
    int dailyRequestBudget = ANONYMOUS_TOKENS_PER_DAY - TOKEN_BUFFER; // non-authed tokens minus buffer

    const String token = authHandler.GetValidToken(configServer.GetStoredString("opensky-id"), configServer.GetStoredString("opensky-secret"));
    if (!token.isEmpty())
        dailyRequestBudget = AUTHED_TOKENS_PER_DAY - TOKEN_BUFFER; // authed tokens minus buffer

    fetchInterval = MS_PER_DAY / dailyRequestBudget;
}

void AircraftManager::ReloadSettings()
{
    // core1 reads lat/lon/rad + the display flags while drawing, so update them
    // under the same lock to avoid a torn read mid-frame.
    mutex_enter_blocking(&g_dataMutex);

    lat = configServer.GetStoredString("latitude").toDouble();
    lon = configServer.GetStoredString("longitude").toDouble();
    rad = configServer.GetStoredString("radius").toDouble();

    const String renderText = configServer.GetStoredString("infotext");
    const String renderTris = configServer.GetStoredString("triangle");
    const String renderScan = configServer.GetStoredString("scanline");
    if (!renderText.isEmpty()) displayInfoText = renderText == "true";
    if (!renderTris.isEmpty()) displayTriangles = renderTris == "true";
    displayScanline = renderScan.isEmpty() || renderScan == "true";

    // the tracked planes belong to the old area; drop them and refetch promptly
    trackedAircraft.clear();
    forceFetch = true;

    mutex_exit(&g_dataMutex);
}

void AircraftManager::Update()
{
    unsigned long now = millis();

    // fetch cycle
    if (forceFetch || now - lastFetch >= fetchInterval) {
        forceFetch = false;
        lastFetch = now;

        // auth
        const String token = authHandler.GetValidToken(
            configServer.GetStoredString("opensky-id"),
            configServer.GetStoredString("opensky-secret")
        );

        std::vector<std::pair<String, String>> headers = {};
        if (!token.isEmpty()) headers.push_back({ "Authorization", "Bearer " + token });

        // request
        HttpResult result = http.Get(
            "https://opensky-network.org/api/states/all",
            {
              {"lamin", String(lat - rad)},
              {"lamax", String(lat + rad)},
              {"lomin", String(lon - rad)},
              {"lomax", String(lon + rad)}
            },
            headers
        );

        // If request failed, skip this update
        if (!result.success) {
            Serial.print("[WARN] OpenSky API request failed: ");
            Serial.println(result.errorMessage);
            return;
        }

        // parse off-lock (the slow part), then merge into the shared map briefly
        // under the lock so core1's draw never sees a half-updated map.
        JsonDocument doc;
        deserializeJson(doc, result.response);
        auto aircraft = JsonParser::ParseArray<Aircraft>(doc["states"]);
        now = millis(); // override with post-parse timestamp

        mutex_enter_blocking(&g_dataMutex);
        for (auto& ac : aircraft) {
            auto it = trackedAircraft.find(ac.icao24);
            if (it == trackedAircraft.end())
                trackedAircraft.emplace(ac.icao24, TrackedAircraft{ ac, now });
            else
                it->second.Update(ac, now);
        }

        // remove any planes that disappeared from the feed
        for (auto it = trackedAircraft.begin(); it != trackedAircraft.end(); ) {
            bool aircraftPresent = std::any_of(aircraft.begin(), aircraft.end(), [&](const Aircraft& ac) { return ac.icao24 == it->first; });
            if (!aircraftPresent)
                it = trackedAircraft.erase(it);
            else
                ++it;
        }
        mutex_exit(&g_dataMutex);
    }
}

void AircraftManager::Draw(LGFX_Sprite& backbuffer)
{
    // sweep + circles touch only the backbuffer (owned by this core), so they
    // need no lock; only the tracked-aircraft iteration shares state with core0.
    if (displayScanline) {
        const float ang = millis() / 3000.0f;
        // Reach past the rectangle's half-diagonal so the sweep line AND its trailing
        // fan over-shoot the boundary in every direction and just clip at the screen
        // edge - that fills the rectangular screen, corners included.
        const float reach = std::sqrt((float)(RADAR_CENTRE_X * RADAR_CENTRE_X +
                                              RADAR_CENTRE_Y * RADAR_CENTRE_Y)) * 1.15f;
        DrawScanLines(backbuffer,
            RADAR_CENTRE_X, RADAR_CENTRE_Y,
            RADAR_CENTRE_X + std::cos(ang) * reach,
            RADAR_CENTRE_Y + std::sin(ang) * reach,
            20, 128, 5);
    }

    DrawRadarCircles(backbuffer);

    mutex_enter_blocking(&g_dataMutex);
    for (auto& [icao, tracked] : trackedAircraft) {
        if (tracked.state.onGround) continue;

        tracked.Tick();
        auto [predLat, predLon] = tracked.GetDisplayPosition();
        auto [x, y] = ProjectCoordinateToScreen(predLat, predLon);

        if (displayInfoText)
            DrawAircraftInfo(backbuffer, x, y, tracked);

        if (displayTriangles)
            DrawAircraftMarker(backbuffer, x, y, tracked);
        else
            backbuffer.fillCircle(x, y, 3, lgfx::color888(0, 255, 0));
    }
    mutex_exit(&g_dataMutex);
}

void AircraftManager::DrawRadarCircles(LGFX_Sprite& backbuffer) const
{
    constexpr int OUTER = RADAR_RADIUS - 1;

    backbuffer.drawCircle(RADAR_CENTRE_X, RADAR_CENTRE_Y, OUTER, lgfx::color888(0, 200, 0));
    backbuffer.drawCircle(RADAR_CENTRE_X, RADAR_CENTRE_Y, (OUTER / 3) * 2, lgfx::color888(0, 64, 0));
    backbuffer.drawCircle(RADAR_CENTRE_X, RADAR_CENTRE_Y, OUTER / 3, lgfx::color888(0, 32, 0));
}

std::pair<int, int> AircraftManager::ProjectCoordinateToScreen(float predLat, float predLon) const
{
    const float dLon = predLon - lon;
    const float dLat = predLat - lat;

    const float normLon = (dLon + rad) / (2.0f * rad);
    const float normLat = (dLat + rad) / (2.0f * rad);

    constexpr int DIAMETER = 2 * RADAR_RADIUS;
    const int x = (RADAR_CENTRE_X - RADAR_RADIUS) + static_cast<int>(normLon * DIAMETER);
    const int y = (RADAR_CENTRE_Y + RADAR_RADIUS) - static_cast<int>(normLat * DIAMETER);

    return { x, y };
}

void AircraftManager::DrawAircraftInfo(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const
{
    const int lineHeight = tft.fontHeight() + 1;

    backbuffer.setTextSize(1);
    backbuffer.setTextColor(lgfx::color888(0, 128, 0));
    backbuffer.drawString(tracked.state.callsign, x + 5, y + 5);
    backbuffer.drawString(String(tracked.state.velocity) + "m/s", x + 5, y + 5 + lineHeight);
    backbuffer.drawString(String(tracked.state.baroAltitude) + "m", x + 5, y + 5 + lineHeight * 2);
}

void AircraftManager::DrawAircraftMarker(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const
{
    const float dx = std::sin(radians(tracked.state.trueTrack));
    const float dy = -std::cos(radians(tracked.state.trueTrack));
    const float px = -dy, py = dx; // perpendicular

    // heading + speed "vector" line; length grows a little with velocity (clamped
    // so fast jets don't streak across the screen)
    float vlen = 6.0f + tracked.state.velocity * 0.12f;
    if (vlen > 18.0f) vlen = 18.0f;
    backbuffer.drawLine(x, y, x + dx * vlen, y + dy * vlen, lgfx::color888(0, 120, 0));

    // directional arrow pointing along the heading
    constexpr float LEN = 6.0f, WIDTH = 3.0f;
    backbuffer.fillTriangle(
        x + dx * LEN, y + dy * LEN,
        x - dx * LEN * 0.5f + px * WIDTH * 0.5f, y - dy * LEN * 0.5f + py * WIDTH * 0.5f,
        x - dx * LEN * 0.5f - px * WIDTH * 0.5f, y - dy * LEN * 0.5f - py * WIDTH * 0.5f,
        lgfx::color888(0, 255, 0));
}