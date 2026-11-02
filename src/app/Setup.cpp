#include "app/Setup.h"
#include "app/I18n.h"
#include <algorithm>

namespace {
    constexpr int ROW_HEIGHT = 23;

    // "Argentina/Buenos_Aires" -> "Argentina / Buenos Aires"
    std::string prettyZone(const std::string &zone) {
        std::string s;
        for (char c: zone) {
            if (c == '_') s += ' ';
            else if (c == '/') s += " / ";
            else s += c;
        }
        return s;
    }

    // Greetings cycled on the Hello screen.
    const std::vector<std::string> &greetings() {
        static const std::vector<std::string> g{"Hello", "Hei", "Hej", "Hallo", "Bonjour", "Hola"};
        return g;
    }
}

SetupWizard::SetupWizard(Settings &s, Callbacks callbacks) : settings(s), cb(std::move(callbacks)) {}

SetupWizard::~SetupWizard() {
    for (auto &f: background) f.wait();
    if (scanTask.valid()) scanTask.wait();
    if (connectTask.valid()) connectTask.wait();
}

void SetupWizard::start() {
    name = settings.deviceName.empty() ? "PiPod" : settings.deviceName;
    password.clear();
    jfUrl = settings.jellyfinUrl.empty() ? "http://" : settings.jellyfinUrl;
    jfUser = settings.jellyfinUser;
    jfPass.clear();
    zones = SystemControl::timezones();
    regions.clear();
    for (const auto &z: zones) {
        std::string area = z.substr(0, z.find('/'));
        if (std::find(regions.begin(), regions.end(), area) == regions.end()) regions.push_back(area);
    }
    helloStart = SDL_GetTicks();
    go(Step::Hello);
}

void SetupWizard::toast(const std::string &m, Uint32 ms) {
    message = m;
    messageUntil = SDL_GetTicks() + ms;
}

bool SetupWizard::isPrompt() const {
    return step == Step::Name || step == Step::Password || step == Step::WifiPassword ||
           step == Step::JellyfinUrl || step == Step::JellyfinUser || step == Step::JellyfinPass;
}

std::string &SetupWizard::promptValue() {
    switch (step) {
        case Step::Name: return name;
        case Step::Password: return password;
        case Step::WifiPassword: return wifiPassword;
        case Step::JellyfinUrl: return jfUrl;
        case Step::JellyfinUser: return jfUser;
        default: return jfPass;
    }
}

void SetupWizard::go(Step next) {
    step = next;
    list.selected = 0;
    list.scrollTop = 0;
    list.visualOffset = 0;

    if (isPrompt()) SDL_StartTextInput();
    else SDL_StopTextInput();

    switch (step) {
        case Step::Language: {
            const auto &langs = I18n::languages();
            for (size_t i = 0; i < langs.size(); i++)
                if (langs[i].code == I18n::current()) list.selected = static_cast<int>(i);
            break;
        }
        case Step::Region: {
            std::string tz = settings.timezone.empty() ? SystemControl::currentTimezone() : settings.timezone;
            if (tz.empty()) tz = "Europe/Oslo";
            std::string area = tz.substr(0, tz.find('/'));
            auto it = std::find(regions.begin(), regions.end(), area);
            if (it != regions.end()) list.selected = static_cast<int>(it - regions.begin());
            break;
        }
        case Step::City: {
            cities.clear();
            for (const auto &z: zones)
                if (z.rfind(region + "/", 0) == 0) cities.push_back(z);
            std::string tz = settings.timezone.empty() ? SystemControl::currentTimezone() : settings.timezone;
            auto it = std::find(cities.begin(), cities.end(), tz);
            if (it != cities.end()) list.selected = static_cast<int>(it - cities.begin());
            break;
        }
        case Step::Wifi:
            startWifiScan();
            break;
        case Step::WifiPassword:
            wifiPassword.clear();
            break;
        default:
            break;
    }
}

void SetupWizard::back() {
    switch (step) {
        case Step::Language: go(Step::Hello); break;
        case Step::Name: go(Step::Language); break;
        case Step::Password: go(Step::Name); break;
        case Step::Region: go(Step::Password); break;
        case Step::City: go(Step::Region); break;
        case Step::Wifi: go(Step::Region); break;
        case Step::WifiPassword: go(Step::Wifi); break;
        case Step::Jellyfin: go(Step::Wifi); break;
        case Step::JellyfinUrl: go(Step::Jellyfin); break;
        case Step::JellyfinUser: go(Step::JellyfinUrl); break;
        case Step::JellyfinPass: go(Step::JellyfinUser); break;
        case Step::Done: go(Step::Jellyfin); break;
        default: break;
    }
}

