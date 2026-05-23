#include "file_assoc.h"
#include "app_registry.h"
#include <algorithm>
#include <cstdio>

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};

static const int DIALOG_W = 360;
static const int DIALOG_ITEM_H = 44;

// ── Init ────────────────────────────────────────────────────────

void FileAssocManager::init() {
    register_defaults();
}

void FileAssocManager::register_defaults() {
    // Text files → Journal
    for (auto& ext : {".txt", ".md", ".log", ".json", ".yaml", ".yml",
                       ".xml", ".csv", ".ini", ".conf", ".cfg"}) {
        defaults_[ext] = "com.heros.journal";
        mime_types_[ext] = "text/plain";
        icons_[ext] = Icon::Journal;
        capable_[ext] = {"com.heros.journal", "com.heros.terminal"};
    }

    // Code files → Terminal
    for (auto& ext : {".cpp", ".h", ".c", ".py", ".js", ".ts", ".rs",
                       ".go", ".java", ".rb", ".sh", ".bash"}) {
        defaults_[ext] = "com.heros.terminal";
        mime_types_[ext] = "text/x-source";
        icons_[ext] = Icon::Grid;
        capable_[ext] = {"com.heros.terminal", "com.heros.journal"};
    }

    // Data files → Finance
    for (auto& ext : {".xls", ".xlsx", ".ods"}) {
        defaults_[ext] = "com.heros.finance";
        mime_types_[ext] = "application/spreadsheet";
        icons_[ext] = Icon::Briefcase;
        capable_[ext] = {"com.heros.finance"};
    }

    // Image files
    for (auto& ext : {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg"}) {
        defaults_[ext] = "com.heros.files";
        mime_types_[ext] = "image/*";
        icons_[ext] = Icon::Image;
        capable_[ext] = {"com.heros.files"};
    }

    // Archive files
    for (auto& ext : {".zip", ".tar", ".gz", ".bz2", ".xz", ".7z"}) {
        defaults_[ext] = "com.heros.files";
        mime_types_[ext] = "application/archive";
        icons_[ext] = Icon::Box;
        capable_[ext] = {"com.heros.files", "com.heros.terminal"};
    }

    // Directories → Files
    defaults_[""] = "com.heros.files";
    capable_[""] = {"com.heros.files", "com.heros.terminal"};
}

// ── Queries ─────────────────────────────────────────────────────

std::string FileAssocManager::get_extension(const std::string& filepath) {
    auto dot = filepath.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = filepath.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

void FileAssocManager::set_default(const std::string& extension, const std::string& app_id) {
    defaults_[extension] = app_id;
}

std::string FileAssocManager::get_default_app(const std::string& extension) const {
    auto it = defaults_.find(extension);
    if (it != defaults_.end()) return it->second;
    // Fallback to file manager
    return "com.heros.files";
}

std::vector<OpenWithChoice> FileAssocManager::get_openers(const std::string& extension,
                                                           const AppRegistry& registry) const {
    std::vector<OpenWithChoice> result;
    std::string def = get_default_app(extension);

    auto it = capable_.find(extension);
    if (it != capable_.end()) {
        for (auto& app_id : it->second) {
            auto* m = registry.get_manifest(app_id);
            if (m) {
                result.push_back({app_id, m->name, m->icon, app_id == def});
            }
        }
    }

    // Always include file manager and terminal as fallbacks
    auto ensure_app = [&](const std::string& id) {
        for (auto& c : result) {
            if (c.app_id == id) return;
        }
        auto* m = registry.get_manifest(id);
        if (m) result.push_back({id, m->name, m->icon, id == def});
    };
    ensure_app("com.heros.files");
    ensure_app("com.heros.terminal");

    // Sort: default first
    std::sort(result.begin(), result.end(), [](const OpenWithChoice& a, const OpenWithChoice& b) {
        if (a.is_default != b.is_default) return a.is_default;
        return a.app_name < b.app_name;
    });

    return result;
}

Icon FileAssocManager::get_file_icon(const std::string& extension) const {
    auto it = icons_.find(extension);
    if (it != icons_.end()) return it->second;
    return Icon::Book;
}

std::string FileAssocManager::get_mime_type(const std::string& extension) const {
    auto it = mime_types_.find(extension);
    if (it != mime_types_.end()) return it->second;
    return "application/octet-stream";
}

// ── Open file ───────────────────────────────────────────────────

bool FileAssocManager::open_file(const std::string& filepath, AppRegistry& registry,
                                  WindowManager& wm, int screen_w, int screen_h) {
    std::string ext = get_extension(filepath);
    std::string app_id = get_default_app(ext);

    if (registry.has_app(app_id)) {
        registry.launch(app_id, wm, screen_w, screen_h);
        return true;
    }

    // No default — show Open With dialog
    dialog_open_ = true;
    dialog_filepath_ = filepath;
    dialog_extension_ = ext;
    dialog_choices_ = get_openers(ext, registry);
    dialog_hover_ = -1;
    return false;
}

// ── Dialog interaction ──────────────────────────────────────────

bool FileAssocManager::handle_click(int mx, int my, AppRegistry& registry,
                                     WindowManager& wm, int screen_w, int screen_h) {
    if (!dialog_open_) return false;

    int dh = 60 + (int)dialog_choices_.size() * DIALOG_ITEM_H + 40;
    int dx = (screen_w - DIALOG_W) / 2;
    int dy = (screen_h - dh) / 2;

    // Outside dialog
    if (mx < dx || mx >= dx + DIALOG_W || my < dy || my >= dy + dh) {
        dialog_open_ = false;
        return true;
    }

    // Check items
    int item_y = dy + 58;
    for (int i = 0; i < (int)dialog_choices_.size(); i++) {
        if (my >= item_y && my < item_y + DIALOG_ITEM_H) {
            // Launch selected app
            registry.launch(dialog_choices_[i].app_id, wm, screen_w, screen_h);
            dialog_open_ = false;
            return true;
        }
        item_y += DIALOG_ITEM_H;
    }

    // "Set as default" button area
    if (dialog_hover_ >= 0) {
        set_default(dialog_extension_, dialog_choices_[dialog_hover_].app_id);
    }

    return true;
}

void FileAssocManager::on_mouse_move(int mx, int my, int screen_w, int screen_h) {
    if (!dialog_open_) { dialog_hover_ = -1; return; }

    int dh = 60 + (int)dialog_choices_.size() * DIALOG_ITEM_H + 40;
    int dx = (screen_w - DIALOG_W) / 2;
    int dy = (screen_h - dh) / 2;

    dialog_hover_ = -1;
    int item_y = dy + 58;
    for (int i = 0; i < (int)dialog_choices_.size(); i++) {
        if (mx >= dx && mx < dx + DIALOG_W && my >= item_y && my < item_y + DIALOG_ITEM_H) {
            dialog_hover_ = i;
            break;
        }
        item_y += DIALOG_ITEM_H;
    }
}

// ── Render dialog ───────────────────────────────────────────────

void FileAssocManager::render_dialog(SDL_Renderer* r, const Fonts* fonts,
                                      int screen_w, int screen_h) {
    if (!dialog_open_) return;

    // Dark overlay
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 15, 160);
    SDL_Rect full = {0, 0, screen_w, screen_h};
    SDL_RenderFillRect(r, &full);

    int dh = 60 + (int)dialog_choices_.size() * DIALOG_ITEM_H + 40;
    int dx = (screen_w - DIALOG_W) / 2;
    int dy = (screen_h - dh) / 2;

    // Panel
    SDL_Rect panel = {dx, dy, DIALOG_W, dh};
    draw::filled_rounded_rect(r, panel, 12, {15, 18, 30, 230});
    draw::rounded_rect(r, panel, 12, {180, 195, 220, 40});

    // Title
    draw::text(r, fonts->title, "Open With", dx + 16, dy + 12, WHITE);

    // Filename
    std::string filename = dialog_filepath_;
    auto slash = filename.rfind('/');
    if (slash != std::string::npos) filename = filename.substr(slash + 1);
    if (filename.size() > 35) filename = filename.substr(0, 32) + "...";
    draw::text(r, fonts->small, filename.c_str(), dx + 16, dy + 34, DIM);

    // Separator
    draw::line(r, dx + 12, dy + 54, dx + DIALOG_W - 12, dy + 54, {180, 195, 220, 30});

    // App choices
    int item_y = dy + 58;
    for (int i = 0; i < (int)dialog_choices_.size(); i++) {
        auto& choice = dialog_choices_[i];
        SDL_Rect item = {dx + 8, item_y, DIALOG_W - 16, DIALOG_ITEM_H};

        if (i == dialog_hover_) {
            draw::filled_rounded_rect(r, item, 6, {100, 150, 255, 25});
        }

        // App icon
        draw::icon(r, choice.icon, dx + 30, item_y + DIALOG_ITEM_H / 2, 18,
                   choice.is_default ? ACCENT : DIM);

        // App name
        draw::text(r, fonts->body, choice.app_name.c_str(), dx + 50, item_y + 8,
                   choice.is_default ? WHITE : DIM);

        // Default badge
        if (choice.is_default) {
            draw::text_right(r, fonts->small, "Default", dx + DIALOG_W - 16, item_y + 14,
                             ACCENT);
        }

        // App ID subtitle
        draw::text(r, fonts->small, choice.app_id.c_str(), dx + 50, item_y + 26,
                   {120, 130, 150, 150});

        item_y += DIALOG_ITEM_H;
    }

    // Footer hint
    draw::text_centered(r, fonts->small, "Click to open  |  Right-click to set as default",
                        screen_w / 2, item_y + 12, {120, 130, 150, 120});
}
