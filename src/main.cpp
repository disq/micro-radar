#include <Arduino.h>
#include <ArduinoJson.h>

#include "LGFX.h"
#include "RadarLayout.h"
#include "ConfigStore.h"
#include "WiFiPortal.h"
#include "ConfigurationWebServer.h"
#include "HttpRequestManager.h"
#include "OpenSkyAuthTokenHandler.h"
#include "AircraftManager.h"
#include "DrawHelpers.h"
#include "models/Aircraft.h"
#include "models/TrackedAircraft.h"

LGFX tft;
LGFX_Sprite backbuffer(&tft);

ConfigStore config;
ConfigurationWebServer configServer(config);
WiFiPortal wifiPortal(config, tft);
HttpRequestManager http;
OpenSkyAuthTokenHandler authHandler(http);

AircraftManager aircraftManager(configServer, authHandler, http, tft);

void setup()
{
  Serial.begin(115200);
  // give the USB serial monitor a moment to attach so early boot logs aren't lost
  const unsigned long serialDeadline = millis() + 2500;
  while (!Serial && millis() < serialDeadline)
    delay(10);
  Serial.println("\n[boot] Micro Radar starting...");

  // initialise LGFX + screen
  tft.init();
  tft.setRotation(1); // ST7789 is natively 240x320 portrait; rotate to 320x240 landscape

  backbuffer.setColorDepth(8);
  backbuffer.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

  // load persisted config, then connect to WiFi (or run the setup portal)
  config.Begin();
  wifiPortal.AutoConnect();

  // begin background server for configuration
  configServer.Initialise();
  Serial.println("[boot] Config page: http://microradar.local/ (or the device IP)");

  // initialise aircraft manager
  aircraftManager.Initialise();
}

void loop()
{
  wifiPortal.MaintainConnection();
  configServer.Handle();
  aircraftManager.Update();

  // draw cycle
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));

  String renderScanlines = configServer.GetStoredString("scanline");
  if (renderScanlines.isEmpty() || renderScanlines == "true") {
    DrawScanLines(backbuffer,
      RADAR_CENTRE_X,
      RADAR_CENTRE_Y,
      RADAR_CENTRE_X + (std::cos(millis() / 3000.0f) * RADAR_RADIUS),
      RADAR_CENTRE_Y + (std::sin(millis() / 3000.0f) * RADAR_RADIUS),
      20, 128, 5
    );
  }

  aircraftManager.Draw(backbuffer);
  backbuffer.pushSprite(0, 0);
}