void SetupWizard::runInBackground(std::function<std::string()> job) {
    background.push_back(std::async(std::launch::async, std::move(job)));
}

void SetupWizard::startWifiScan() {
    if (scanTask.valid()) return;
    scanTask = std::async(std::launch::async, []() { return SystemControl::scanWifi(); });
    online = SystemControl::online();
}

int SetupWizard::rowCount() const {
    return static_cast<int>(rows().size());
}

std::vector<ListRow> SetupWizard::rows() const {
    std::vector<ListRow> r;
    switch (step) {
        case Step::Language:
            for (const auto &l: I18n::languages()) r.push_back({l.nativeName, ""});
            break;
        case Step::Region:
            for (const auto &reg: regions) r.push_back({tr(reg), "", true});
            break;
        case Step::City:
            for (const auto &c: cities) r.push_back({prettyZone(c.substr(region.size() + 1)), ""});
            break;
        case Step::Wifi:
            r.push_back({online ? tr("Continue") : tr("Skip"), online ? tr("Connected") : "", false, true});
            for (const auto &n: networks) {
                std::string detail = n.active ? tr("Connected")
                                              : (n.secured ? tr("Secured") + "  " : "") + std::to_string(n.signal) + "%";
                r.push_back({n.ssid, detail, false, true});
            }
            break;
        case Step::Jellyfin:
            r.push_back({tr("Connect to Jellyfin"), "", true});
            r.push_back({tr("Skip"), ""});
            break;
        default:
            break;
    }
    return r;
}

void SetupWizard::selectRow() {
    int i = list.selected;
    switch (step) {
        case Step::Hello:
            go(Step::Language);
            break;
        case Step::Language: {
            const auto &langs = I18n::languages();
            if (i >= 0 && i < static_cast<int>(langs.size())) {
                settings.language = langs[i].code;
                if (cb.setLanguage) cb.setLanguage(settings.language);
            }
            go(Step::Name);
            break;
        }
        case Step::Region:
            if (i >= 0 && i < static_cast<int>(regions.size())) {
                region = regions[i];
                go(Step::City);
            }
            break;
        case Step::City:
            if (i >= 0 && i < static_cast<int>(cities.size())) {
                std::string tz = cities[i];
                settings.timezone = tz;
                std::string cc = SystemControl::countryForTimezone(tz);
                runInBackground([tz, cc]() {
                    std::string err = SystemControl::setTimezone(tz);
                    // Wi-Fi stays blocked until a regulatory country is set.
                    if (err.empty() && !cc.empty()) err = SystemControl::setWifiCountry(cc);
                    return err;
                });
                go(Step::Wifi);
            }
            break;
        case Step::Wifi:
            if (i == 0) {
                go(Step::Jellyfin);
            } else if (i - 1 < static_cast<int>(networks.size())) {
                const auto &n = networks[i - 1];
                connectingTo = n.ssid;
                if (n.active) {
                    go(Step::Jellyfin);
                } else if (n.secured) {
                    go(Step::WifiPassword);
                } else {
                    std::string ssid = n.ssid;
                    connectTask = std::async(std::launch::async, [ssid]() { return SystemControl::connectWifi(ssid, ""); });
                }
            }
            break;
        case Step::Jellyfin:
            if (i == 0) go(Step::JellyfinUrl);
            else go(Step::Done);
            break;
        case Step::Done:
            step = Step::Inactive;
            SDL_StopTextInput();
            settings.setupDone = true;
            if (cb.finished) cb.finished();
            break;
        default:
            break;
    }
}

void SetupWizard::submitPrompt() {
    switch (step) {
        case Step::Name: {
            if (name.empty()) {
                toast(tr("Please enter a name"));
                return;
            }
            settings.deviceName = name;
            std::string host = SystemControl::hostnameFrom(name);
            runInBackground([host]() { return SystemControl::setHostname(host); });
            go(Step::Password);
            break;
        }
        case Step::Password:
            if (!password.empty()) {
                std::string pw = password;
                runInBackground([pw]() { return SystemControl::setPassword(pw); });
            }
            password.clear();
            go(Step::Region);
            break;
        case Step::WifiPassword: {
            std::string ssid = connectingTo, pw = wifiPassword;
            connectTask = std::async(std::launch::async, [ssid, pw]() { return SystemControl::connectWifi(ssid, pw); });
            break;
        }
        case Step::JellyfinUrl:
            go(Step::JellyfinUser);
            break;
        case Step::JellyfinUser:
            go(Step::JellyfinPass);
            break;
        case Step::JellyfinPass:
            if (cb.connectJellyfin) cb.connectJellyfin(jfUrl, jfUser, jfPass);
            jfPass.clear();
            go(Step::Done);
            break;
        default:
            break;
    }
}

