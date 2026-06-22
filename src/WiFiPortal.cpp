#include "WiFiPortal.h"
#include "RadarLayout.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// %OPTIONS% is replaced with <option> tags for nearby networks (datalist).
static const char PORTAL_HTML[] PROGMEM = R"(
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Micro Radar - Setup WiFi</title>
        <style>
            body { background:#111; color:#0f0; font-family:monospace; padding:1rem; }
            fieldset { border:1px solid #0f0; max-width:32rem; margin:1rem auto; padding:1.25rem; }
            label { display:flex; flex-direction:column; gap:.25rem; margin-bottom:1rem; }
            input { background:#111; color:#0f0; border:1px solid #0f0; padding:.5rem; }
            input[type=submit] { background:#0f0; color:#000; cursor:pointer; }
        </style>
    </head>
    <body>
        <fieldset>
            <legend>Micro Radar - Setup WiFi</legend>
            <form action="/save" method="POST">
                <label>
                    <span>Network:</span>
                    <input name="ssid" list="networks" autocomplete="off">
                    <datalist id="networks">%OPTIONS%</datalist>
                </label>
                <label>
                    <span>Password:</span>
                    <input name="pass" type="password" autocomplete="off">
                </label>
                <input type="submit" value="Save & Connect">
            </form>
        </fieldset>
    </body>
</html>
)";

void WiFiPortal::AutoConnect()
{
    const String ssid = config.GetString("wifi-ssid");
    Serial.printf("[WiFi] Stored SSID: '%s'\n", ssid.c_str());

    // With no credentials there is nothing to retry, but we still don't auto-open
    // the portal - wait for the user to ask for it with BOOTSEL.
    if (ssid.isEmpty()) {
        Serial.println("[WiFi] No stored credentials. Hold BOOTSEL to open the setup portal.");
        ShowStatus("No WiFi configured", "Hold BOOTSEL", "to set up WiFi");
        while (!BOOTSEL)
            delay(50);
        RunConfigPortal(); // never returns (reboots after save)
    }

    // Never fall back to the hotspot on failure - just keep retrying forever.
    // Hold BOOTSEL during a connection attempt to (re)open the setup portal.
    int round = 0;
    while (true) {
        ShowStatus("Connecting to WiFi...", ssid, "hold BOOTSEL to set up");
        if (TryConnect(ssid, config.GetString("wifi-pass"), 30000)) {
            Serial.printf("[WiFi] Connected. IP=%s RSSI=%d dBm\n",
                WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
            return;
        }
        Serial.printf("[WiFi] Round %d failed - retrying (hold BOOTSEL for setup)...\n", ++round);
    }
}

void WiFiPortal::MaintainConnection()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    // Recover a dropped link through the SAME robust routine as the initial
    // connect (clean-state scan + strongest-BSSID lock + retry-forever, with
    // BOOTSEL still opening the setup portal) rather than a plain begin() that
    // the band-steering problem would defeat. Blocks until reconnected - which
    // is fine, there's no live data to show while the link is down anyway.
    Serial.println("[WiFi] Link dropped - reconnecting...");
    AutoConnect();
}

bool WiFiPortal::TryConnect(const String& ssid, const String& pass, uint32_t timeoutMs)
{
    // status legend: 0=IDLE 1=NO_SSID 3=CONNECTED 4=CONNECT_FAILED 6=DISCONNECTED
    Serial.printf("[WiFi] Connecting to '%s' with '%s' (Pico W is 2.4GHz only)...\n", ssid.c_str(), pass.c_str());

    WiFi.mode(WIFI_STA);
    delay(200);

    const uint32_t deadline = millis() + timeoutMs;

    // Scan from a clean STA state (NOT right after disconnect, which returns an
    // empty list) and lock onto the strongest 2.4GHz BSSID for this SSID. On a
    // mesh / band-steering network, letting the chip pick among same-SSID APs
    // often lands on one that steers us toward a 5GHz radio the Pico W can't use,
    // so association sticks at status 6. The scan only ever sees 2.4GHz here.
    uint8_t bestBssid[6];
    int32_t bestRssi = -1000;
    bool haveBssid = false;
    while (millis() < deadline && !haveBssid) {
        const int n = WiFi.scanNetworks();
        Serial.printf("[WiFi] scan returned %d network(s)\n", n);
        for (int i = 0; i < n; i++) {
            const char* found = WiFi.SSID(i);
            if (!found) found = "";
            const int32_t rssi = WiFi.RSSI(i);
            Serial.printf("   [%d] '%s' %d dBm\n", i, found, (int)rssi);
            if (ssid == found && rssi > bestRssi) {
                bestRssi = rssi;
                WiFi.BSSID(i, bestBssid);
                haveBssid = true;
            }
        }
        if (!haveBssid) {
            Serial.println("[WiFi] target SSID not in scan, rescanning...");
            if (BOOTSEL) RunConfigPortal();
            delay(500);
        }
    }

    WiFi.setHostname("microradar");
    WiFi.noLowPowerMode(); // disable CYW43 power-save - big reliability win on Pico W

    int attempt = 0;
    while (millis() < deadline) {
        if (haveBssid) {
            Serial.printf("[WiFi] begin() attempt %d -> BSSID %02x:%02x:%02x:%02x:%02x:%02x (%d dBm)\n",
                ++attempt, bestBssid[0], bestBssid[1], bestBssid[2], bestBssid[3], bestBssid[4],
                bestBssid[5], (int)bestRssi);
            WiFi.begin(ssid.c_str(), pass.c_str(), bestBssid);
        } else {
            Serial.printf("[WiFi] begin() attempt %d (no BSSID lock)\n", ++attempt);
            WiFi.begin(ssid.c_str(), pass.c_str());
        }

        uint32_t attemptDeadline = millis() + 8000;
        if (attemptDeadline > deadline) attemptDeadline = deadline;
        uint32_t lastPrint = 0;
        while (WiFi.status() != WL_CONNECTED && millis() < attemptDeadline) {
            if (BOOTSEL)
                RunConfigPortal(); // user asked for the setup portal; never returns
            if (millis() - lastPrint >= 1000) {
                Serial.printf("[WiFi] ...status=%d\n", WiFi.status());
                lastPrint = millis();
            }
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED)
            return true;

        WiFi.disconnect();
        delay(200);
    }

    return false;
}

void WiFiPortal::RunConfigPortal()
{
    // scan for nearby networks before switching the radio into AP mode
    const int networkCount = WiFi.scanNetworks();
    Serial.printf("[WiFi] Scan found %d network(s):\n", networkCount);
    String options;
    for (int i = 0; i < networkCount; i++) {
        const String ssid = WiFi.SSID(i);
        Serial.printf("  %2d: %-32s %d dBm\n", i, ssid.c_str(), (int)WiFi.RSSI(i));
        if (!ssid.isEmpty())
            options += "<option value='" + ssid + "'>";
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(PortalName);
    const IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WiFi] Setup portal AP '%s' started at http://%s/\n",
        PortalName, apIP.toString().c_str());

    DNSServer dns;
    dns.start(53, "*", apIP); // capture every lookup so any URL opens the portal

    WebServer server(80);

    auto servePage = [&]() {
        String html = FPSTR(PORTAL_HTML);
        html.replace("%OPTIONS%", options);
        server.send(200, "text/html", html);
    };

    server.on("/", HTTP_GET, servePage);
    server.onNotFound(servePage);
    server.on("/save", HTTP_POST, [&]() {
        Serial.printf("[WiFi] Saving SSID '%s', rebooting...\n", server.arg("ssid").c_str());
        config.PutString("wifi-ssid", server.arg("ssid"));
        config.PutString("wifi-pass", server.arg("pass"));
        server.send(200, "text/html",
            "<html><body style='background:#111;color:#0f0;font-family:monospace'>"
            "Saved. Restarting...</body></html>");
        delay(1500);
        rp2040.reboot();
    });

    server.begin();

    ShowStatus("- SETUP -", "Connect to this WiFi hotspot:", PortalName);

    while (true) {
        dns.processNextRequest();
        server.handleClient();
    }
}

void WiFiPortal::ShowStatus(const char* line1, const String& line2, const String& line3)
{
    tft.fillScreen(lgfx::color888(0, 0, 0));
    tft.setTextColor(lgfx::color888(0, 255, 0));

    const int cx = SCREEN_WIDTH / 2;
    const int cy = SCREEN_HEIGHT / 2;
    const int lineHeight = tft.fontHeight() + 10;

    tft.drawCenterString(line1, cx, cy - lineHeight);
    if (!line2.isEmpty()) tft.drawCenterString(line2, cx, cy);
    if (!line3.isEmpty()) tft.drawCenterString(line3, cx, cy + lineHeight);
}
