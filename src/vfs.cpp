#include "vfs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>

// ── FileSystem ──────────────────────────────────────────────────

FileSystem::FileSystem() {
    const char* home = getenv("HOME");
    if (home) {
        root_ = std::string(home) + "/.heros/";
    } else {
        root_ = "/tmp/.heros/";
    }
    ensure_root();
}

void FileSystem::ensure_root() {
    ::mkdir(root_.c_str(), 0755);
    ::mkdir((root_ + "system").c_str(), 0755);
    ::mkdir((root_ + "apps").c_str(), 0755);
    ::mkdir((root_ + "user").c_str(), 0755);
    ::mkdir((root_ + "tmp").c_str(), 0755);
}

std::string FileSystem::resolve(const std::string& vpath) const {
    // Strip leading /
    std::string rel = vpath;
    while (!rel.empty() && rel[0] == '/') rel = rel.substr(1);

    // Security: prevent path traversal
    if (rel.find("..") != std::string::npos) {
        fprintf(stderr, "VFS: path traversal rejected: %s\n", vpath.c_str());
        return "";
    }

    return root_ + rel;
}

std::string FileSystem::read(const std::string& vpath) const {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return "";

    std::ifstream f(rpath);
    if (!f.is_open()) return "";

    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool FileSystem::write(const std::string& vpath, const std::string& data) {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return false;

    // Ensure parent directory exists
    size_t last_slash = rpath.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir = rpath.substr(0, last_slash);
        ::mkdir(dir.c_str(), 0755);
    }

    std::ofstream f(rpath);
    if (!f.is_open()) return false;
    f << data;
    return f.good();
}

bool FileSystem::write_atomic(const std::string& vpath, const std::string& data) {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return false;

    // Ensure parent directory exists
    size_t last_slash = rpath.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir = rpath.substr(0, last_slash);
        ::mkdir(dir.c_str(), 0755);
    }

    std::string tmp_path = rpath + ".tmp";
    std::ofstream f(tmp_path);
    if (!f.is_open()) return false;
    f << data;
    f.close();
    if (!f.good()) {
        ::unlink(tmp_path.c_str());
        return false;
    }
    return ::rename(tmp_path.c_str(), rpath.c_str()) == 0;
}

bool FileSystem::exists(const std::string& vpath) const {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return false;
    struct stat st;
    return ::stat(rpath.c_str(), &st) == 0;
}

bool FileSystem::mkdir(const std::string& vpath) {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return false;

    // Create intermediate dirs
    std::string accum;
    for (size_t i = 0; i < rpath.size(); i++) {
        accum += rpath[i];
        if (rpath[i] == '/' || i == rpath.size() - 1) {
            ::mkdir(accum.c_str(), 0755);
        }
    }
    return true;
}

bool FileSystem::remove(const std::string& vpath) {
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return false;
    return ::unlink(rpath.c_str()) == 0 || ::rmdir(rpath.c_str()) == 0;
}

FileInfo FileSystem::stat(const std::string& vpath) const {
    FileInfo info;
    info.path = vpath;

    // Extract filename
    size_t last_slash = vpath.rfind('/');
    info.name = (last_slash != std::string::npos) ? vpath.substr(last_slash + 1) : vpath;

    std::string rpath = resolve(vpath);
    if (rpath.empty()) return info;

    struct stat st;
    if (::stat(rpath.c_str(), &st) != 0) return info;

    info.size = st.st_size;
    info.modified = st.st_mtime;
    info.is_directory = S_ISDIR(st.st_mode);
    info.mime_type = mime_from_extension(info.name);
    return info;
}

std::vector<FileInfo> FileSystem::list(const std::string& vpath) const {
    std::vector<FileInfo> result;
    std::string rpath = resolve(vpath);
    if (rpath.empty()) return result;

    DIR* dir = opendir(rpath.c_str());
    if (!dir) return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string child_vpath = vpath;
        if (!child_vpath.empty() && child_vpath.back() != '/') child_vpath += '/';
        child_vpath += entry->d_name;

        result.push_back(stat(child_vpath));
    }
    closedir(dir);

    // Sort: directories first, then by name
    std::sort(result.begin(), result.end(), [](const FileInfo& a, const FileInfo& b) {
        if (a.is_directory != b.is_directory) return a.is_directory > b.is_directory;
        return a.name < b.name;
    });

    return result;
}

std::string FileSystem::app_data_path(const std::string& app_id) const {
    return "/apps/" + app_id + "/data";
}

std::string FileSystem::mime_from_extension(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = filename.substr(dot);
    // Lowercase
    for (auto& c : ext) c = (char)tolower(c);

    if (ext == ".txt")  return "text/plain";
    if (ext == ".json") return "text/json";
    if (ext == ".csv")  return "text/csv";
    if (ext == ".html") return "text/html";
    if (ext == ".md")   return "text/markdown";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".pdf")  return "application/pdf";
    return "application/octet-stream";
}

// ── Settings ────────────────────────────────────────────────────

Settings::Settings(FileSystem& fs, const std::string& namespace_path)
    : fs_(fs), path_(namespace_path) {
    load();
}

Settings::Entry* Settings::find(const std::string& key) {
    for (auto& e : entries_)
        if (e.key == key) return &e;
    return nullptr;
}

const Settings::Entry* Settings::find(const std::string& key) const {
    for (auto& e : entries_)
        if (e.key == key) return &e;
    return nullptr;
}

std::string Settings::get_string(const std::string& key, const std::string& def) const {
    auto* e = find(key);
    return e ? e->value : def;
}

int Settings::get_int(const std::string& key, int def) const {
    auto* e = find(key);
    if (!e) return def;
    try { return std::stoi(e->value); } catch (...) { return def; }
}

float Settings::get_float(const std::string& key, float def) const {
    auto* e = find(key);
    if (!e) return def;
    try { return std::stof(e->value); } catch (...) { return def; }
}

bool Settings::get_bool(const std::string& key, bool def) const {
    auto* e = find(key);
    if (!e) return def;
    return e->value == "true" || e->value == "1";
}

void Settings::set_string(const std::string& key, const std::string& value) {
    auto* e = find(key);
    if (e) { e->value = value; }
    else entries_.push_back({key, value});
    save();
}

void Settings::set_int(const std::string& key, int value) {
    set_string(key, std::to_string(value));
}

void Settings::set_float(const std::string& key, float value) {
    set_string(key, std::to_string(value));
}

void Settings::set_bool(const std::string& key, bool value) {
    set_string(key, value ? "true" : "false");
}

bool Settings::has(const std::string& key) const {
    return find(key) != nullptr;
}

void Settings::remove(const std::string& key) {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&key](const Entry& e) { return e.key == key; }),
        entries_.end());
    save();
}

// Simple key=value format for persistence
void Settings::load() {
    entries_.clear();
    std::string data = fs_.read(path_);
    if (data.empty()) return;

    std::istringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        Entry e;
        e.key = line.substr(0, eq);
        e.value = line.substr(eq + 1);
        entries_.push_back(e);
    }
}

void Settings::save() {
    std::ostringstream ss;
    for (auto& e : entries_) {
        ss << e.key << "=" << e.value << "\n";
    }
    fs_.write_atomic(path_, ss.str());
}

// ── SystemSettings ──────────────────────────────────────────────

SystemSettings::SystemSettings(FileSystem& fs)
    : fs_(fs), system_(fs, "/system/settings.conf") {}

Settings SystemSettings::app(const std::string& app_id) {
    fs_.mkdir("/apps/" + app_id);
    return Settings(fs_, "/apps/" + app_id + "/settings.conf");
}
