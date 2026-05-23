#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <ctime>
#include <functional>

// ── Process states ──────────────────────────────────────────────

enum class ProcessState {
    Starting,
    Running,
    Suspended,
    Terminating,
    Dead
};

const char* process_state_str(ProcessState s);

// ── Process info ────────────────────────────────────────────────

struct ProcessInfo {
    uint32_t pid = 0;
    std::string app_id;
    ProcessState state = ProcessState::Dead;
    int window_id = -1;     // -1 for services (no window)
    uint32_t parent_pid = 0;
    time_t start_time = 0;
    double cpu_time_ms = 0;  // approximate render time accumulated
    bool is_service = false;
};

// ── Service base class ──────────────────────────────────────────

class Service {
public:
    virtual ~Service() = default;
    virtual void on_start() {}
    virtual void on_stop() {}
    virtual void on_tick() {}  // called each frame

    void set_pid(uint32_t pid) { pid_ = pid; }
    uint32_t pid() const { return pid_; }

protected:
    uint32_t pid_ = 0;
};

// ── Process Manager ─────────────────────────────────────────────

class WindowManager;
class AppRegistry;

class ProcessManager {
public:
    // Spawn an app process (creates window via registry)
    uint32_t spawn(const std::string& app_id, AppRegistry& registry,
                   WindowManager& wm, int screen_w, int screen_h);

    // Spawn a background service
    uint32_t spawn_service(const std::string& service_id,
                           std::unique_ptr<Service> service);

    // Terminate gracefully (calls on_close, waits)
    void terminate(uint32_t pid, WindowManager& wm);

    // Force kill (no callbacks)
    void kill(uint32_t pid, WindowManager& wm);

    // Queries
    const ProcessInfo* get_process(uint32_t pid) const;
    std::vector<const ProcessInfo*> list_processes() const;
    std::vector<const ProcessInfo*> list_by_app(const std::string& app_id) const;
    uint32_t find_pid_by_window(int window_id) const;
    int process_count() const { return (int)processes_.size(); }

    // Per-frame update: tick services, track timing
    void tick(WindowManager& wm);

    // Sync with window manager (detect closed windows)
    void sync(const WindowManager& wm, AppRegistry& registry);

    // Session save/restore
    struct SessionEntry {
        std::string app_id;
        int x, y, w, h;
        bool maximized;
    };
    std::vector<SessionEntry> save_session(const WindowManager& wm) const;
    void restore_session(const std::vector<SessionEntry>& session,
                         AppRegistry& registry, WindowManager& wm,
                         int screen_w, int screen_h);

    // Uptime
    time_t start_time() const { return boot_time_; }

private:
    uint32_t next_pid_ = 1;
    time_t boot_time_ = 0;

    struct ProcessEntry {
        ProcessInfo info;
        std::unique_ptr<Service> service; // null for windowed apps
    };

    std::vector<ProcessEntry> processes_;

    ProcessEntry* find_entry(uint32_t pid);
    const ProcessEntry* find_entry(uint32_t pid) const;
    void remove_process(uint32_t pid);
};