void SetupWizard::handleText(const char *text) {
    if (isPrompt() && !connectTask.valid()) promptValue() += text;
}

void SetupWizard::handleKey(SDL_Keycode key) {
    if (connectTask.valid()) return; // wait for the Wi-Fi attempt

    if (isPrompt()) {
        std::string &v = promptValue();
        switch (key) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                submitPrompt();
                break;
            case SDLK_ESCAPE:
                back();
                break;
            case SDLK_BACKSPACE:
                while (!v.empty()) {
                    unsigned char c = static_cast<unsigned char>(v.back());
                    v.pop_back();
                    if ((c & 0xC0) != 0x80) break;
                }
                break;
            case SDLK_v:
                if (SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) {
                    if (char *clip = SDL_GetClipboardText()) {
                        v += clip;
                        SDL_free(clip);
                    }
                }
                break;
            default:
                break;
        }
        return;
    }

    int count = rowCount();
    switch (key) {
        case SDLK_UP:
            if (count > 0) list.selected = std::max(0, list.selected - 1);
            break;
        case SDLK_DOWN:
            if (count > 0) list.selected = std::min(count - 1, list.selected + 1);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_RIGHT:
            selectRow();
            break;
        case SDLK_BACKSPACE:
        case SDLK_LEFT:
        case SDLK_ESCAPE:
            back();
            break;
        default:
            break;
    }
}

void SetupWizard::update() {
    if (scanTask.valid() && scanTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        networks = scanTask.get();
        for (const auto &n: networks) online = online || n.active;
        list.selected = std::min(list.selected, std::max(0, rowCount() - 1));
    }
    if (connectTask.valid() && connectTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::string err = connectTask.get();
        wifiPassword.clear();
        if (err.empty()) {
            online = true;
            toast(trf("Connected to {}", {connectingTo}));
            go(Step::Jellyfin);
        } else {
            toast(tr("Couldn't connect. Check the password."), 3500);
            go(Step::Wifi);
        }
    }
    for (auto it = background.begin(); it != background.end();) {
        if (it->wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        std::string err = it->get();
        if (!err.empty()) toast(err, 3500);
        it = background.erase(it);
    }
    if (messageUntil && SDL_GetTicks() > messageUntil) {
        message.clear();
        messageUntil = 0;
    }
}

void SetupWizard::drawPrompt(const UI &ui, const std::string &title, const std::string &text,
                             const std::string &value, bool masked, const std::string &hint) {
    SDL_Renderer *r = ui.r;
    drawTopBar(ui, title);

    int y = TOP_BAR_HEIGHT + 16;
    int lineH = TTF_FontHeight(ui.font);
    y += drawText(r, ui.medium ? ui.medium : ui.font, text, TEXT_COLOR, 14, y, 0, ui.w - 28).y + 14;

    SDL_Rect field{12, y, ui.w - 24, lineH + 10};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &field);
    SDL_SetRenderDrawColor(r, 120, 150, 220, 255);
    SDL_RenderDrawRect(r, &field);

    std::string shown = masked ? std::string(value.size(), '*') : value;
    int maxW = field.w - 16, w = 0;
    while (!shown.empty() && TTF_SizeUTF8(ui.font, (shown + "|").c_str(), &w, nullptr) == 0 && w > maxW)
        shown.erase(0, 1);
    bool cursor = (SDL_GetTicks() / 500) % 2 == 0;
    drawText(r, ui.font, shown + (cursor ? "|" : ""), TEXT_COLOR, field.x + 8, field.y + 5);

    drawText(r, ui.small, hint, SUBTEXT_COLOR, ui.w / 2, ui.h - TTF_FontHeight(ui.small) - 10, 1, ui.w - 20);
}

