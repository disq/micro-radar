#include "ConfigurationWebServer.h"
#include <LEAmDNS.h>

// HTML stored in flash. %PLACEHOLDER% tokens are substituted at serve time.
static const char CONFIG_HTML[] PROGMEM = R"(
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Configure Micro Radar</title>
        <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4.3.0"></script>
    </head>
    <body class="font-mono bg-gray-900 text-green-500 min-h-screen p-4 sm:p-0 text-md sm:text-sm">
        <fieldset class="border border-green-500 p-5 w-full max-w-2xl mx-auto sm:m-10">
            <legend class="px-2">Configure Micro Radar</legend>

            <form id="cfg" action="/save" method="POST" class="flex flex-col gap-4 sm:gap-2">

                <div class="flex flex-col sm:flex-row gap-4 sm:gap-5">
                    <label class="flex flex-col sm:flex-row gap-2 flex-1">
                        <span>Latitude:</span>
                        <input
                            name="latitude"
                            type="number"
                            min="-90"
                            step="0.000001"
                            max="90"
                            value='%LATITUDE%'
                            class="border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                    </label>

                    <label class="flex flex-col sm:flex-row gap-2 flex-1">
                        <span>Longitude:</span>
                        <input
                            name="longitude"
                            type="number"
                            min="-180"
                            step="0.000001"
                            max="180"
                            value='%LONGITUDE%'
                            class="border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                    </label>
                </div>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>Radius (in &deg;):</span>
                    <input
                        name="radius"
                        type="number"
                        min="0.000001"
                        step="0.000001"
                        max="2.499999"
                        value='%RADIUS%'
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>OpenSkyAPI Client ID:</span>
                    <input
                        name="opensky-id"
                        value='%OPENSKY_ID%'
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>

                <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                    <span>OpenSkyAPI Client Secret:</span>
                    <input
                        name="opensky-secret"
                        value='%OPENSKY_SECRET%'
                        class="flex-1 border border-green-500 bg-gray-900 w-full px-3 py-2 text-lg sm:text-base sm:px-1 sm:py-0">
                </label>

                <div class="flex flex-col sm:flex-row gap-4 sm:justify-between">
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Radar sweep:</span>
                        <input
                            name="scanline"
                            type="checkbox"
                            %SCANLINE%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Aircraft Info:</span>
                        <input
                            name="infotext"
                            type="checkbox"
                            %INFOTEXT%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                    <label class="flex flex-col sm:flex-row items-start sm:items-center gap-2">
                        <span>Directional Aircraft:</span>
                        <input
                            name="triangle"
                            type="checkbox"
                            %TRIANGLE%
                            class="px-3 sm:px-1 accent-green-500">
                    </label>
                </div>

                <div class="flex flex-col sm:flex-row gap-4 sm:gap-5">
                    <input
                        type="submit"
                        value="Save"
                        class="bg-green-500 text-black mt-4 px-4 py-3 text-lg sm:text-base sm:px-2 sm:py-0 self-start cursor-pointer">

                        <div id="result" class="mt-4 px-1 sm:px-10"></div>
                </div>
            </form>
        </fieldset>

        <script>
            document.getElementById('cfg').addEventListener('submit', function(e) {
                e.preventDefault();
                fetch(this.action, { method: 'POST', body: new FormData(this) })
                    .then(r => r.text())
                    .then(html => document.getElementById('result').innerHTML = html);
            });
        </script>
    </body>
</html>
)";

void ConfigurationWebServer::Initialise() {
    // start mDNS so the config page is reachable at microradar.local
    if (!MDNS.begin("microradar")) {
        Serial.println("[WARN] Failed to start mDNS. Continuing without mDNS...");
    } else {
        MDNS.addService("http", "tcp", 80);
    }

    server.on("/", HTTP_GET, [this]() { HandleRoot(); });
    server.on("/save", HTTP_POST, [this]() { HandleSave(); });

    server.begin();
}

void ConfigurationWebServer::Handle() {
    server.handleClient();
    MDNS.update();
}

void ConfigurationWebServer::HandleRoot() {
    Serial.println("[GET] Handling request to config web server...");

    const String latitude = store.GetString("latitude", "");
    const String longitude = store.GetString("longitude", "");
    const String radius = store.GetString("radius", "1.0");
    const String openskyClientId = store.GetString("opensky-id", "");
    String openskySecret = store.GetString("opensky-secret", "");
    const String scanlineEnabled = store.GetString("scanline", "true");
    const String infoTextEnabled = store.GetString("infotext", "true");
    const String triangleEnabled = store.GetString("triangle", "true");

    // mask secret before sending to client
    for (size_t i = 0; i < openskySecret.length(); i++)
        openskySecret[i] = '*';

    String html = FPSTR(CONFIG_HTML);
    html.replace("%LATITUDE%", latitude);
    html.replace("%LONGITUDE%", longitude);
    html.replace("%RADIUS%", radius);
    html.replace("%OPENSKY_ID%", openskyClientId);
    html.replace("%OPENSKY_SECRET%", openskySecret);
    html.replace("%SCANLINE%", scanlineEnabled == "true" ? "checked" : "");
    html.replace("%INFOTEXT%", infoTextEnabled == "true" ? "checked" : "");
    html.replace("%TRIANGLE%", triangleEnabled == "true" ? "checked" : "");

    server.send(200, "text/html", html);
}

void ConfigurationWebServer::HandleSave() {
    Serial.println("[POST] Handling form submission to config web server...");

    auto TrySaveParam = [this](const char* paramName) {
        if (server.hasArg(paramName))
            store.PutString(paramName, server.arg(paramName));
    };

    TrySaveParam("latitude");
    TrySaveParam("longitude");
    TrySaveParam("radius");

    // OpenSky credentials affect auth + the daily request budget, so a change
    // there needs a reboot to re-auth cleanly. Location/radius/display options
    // are applied live (see ConsumeSettingsChanged + AircraftManager::ReloadSettings).
    bool credentialsChanged = false;

    if (server.hasArg("opensky-id")) {
        const String id = server.arg("opensky-id");
        if (id != store.GetString("opensky-id", ""))
            credentialsChanged = true;
        store.PutString("opensky-id", id);
    }

    if (server.hasArg("opensky-secret")) {
        const String secret = server.arg("opensky-secret");
        if (secret.indexOf('*') == -1) { // a real value, not the masked placeholder
            store.PutString("opensky-secret", secret);
            credentialsChanged = true;
        }
    }

    store.PutString("scanline", server.hasArg("scanline") ? "true" : "false");
    store.PutString("triangle", server.hasArg("triangle") ? "true" : "false");
    store.PutString("infotext", server.hasArg("infotext") ? "true" : "false");

    if (credentialsChanged) {
        server.send(200, "text/html", "Saved - restarting to apply credentials...");
        delay(500);
        rp2040.reboot();
    }

    settingsChanged = true;
    server.send(200, "text/html", "Saved.");
}

bool ConfigurationWebServer::ConsumeSettingsChanged() {
    if (!settingsChanged)
        return false;
    settingsChanged = false;
    return true;
}

const String ConfigurationWebServer::GetStoredString(const char* key)
{
    return store.GetString(key, "");
}
