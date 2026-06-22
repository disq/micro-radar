#include "ConfigStore.h"
#include <LittleFS.h>

static const char* CONFIG_PATH = "/config.json";

void ConfigStore::Begin()
{
    if (!LittleFS.begin()) {
        Serial.println("[WARN] LittleFS mount failed, formatting...");
        LittleFS.format();
        LittleFS.begin();
    }
    Load();
}

void ConfigStore::Load()
{
    File file = LittleFS.open(CONFIG_PATH, "r");
    if (!file)
        return;

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("[WARN] Failed to parse config, starting fresh: ");
        Serial.println(error.f_str());
        doc.clear();
    }
}

void ConfigStore::Save()
{
    File file = LittleFS.open(CONFIG_PATH, "w");
    if (!file) {
        Serial.println("[ERROR] Failed to open config for writing");
        return;
    }

    serializeJson(doc, file);
    file.close();
}

String ConfigStore::GetString(const char* key, const String& def)
{
    if (doc[key].isNull())
        return def;
    return doc[key].as<String>();
}

void ConfigStore::PutString(const char* key, const String& value)
{
    doc[key] = value;
    Save();
}
