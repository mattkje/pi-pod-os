#pragma once
#include <string>
#include "app/AppState.h"

namespace Config {
    // ~/.config/pi-pod-os (or $XDG_CONFIG_HOME/pi-pod-os)
    std::string configDir();

    // ~/.cache/pi-pod-os (or $XDG_CACHE_HOME/pi-pod-os)
    std::string cacheDir();

    // Directory holding bundled assets (fonts, icons). Next to the executable when possible.
    std::string assetsDir();

    Settings load();

    void save(const Settings &settings);

    // Deletes the config file and cached downloads.
    void reset();
}
