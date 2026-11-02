#pragma once
#include <string>
#include <vector>

// OS integration used by Setup and Settings. On the device, privileged changes go through
// `sudo -n /usr/local/bin/pipod-system`; off the device (development on a computer) the calls are
// simulated so the UI can be exercised safely.
namespace SystemControl {
    struct WifiNetwork {
        std::string ssid;
        int signal = 0; // 0-100
        bool secured = false;
        bool active = false;
    };

    void setSimulated(bool simulated);
    bool simulated();

    // Each returns an empty string on success, otherwise a short error message.
    std::string setHostname(const std::string &name);
    std::string setTimezone(const std::string &tz);
    std::string setWifiCountry(const std::string &countryCode);
    std::string connectWifi(const std::string &ssid, const std::string &password);
    std::string setPassword(const std::string &password);

    std::vector<WifiNetwork> scanWifi(); // strongest first, duplicates merged
    bool online();                       // NetworkManager reports a connection

    // "Europe/Oslo", ... from /usr/share/zoneinfo, sorted.
    std::vector<std::string> timezones();
    std::string currentTimezone();
    // Two-letter country for a time zone (from zone.tab), or "" if unknown.
    std::string countryForTimezone(const std::string &tz);

    // "Matti's PiPod" -> "mattis-pipod"
    std::string hostnameFrom(const std::string &name);
}
