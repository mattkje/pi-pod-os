#include "net/Net.h"
#include <curl/curl.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    const char *USER_AGENT = "PiPodOS/1.0 (+https://github.com/mattkje/pi-pod-os)";

    size_t toString(void *data, size_t size, size_t n, void *user) {
        static_cast<std::string *>(user)->append(static_cast<char *>(data), size * n);
        return size * n;
    }

    size_t toFile(void *data, size_t size, size_t n, void *user) {
        auto *out = static_cast<std::ofstream *>(user);
        out->write(static_cast<char *>(data), static_cast<std::streamsize>(size * n));
        return out->good() ? size * n : 0;
    }

    CURL *makeHandle(const std::string &url) {
        CURL *c = curl_easy_init();
        if (!c) return nullptr;
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
        // Abort if the transfer stalls below 1 KB/s for 30 s.
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
        return c;
    }

    std::string result(CURL *c, CURLcode rc) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        if (rc != CURLE_OK) return curl_easy_strerror(rc);
        if (status >= 400) return "HTTP " + std::to_string(status);
        return {};
    }
}

std::string Net::get(const std::string &url, std::string &body) {
    CURL *c = makeHandle(url);
    if (!c) return "curl init failed";
    body.clear();
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, toString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    std::string err = result(c, curl_easy_perform(c));
    curl_easy_cleanup(c);
    return err;
}

std::string Net::download(const std::string &url, const std::string &path) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::string part = path + ".part";
    std::ofstream out(part, std::ios::binary);
    if (!out) return "Can't write cache";
    CURL *c = makeHandle(url);
    if (!c) return "curl init failed";
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, toFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    std::string err = result(c, curl_easy_perform(c));
    curl_easy_cleanup(c);
    out.close();
    if (err.empty()) fs::rename(part, path, ec);
    if (!err.empty() || ec) {
        fs::remove(part, ec);
        return err.empty() ? "Can't save download" : err;
    }
    return {};
}

std::string Net::hashName(const std::string &s) {
    // FNV-1a 64-bit: stable across runs (std::hash isn't guaranteed to be).
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char ch: s) {
        h ^= ch;
        h *= 1099511628211ULL;
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", h);
    return buf;
}
