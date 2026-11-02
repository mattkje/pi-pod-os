#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace httplib { class Server; }

// Small web interface for managing the PiPod's media from another device on the network:
// upload / delete music (~/Music), videos (~/Videos) and photos (~/Pictures), and manage podcast
// subscriptions. Access requires the 4-digit code shown on the PiPod (Settings -> Add Media);
// a successful login sets a session cookie.
//
// Runs on its own thread. The app polls takeChanges() / takePodcastCommands() each frame.
class WebServer {
public:
    WebServer();
    ~WebServer();

    enum Change { MusicChanged = 1, VideosChanged = 2, PhotosChanged = 4 };

    struct PodcastCommand {
        bool add;        // false = remove
        std::string url;
    };

    struct PodcastInfo {
        std::string url, title, error;
        int episodes = 0;
    };

    // webDir: static files (index.html). Media folders are created if missing.
    // Tries `port`, then 8080 and 8081 if that fails. Returns false if no port could be bound.
    bool start(const std::string &webDir, const std::string &deviceName, int port);
    void stop();

    bool running() const { return port_ != 0; }
    int port() const { return port_; }
    const std::string &accessCode() const { return code_; }

    // Bitmask of Change values since the last call.
    int takeChanges() { return changes_.exchange(0); }

    // Subscribe / unsubscribe requests from the web page (applied by the app).
    std::vector<PodcastCommand> takePodcastCommands();
    // What the web page shows as the current subscriptions (set by the app).
    void setPodcasts(const std::vector<PodcastInfo> &podcasts);

    void setDeviceName(const std::string &name);

private:
    void routes();

    std::unique_ptr<httplib::Server> server_;
    std::thread thread_;
    std::string webDir_;
    std::string code_, token_;
    std::atomic<int> port_{0};
    std::atomic<int> changes_{0};
    std::vector<PodcastCommand> podcastCommands_;
    std::vector<PodcastInfo> podcasts_;
    std::mutex podcastMutex_;
    std::atomic<int> failedLogins_{0};
    std::atomic<long long> lockedUntil_{0};
    std::string deviceName_;
    mutable std::mutex nameMutex_;
};

// "http://pipod.local" (or with ":8080"); empty if the server isn't running.
std::string webAddressByName(const WebServer &server);
// "http://192.168.1.20" (or with port); empty if offline.
std::string webAddressByIp(const WebServer &server);
