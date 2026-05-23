#pragma once
#include "draw.h"
#include <string>
#include <vector>
#include <unordered_map>

class AppRegistry;
class WindowManager;

// ── File type association ───────────────────────────────────────

struct FileTypeAssoc {
    std::string extension;       // e.g. ".txt"
    std::string mime_type;       // e.g. "text/plain"
    std::string default_app_id;  // e.g. "com.heros.journal"
    Icon icon = Icon::Book;
};

// ── Open With dialog result ─────────────────────────────────────

struct OpenWithChoice {
    std::string app_id;
    std::string app_name;
    Icon icon;
    bool is_default;
};

// ── File Association Manager ────────────────────────────────────

class FileAssocManager {
public:
    void init();

    // Register a default app for a file extension
    void set_default(const std::string& extension, const std::string& app_id);

    // Get the default app for a file extension
    std::string get_default_app(const std::string& extension) const;

    // Get all apps that can open a given extension
    std::vector<OpenWithChoice> get_openers(const std::string& extension,
                                             const AppRegistry& registry) const;

    // Get icon for a file extension
    Icon get_file_icon(const std::string& extension) const;

    // Get MIME type for extension
    std::string get_mime_type(const std::string& extension) const;

    // Open a file with default app (or show picker)
    bool open_file(const std::string& filepath, AppRegistry& registry,
                   WindowManager& wm, int screen_w, int screen_h);

    // "Open With" dialog state
    bool dialog_open() const { return dialog_open_; }
    void close_dialog() { dialog_open_ = false; }

    // Event handling for dialog
    bool handle_click(int mx, int my, AppRegistry& registry,
                      WindowManager& wm, int screen_w, int screen_h);
    void on_mouse_move(int mx, int my, int screen_w, int screen_h);

    // Render the "Open With" dialog
    void render_dialog(SDL_Renderer* r, const Fonts* fonts,
                       int screen_w, int screen_h);

private:
    // Extension -> default app_id
    std::unordered_map<std::string, std::string> defaults_;
    // Extension -> MIME type
    std::unordered_map<std::string, std::string> mime_types_;
    // Extension -> icon
    std::unordered_map<std::string, Icon> icons_;
    // Extension -> list of capable app IDs
    std::unordered_map<std::string, std::vector<std::string>> capable_;

    // Open With dialog
    bool dialog_open_ = false;
    std::string dialog_filepath_;
    std::string dialog_extension_;
    std::vector<OpenWithChoice> dialog_choices_;
    int dialog_hover_ = -1;

    static std::string get_extension(const std::string& filepath);
    void register_defaults();
};
