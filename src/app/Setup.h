#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <future>
#include <string>
#include <vector>
#include "app/AppState.h"
#include "system/SystemControl.h"
#include "ui/Utils.h"

// First-boot setup assistant, iPod style: Language, Name, Password, Time Zone, Wi-Fi, Jellyfin.
// Also reachable later from Settings -> Setup Assistant.
class SetupWizard {
public:
    struct Callbacks {
        std::function<void(const std::string &code)> setLanguage;
        std::function<void(const std::string &url, const std::string &user, const std::string &pass)> connectJellyfin;
        std::function<void()> finished; // settings were updated; save and leave the wizard
    };

    SetupWizard(Settings &settings, Callbacks callbacks);
    ~SetupWizard();

    void start();
    bool active() const { return step != Step::Inactive; }

    void handleKey(SDL_Keycode key);
    void handleText(const char *text);
    void update(); // poll background work; call once per frame
    void draw(const UI &ui, SDL_Texture *logo);

private:
    enum class Step {
        Inactive,
        Hello,
        Language,
        Name,
        Password,
        Region,
        City,
        Wifi,
        WifiPassword,
        Jellyfin,
        JellyfinUrl,
        JellyfinUser,
        JellyfinPass,
        Done
    };

    void go(Step next);
    void back();
    bool isPrompt() const;
    std::string &promptValue();
    void submitPrompt();
    void selectRow();
    int rowCount() const;
    std::vector<ListRow> rows() const;
    void startWifiScan();
    void runInBackground(std::function<std::string()> job);
    void toast(const std::string &message, Uint32 ms = 2500);

    void drawPrompt(const UI &ui, const std::string &title, const std::string &text, const std::string &value,
                    bool masked, const std::string &hint);

    Settings &settings;
    Callbacks cb;
    Step step = Step::Inactive;
    AppState list; // selection / scroll state for list steps

    std::string name, password, wifiPassword, jfUrl, jfUser, jfPass;
    std::vector<std::string> zones;
    std::vector<std::string> regions;
    std::string region;
    std::vector<std::string> cities; // full zone names in `region`

    std::vector<SystemControl::WifiNetwork> networks;
    std::future<std::vector<SystemControl::WifiNetwork>> scanTask;
    std::future<std::string> connectTask;
    std::string connectingTo;
    bool online = false;

    std::vector<std::future<std::string>> background; // fire-and-forget system changes

    std::string message;
    Uint32 messageUntil = 0;
    Uint32 helloStart = 0;
};
