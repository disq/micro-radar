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

void AircraftManager::ToggleScanline()
{
    mutex_enter_blocking(&g_dataMutex);
    displayScanline = !displayScanline;
    mutex_exit(&g_dataMutex);
}

void AircraftManager::ToggleInfoText()
{
    mutex_enter_blocking(&g_dataMutex);
    displayInfoText = !displayInfoText;
    mutex_exit(&g_dataMutex);
}

void AircraftManager::AdjustRadius(double delta)
{
    mutex_enter_blocking(&g_dataMutex);
    rad += delta;
    if (rad < 0.25) rad = 0.25;
    if (rad > 2.5) rad = 2.5;
    trackedAircraft.clear(); // old planes belong to the old area
    forceFetch = true;       // refetch the new bounding box
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
              {"lomax", String(lon + rad)},
              {"extended", "1"} // include the ADS-B emitter category (state vector [17])
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

    // range readout in the top-left corner (outside the radar circle)
    backbuffer.setTextSize(1);
    backbuffer.setTextColor(lgfx::color888(0, 160, 0));
    backbuffer.drawString("RNG " + String(rad, 2), 4, 4);

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

    String label = tracked.state.callsign;
    label.trim();
    backbuffer.drawString(label, x + 5, y + 5);
    backbuffer.drawString(String(tracked.state.velocity) + "m/s", x + 5, y + 5 + lineHeight);
    backbuffer.drawString(String(tracked.state.baroAltitude) + "m", x + 5, y + 5 + lineHeight * 2);
}

// OpenSky ADS-B emitter category (state vector [17]) grouped into marker shapes.
enum class MarkerType { Default, Commercial, Helicopter, Light, Drone };

static MarkerType ClassifyCategory(int category)
{
    switch (category) {
        case 4: case 5: case 6: case 7: return MarkerType::Commercial; // large / heavy / high-perf
        case 8:                          return MarkerType::Helicopter; // rotorcraft
        case 2: case 3: case 9: case 12: return MarkerType::Light;      // light / small / glider / ultralight
        case 14:                         return MarkerType::Drone;      // UAV
        default:                         return MarkerType::Default;    // 0/1 unknown, everything else
    }
}

// An ICAO airline callsign is 3 letters + a flight number (e.g. BAW33K), versus a
// tail registration which is letters only (e.g. GBKBW). Used as a type hint.
static bool IsAirlineCallsign(const String& cs)
{
    if (cs.length() < 4) return false;
    if (!isAlpha(cs[0]) || !isAlpha(cs[1]) || !isAlpha(cs[2])) return false;
    for (unsigned int i = 3; i < cs.length(); i++)
        if (isDigit(cs[i])) return true;
    return false;
}

// OpenSky's emitter category is sparse, so fall back to callsign + kinematics when
// it's absent (0/1). Real category data always wins when present.
static MarkerType ClassifyAircraft(const Aircraft& s)
{
    if (s.category >= 2) return ClassifyCategory(s.category);

    String cs = s.callsign;
    cs.trim();
    if (IsAirlineCallsign(cs) || (s.baroAltitude > 6000.0f && s.velocity > 150.0f))
        return MarkerType::Commercial;
    if (s.baroAltitude > 0.0f && s.baroAltitude < 3000.0f && s.velocity < 100.0f)
        return MarkerType::Light;
    return MarkerType::Default;
}

// Draws a forward-pointing arrow oriented along the heading unit vector (dx, dy).
static void DrawArrow(LGFX_Sprite& buf, int x, int y, float dx, float dy, float len, float width, uint32_t colour)
{
    const float px = -dy, py = dx; // perpendicular
    buf.fillTriangle(
        x + dx * len, y + dy * len,
        x - dx * len * 0.5f + px * width * 0.5f, y - dy * len * 0.5f + py * width * 0.5f,
        x - dx * len * 0.5f - px * width * 0.5f, y - dy * len * 0.5f - py * width * 0.5f,
        colour);
}

void AircraftManager::DrawAircraftMarker(LGFX_Sprite& backbuffer, int x, int y, const TrackedAircraft& tracked) const
{
    const float dx = std::sin(radians(tracked.state.trueTrack));
    const float dy = -std::cos(radians(tracked.state.trueTrack));
    const uint32_t green = lgfx::color888(0, 255, 0);

    // heading + speed "vector" line so even non-pointy bodies show where it's going;
    // length grows a little with velocity (clamped so fast jets don't streak across)
    float vlen = 6.0f + tracked.state.velocity * 0.12f;
    if (vlen > 18.0f) vlen = 18.0f;
    backbuffer.drawLine(x, y, x + dx * vlen, y + dy * vlen, lgfx::color888(0, 120, 0));

    switch (ClassifyAircraft(tracked.state)) {
        case MarkerType::Commercial:
            DrawArrow(backbuffer, x, y, dx, dy, 9.0f, 5.0f, green);
            break;
        case MarkerType::Helicopter:
            backbuffer.fillCircle(x, y, 3, green);
            backbuffer.drawLine(x - 5, y, x + 5, y, green); // rotor cross
            backbuffer.drawLine(x, y - 5, x, y + 5, green);
            break;
        case MarkerType::Light:
            DrawArrow(backbuffer, x, y, dx, dy, 5.0f, 3.0f, green);
            break;
        case MarkerType::Drone:
            backbuffer.fillRect(x - 3, y - 3, 6, 6, green);
            break;
        case MarkerType::Default:
        default:
            DrawArrow(backbuffer, x, y, dx, dy, 6.0f, 3.0f, green);
            break;
    }
}