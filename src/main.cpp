#include <Arduino.h>
#include <ArduinoJson.h>

#include "LGFX.h"
#include "RadarLayout.h"
#include "Sync.h"
#include "StatusLed.h"
#include "ConfigStore.h"
#include "WiFiPortal.h"
#include "ConfigurationWebServer.h"
#include "HttpRequestManager.h"
#include "OpenSkyAuthTokenHandler.h"
#include "AircraftManager.h"
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

// Cross-core sync (declared extern in Sync.h). core0 runs WiFi + the blocking
// OpenSky fetch; core1 (loop1) draws the radar so the animation never stalls.
mutex_t g_dataMutex;
mutex_t g_displayMutex;
volatile bool g_radarActive = false;
bool core1_separate_stack = true; // give each core its own 8K stack (core0 needs it for TLS)

// Pico Display 2.0 front buttons (active-low, internal pull-ups)
constexpr int BTN_A = 12, BTN_B = 13, BTN_X = 14, BTN_Y = 15;

void setup()
{
  Serial.begin(115200);
  // give the USB serial monitor a moment to attach so early boot logs aren't lost
  const unsigned long serialDeadline = millis() + 2500;
  while (!Serial && millis() < serialDeadline)
    delay(10);
  Serial.println("\n[boot] Micro Radar starting...");

  mutex_init(&g_dataMutex);
  mutex_init(&g_displayMutex);

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);

  StatusLed::Begin(); // RGB LED off until a blink event

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

  // boot + first connect done - let core1 start drawing the radar
  g_radarActive = true;
}

void loop()
{
  wifiPortal.MaintainConnection();
  configServer.Handle();
  if (configServer.ConsumeSettingsChanged())
    aircraftManager.ReloadSettings();
  aircraftManager.Update();
}

// Poll the four front buttons (edge-detected, one action per press).
// A = toggle sweep, B = toggle aircraft labels, X = range +0.25, Y = range -0.25.
static void HandleButtons()
{
  const int pins[4] = { BTN_A, BTN_B, BTN_X, BTN_Y };
  static bool was[4] = { false, false, false, false };

  bool now[4];
  for (int i = 0; i < 4; i++)
    now[i] = digitalRead(pins[i]) == LOW; // active-low

  bool pressed = false;
  if (now[0] && !was[0]) { aircraftManager.ToggleScanline();   pressed = true; }
  if (now[1] && !was[1]) { aircraftManager.ToggleInfoText();   pressed = true; }
  if (now[2] && !was[2]) { aircraftManager.AdjustRadius(+0.25); pressed = true; }
  if (now[3] && !was[3]) { aircraftManager.AdjustRadius(-0.25); pressed = true; }

  if (pressed) StatusLed::Green();

  for (int i = 0; i < 4; i++) was[i] = now[i];
}

// core1: draw the radar. Runs independently of the core0 fetch, so the sweep and
// aircraft keep animating smoothly even while an OpenSky TLS request is blocking.
void loop1()
{
  if (!g_radarActive) {
    delay(5); // core0 is showing a status screen (booting / reconnecting)
    return;
  }

  HandleButtons();

  backbuffer.fillScreen(lgfx::color888(0, 0, 0));
  aircraftManager.Draw(backbuffer);

  mutex_enter_blocking(&g_displayMutex);
  backbuffer.pushSprite(0, 0);
  mutex_exit(&g_displayMutex);
}
