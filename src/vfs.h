#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <cstdint>

// ── File info ───────────────────────────────────────────────────

struct FileInfo {
    std::string path;       // virtual path
    std::string name;       // filename only
    uint64_t size = 0;
    time_t modified = 0;
    bool is_directory = false;
    std::string mime_type;  // inferred from extension
};

// ── Virtual Filesystem ─────────────────────────────────────────

class FileSystem {
public:
    FileSystem();

    // Core operations
    std::string read(const std::string& vpath) const;
    bool write(const std::string& vpath, const std::string& data);
    bool write_atomic(const std::string& vpath, const std::string& data);
    bool exists(const std::string& vpath) const;
    bool mkdir(const std::string& vpath);
    bool remove(const std::string& vpath);
    FileInfo stat(const std::string& vpath) const;
    std::vector<FileInfo> list(const std::string& vpath) const;

    // Scoped access for apps
    std::string app_data_path(const std::string& app_id) const;

    // MIME type inference
    static std::string mime_from_extension(const std::string& filename);

    // Get the real filesystem root
    const std::string& root() const { return root_; }

private:
    std::string root_;  // ~/.heros/
    std::string resolve(const std::string& vpath) const;
    void ensure_root();
};

// ── Settings Store ──────────────────────────────────────────────

class Settings {
public:
    Settings(FileSystem& fs, const std::string& namespace_path);

    // Typed getters with defaults
    std::string get_string(const std::string& key, const std::string& def = "") const;
    int get_int(const std::string& key, int def = 0) const;
    float get_float(const std::string& key, float def = 0.0f) const;
    bool get_bool(const std::string& key, bool def = false) const;

    // Setters
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_float(const std::string& key, float value);
    void set_bool(const std::string& key, bool value);

    bool has(const std::string& key) const;
    void remove(const std::string& key);

    // Persistence
    void load();
    void save();

private:
    FileSystem& fs_;
    std::string path_;  // vfs path to settings file
    // Simple key-value store (flat JSON-like)
    struct Entry {
        std::string key;
        std::string value; // stored as string, parsed on get
    };
    std::vector<Entry> entries_;

    Entry* find(const std::string& key);
    const Entry* find(const std::string& key) const;
};

// ── System-level settings accessor ──────────────────────────────

class SystemSettings {
public:
    SystemSettings(FileSystem& fs);

    Settings& system() { return system_; }
    Settings app(const std::string& app_id);

private:
    FileSystem& fs_;
    Settings system_;
};
