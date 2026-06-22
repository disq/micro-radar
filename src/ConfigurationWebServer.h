#pragma once

#include <WebServer.h>
#include "ConfigStore.h"

class ConfigurationWebServer {
private:
    WebServer server;
    ConfigStore& store;

    void HandleRoot();
    void HandleSave();

public:
    ConfigurationWebServer(ConfigStore& configStore) : server(80), store(configStore) {}
    ConfigurationWebServer(ConfigStore& configStore, int port) : server(port), store(configStore) {}

    void Initialise();
    void Handle();
    [[nodiscard]] const String GetStoredString(const char* key);
};
