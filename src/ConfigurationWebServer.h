#pragma once

#include <WebServer.h>
#include "ConfigStore.h"

class ConfigurationWebServer {
private:
    WebServer server;
    ConfigStore& store;
    bool settingsChanged = false;

    void HandleRoot();
    void HandleSave();

public:
    ConfigurationWebServer(ConfigStore& configStore) : server(80), store(configStore) {}
    ConfigurationWebServer(ConfigStore& configStore, int port) : server(port), store(configStore) {}

    void Initialise();
    void Handle();
    bool ConsumeSettingsChanged(); // true once after non-credential settings are saved
    [[nodiscard]] const String GetStoredString(const char* key);
};
