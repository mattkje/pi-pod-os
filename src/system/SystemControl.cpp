#include "system/SystemControl.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
    bool isSimulated = true;

    // Runs argv without a shell, feeding `input` on stdin. Returns the exit code (or -1).
    int run(const std::vector<std::string> &argv, const std::string &input = "", std::string *output = nullptr) {
        int in[2], out[2];
        if (pipe(in) != 0) return -1;
        if (pipe(out) != 0) {
            close(in[0]);
            close(in[1]);
            return -1;
        }
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            dup2(in[0], STDIN_FILENO);
            dup2(out[1], STDOUT_FILENO);
            dup2(out[1], STDERR_FILENO);
            close(in[0]);
            close(in[1]);
            close(out[0]);
            close(out[1]);
            std::vector<char *> args;
            for (const auto &a: argv) args.push_back(const_cast<char *>(a.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            _exit(127);
        }
        close(in[0]);
        close(out[1]);
        if (!input.empty()) {
            ssize_t ignored = write(in[1], input.data(), input.size());
            (void) ignored;
        }
        close(in[1]);
        std::string collected;
        char buf[1024];
        ssize_t n;
        while ((n = read(out[0], buf, sizeof(buf))) > 0) collected.append(buf, static_cast<size_t>(n));
        close(out[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        if (output) *output = collected;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    std::string privileged(const std::vector<std::string> &args, const std::string &input = "") {
        if (isSimulated) {
            std::string joined;
            for (const auto &a: args) joined += a + " ";
            SDL_Log("[simulated] pipod-system %s", joined.c_str());
            SDL_Delay(400);
            return {};
        }
        std::vector<std::string> argv{"sudo", "-n", "/usr/local/bin/pipod-system"};
        argv.insert(argv.end(), args.begin(), args.end());
        std::string out;
        int rc = run(argv, input, &out);
        if (rc == 0) return {};
        // Last non-empty line of output is the most useful message.
        std::istringstream lines(out);
        std::string line, last;
        while (std::getline(lines, line))
            if (!line.empty()) last = line;
        return last.empty() ? "Failed" : last;
    }

    // Splits an `nmcli -t` line on unescaped ':' and unescapes "\:" and "\\".
    std::vector<std::string> splitTerse(const std::string &line) {
        std::vector<std::string> fields(1);
        for (size_t i = 0; i < line.size(); i++) {
            char c = line[i];
            if (c == '\\' && i + 1 < line.size()) {
                fields.back() += line[++i];
            } else if (c == ':') {
                fields.emplace_back();
            } else {
                fields.back() += c;
            }
        }
        return fields;
    }
}

void SystemControl::setSimulated(bool s) { isSimulated = s; }
bool SystemControl::simulated() { return isSimulated; }

std::string SystemControl::setHostname(const std::string &name) { return privileged({"hostname", name}); }
std::string SystemControl::setTimezone(const std::string &tz) { return privileged({"timezone", tz}); }
std::string SystemControl::setWifiCountry(const std::string &cc) { return privileged({"wifi-country", cc}); }
std::string SystemControl::setPassword(const std::string &pw) { return privileged({"password"}, pw + "\n"); }

std::string SystemControl::connectWifi(const std::string &ssid, const std::string &password) {
    if (isSimulated && password == "wrong") return "Secrets were required, but not provided";
    return privileged({"wifi-connect", ssid}, password + "\n");
}

std::vector<SystemControl::WifiNetwork> SystemControl::scanWifi() {
    if (isSimulated) {
        SDL_Delay(800);
        return {{"Home Network", 82, true, false}, {"Cafe Guest", 54, false, false}, {"Neighbour 5G", 31, true, false}};
    }
    std::string out;
    if (run({"nmcli", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi", "list", "--rescan", "yes"}, "", &out) != 0)
        run({"nmcli", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi", "list"}, "", &out);

    std::map<std::string, WifiNetwork> best;
    std::istringstream lines(out);
    std::string line;
    while (std::getline(lines, line)) {
        auto f = splitTerse(line);
        if (f.size() < 4 || f[1].empty()) continue; // hidden networks have no SSID
        WifiNetwork n;
        n.active = f[0] == "*";
        n.ssid = f[1];
        n.signal = std::atoi(f[2].c_str());
        n.secured = !f[3].empty() && f[3] != "--";
        auto it = best.find(n.ssid);
        if (it == best.end() || n.signal > it->second.signal) {
            n.active = n.active || (it != best.end() && it->second.active);
            best[n.ssid] = n;
        } else if (n.active) {
            it->second.active = true;
        }
    }
    std::vector<WifiNetwork> list;
    for (auto &kv: best) list.push_back(kv.second);
    std::sort(list.begin(), list.end(), [](const WifiNetwork &a, const WifiNetwork &b) {
        if (a.active != b.active) return a.active;
        return a.signal > b.signal;
    });
    return list;
}

bool SystemControl::online() {
    if (isSimulated) return false;
    std::string out;
    if (run({"nmcli", "-t", "-f", "STATE", "general"}, "", &out) != 0) return false;
    return out.rfind("connected", 0) == 0;
}

std::vector<std::string> SystemControl::timezones() {
    static const std::set<std::string> areas{"Africa", "America", "Antarctica", "Arctic", "Asia",
                                             "Atlantic", "Australia", "Europe", "Indian", "Pacific"};
    std::vector<std::string> zones;
    fs::path root = "/usr/share/zoneinfo";
    std::error_code ec;
    for (const auto &area: areas) {
        for (auto it = fs::recursive_directory_iterator(root / area, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string rel = fs::relative(it->path(), root, ec).string();
            if (!ec) zones.push_back(rel);
        }
        ec.clear();
    }
    std::sort(zones.begin(), zones.end());
    return zones;
}

std::string SystemControl::currentTimezone() {
    std::error_code ec;
    fs::path link = fs::read_symlink("/etc/localtime", ec);
    if (ec) return "";
    std::string s = link.string();
    auto pos = s.find("zoneinfo/");
    return pos == std::string::npos ? "" : s.substr(pos + 9);
}

std::string SystemControl::countryForTimezone(const std::string &tz) {
    std::ifstream in("/usr/share/zoneinfo/zone.tab");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream f(line);
        std::string cc, coords, zone;
        f >> cc >> coords >> zone;
        if (zone == tz) return cc;
    }
    return "";
}

std::string SystemControl::hostnameFrom(const std::string &name) {
    std::string h;
    for (unsigned char c: name) {
        if (std::isalnum(c)) h += static_cast<char>(std::tolower(c));
        else if ((c == ' ' || c == '-' || c == '_' || c == '.') && !h.empty() && h.back() != '-') h += '-';
        // apostrophes and other characters are dropped
    }
    while (!h.empty() && h.back() == '-') h.pop_back();
    if (h.size() > 63) h.resize(63);
    while (!h.empty() && h.back() == '-') h.pop_back();
    return h.empty() ? "pipod" : h;
}
