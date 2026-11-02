#include "system/DeviceInfo.h"
#include "app/Config.h"
#include "media/Library.h"
#include "ui/Utils.h"
#include <filesystem>
#include <fstream>
#include <sys/statvfs.h>

namespace fs = std::filesystem;

namespace {
    unsigned long long folderSize(const std::string &dir) {
        unsigned long long bytes = 0;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it->is_regular_file(ec)) bytes += it->file_size(ec);
        }
        return bytes;
    }

    std::string readFirstLine(const char *path, char delim = '\n') {
        std::ifstream in(path);
        std::string s;
        if (in) std::getline(in, s, delim);
        return s;
    }

    std::string modelName() {
        std::string m = readFirstLine("/proc/device-tree/model", '\0');
        if (!m.empty()) return m;
#if defined(__APPLE__)
        return "Mac (development)";
#else
        return "Linux computer";
#endif
    }

    std::string serialNumber() {
        std::string s = readFirstLine("/proc/device-tree/serial-number", '\0');
        if (!s.empty()) {
            // Strip leading zeros the firmware pads with
            size_t nz = s.find_first_not_of('0');
            return nz == std::string::npos ? s : s.substr(nz);
        }
        return "-";
    }

    std::string systemName() {
        std::ifstream in("/etc/os-release");
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                std::string v = line.substr(12);
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
                return v;
            }
        }
#if defined(__APPLE__)
        return "macOS";
#else
        return "Linux";
#endif
    }
}

void refreshDeviceInfo(DeviceInfo &info, int localSongs, int jellyfinSongs) {
    info.localSongs = localSongs;
    info.jellyfinSongs = jellyfinSongs;
    info.ipAddress = localIpAddress();

    info.cacheBytes = folderSize(Config::cacheDir());
    info.cacheSize = formatBytes(info.cacheBytes);
    info.musicBytes = 0;
    for (const auto &dir: Library::defaultMusicDirs()) info.musicBytes += folderSize(dir);

    struct statvfs vfs{};
    if (statvfs("/", &vfs) == 0) {
        info.diskTotal = static_cast<unsigned long long>(vfs.f_blocks) * vfs.f_frsize;
        info.diskFree = static_cast<unsigned long long>(vfs.f_bavail) * vfs.f_frsize;
    }

    if (info.model.empty()) {
        info.model = modelName();
        info.serial = serialNumber();
        info.system = systemName();
    }
}
