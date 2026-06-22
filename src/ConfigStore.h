#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Persistent key/value config backed by a JSON file in LittleFS.
// Replaces the ESP-only Preferences API.
class ConfigStore {
private:
    JsonDocument doc;
    void Load();
    void Save();

public:
    void Begin();
    [[nodiscard]] String GetString(const char* key, const String& def = "");
    void PutString(const char* key, const String& value);
};
