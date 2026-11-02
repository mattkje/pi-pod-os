#pragma once
#include <string>
#include "ui/Utils.h"

// Shows where to open the web interface and the access code (Settings -> Add Media).
// urlByName / urlByIp are empty when unavailable; running = the web server is up.
void drawAddMediaPage(const UI &ui, bool running, const std::string &urlByName, const std::string &urlByIp,
                      const std::string &code);