void SetupWizard::draw(const UI &ui, SDL_Texture *logo) {
    SDL_Renderer *r = ui.r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(r);

    TTF_Font *big = ui.bold ? ui.bold : ui.font;
    switch (step) {
        case Step::Hello: {
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            SDL_RenderClear(r);
            int size = ui.h * 30 / 100;
            if (logo) {
                SDL_Rect dst{(ui.w - size) / 2, ui.h / 2 - size + 10, size, size};
                SDL_RenderCopy(r, logo, nullptr, &dst);
            }
            const auto &g = greetings();
            Uint32 t = SDL_GetTicks() - helloStart;
            size_t idx = (t / 1600) % g.size();
            Uint32 phase = t % 1600;
            Uint8 a = phase < 300 ? Uint8(255 * phase / 300) : phase > 1300 ? Uint8(255 * (1600 - phase) / 300) : 255;
            drawText(r, big, g[idx], {255, 255, 255, a}, ui.w / 2, ui.h / 2 + 24, 1);
            drawText(r, ui.small, tr("Press Select to begin"), {150, 150, 150, 255}, ui.w / 2,
                     ui.h - TTF_FontHeight(ui.small) - 12, 1);
            break;
        }
        case Step::Language:
            drawTopBar(ui, tr("Language"));
            drawList(ui, list, rows(), 0, ui.w, ROW_HEIGHT);
            break;
        case Step::Name:
            drawPrompt(ui, tr("Name"), tr("Give your PiPod a name."), name, false, tr("Enter: next   Esc: back"));
            break;
        case Step::Password:
            drawPrompt(ui, tr("Password"), tr("Choose a password for SSH. Leave empty to skip."), password, true,
                       tr("Enter: next   Esc: back"));
            break;
        case Step::Region:
            drawTopBar(ui, tr("Time Zone"));
            drawList(ui, list, rows(), 0, ui.w, ROW_HEIGHT);
            break;
        case Step::City:
            drawTopBar(ui, tr(region));
            drawList(ui, list, rows(), 0, ui.w, ROW_HEIGHT);
            break;
        case Step::Wifi:
            drawTopBar(ui, tr("Wi-Fi"));
            drawList(ui, list, rows(), 0, ui.w, ROW_HEIGHT);
            if (scanTask.valid())
                drawText(r, ui.small, tr("Searching for networks..."), SUBTEXT_COLOR, ui.w / 2,
                         ui.h - TTF_FontHeight(ui.small) - 10, 1);
            break;
        case Step::WifiPassword:
            drawPrompt(ui, tr("Wi-Fi"), trf("Password for \"{}\"", {connectingTo}), wifiPassword, true,
                       tr("Enter: join   Esc: back"));
            break;
        case Step::Jellyfin:
            drawTopBar(ui, tr("Jellyfin"));
            drawList(ui, list, rows(), 0, ui.w, ROW_HEIGHT);
            drawText(r, ui.small, tr("Stream music from your Jellyfin server."), SUBTEXT_COLOR, ui.w / 2,
                     ui.h - TTF_FontHeight(ui.small) - 10, 1, ui.w - 20);
            break;
        case Step::JellyfinUrl:
            drawPrompt(ui, tr("Jellyfin"), tr("Server address"), jfUrl, false, tr("Enter: next   Esc: back"));
            break;
        case Step::JellyfinUser:
            drawPrompt(ui, tr("Jellyfin"), tr("Username"), jfUser, false, tr("Enter: next   Esc: back"));
            break;
        case Step::JellyfinPass:
            drawPrompt(ui, tr("Jellyfin"), tr("Password"), jfPass, true, tr("Enter: finish   Esc: back"));
            break;
        case Step::Done: {
            drawTopBar(ui, tr("Setup"));
            int y = ui.h / 2 - 30;
            drawText(r, big, tr("You're all set"), TEXT_COLOR, ui.w / 2, y, 1, ui.w - 20);
            drawText(r, ui.medium ? ui.medium : ui.font, trf("Enjoy your {}.", {settings.deviceName}),
                     SUBTEXT_COLOR, ui.w / 2, y + 28, 1, ui.w - 20);
            drawText(r, ui.small, tr("Press Select to start"), SUBTEXT_COLOR, ui.w / 2,
                     ui.h - TTF_FontHeight(ui.small) - 12, 1);
            break;
        }
        default:
            break;
    }

    if (connectTask.valid()) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
        SDL_Rect all{0, TOP_BAR_HEIGHT, ui.w, ui.h - TOP_BAR_HEIGHT};
        SDL_RenderFillRect(r, &all);
        drawText(r, ui.font, trf("Joining \"{}\"...", {connectingTo}), {255, 255, 255, 255}, ui.w / 2,
                 ui.h / 2 - 10, 1, ui.w - 20);
    }
    drawStatus(ui, message);
}
