#include "process.h"
#include "app_registry.h"
#include <cstdio>
#include <algorithm>

// ── State string ────────────────────────────────────────────────

const char* process_state_str(ProcessState s) {
    switch (s) {
    case ProcessState::Starting:    return "Starting";
    case ProcessState::Running:     return "Running";
    case ProcessState::Suspended:   return "Suspended";
    case ProcessState::Terminating: return "Terminating";
    case ProcessState::Dead:        return "Dead";
    }
    return "Unknown";
}

// ── Internal helpers ────────────────────────────────────────────

ProcessManager::ProcessEntry* ProcessManager::find_entry(uint32_t pid) {
    for (auto& e : processes_)
        if (e.info.pid == pid) return &e;
    return nullptr;
}

const ProcessManager::ProcessEntry* ProcessManager::find_entry(uint32_t pid) const {
    for (auto& e : processes_)
        if (e.info.pid == pid) return &e;
    return nullptr;
}

void ProcessManager::remove_process(uint32_t pid) {
    processes_.erase(
        std::remove_if(processes_.begin(), processes_.end(),
                       [pid](const ProcessEntry& e) { return e.info.pid == pid; }),
        processes_.end());
}

// ── Spawn app ───────────────────────────────────────────────────

uint32_t ProcessManager::spawn(const std::string& app_id, AppRegistry& registry,
                                WindowManager& wm, int screen_w, int screen_h) {
    if (boot_time_ == 0) boot_time_ = time(nullptr);

    // Launch through registry (handles singleton, factory, window creation)
    int win_id = registry.launch(app_id, wm, screen_w, screen_h);
    if (win_id < 0) return 0;

    // Check if registry already had this running (singleton focus case)
    // If we just focused an existing window, find the existing process
    for (auto& e : processes_) {
        if (e.info.window_id == win_id && e.info.state == ProcessState::Running) {
            return e.info.pid; // Already tracked
        }
    }

    ProcessEntry entry;
    entry.info.pid = next_pid_++;
    entry.info.app_id = app_id;
    entry.info.state = ProcessState::Running;
    entry.info.window_id = win_id;
    entry.info.start_time = time(nullptr);
    entry.info.is_service = false;

    uint32_t pid = entry.info.pid;
    processes_.push_back(std::move(entry));

    fprintf(stderr, "ProcessManager: spawned '%s' pid=%u win=%d\n",
            app_id.c_str(), pid, win_id);
    return pid;
}

// ── Spawn service ───────────────────────────────────────────────

uint32_t ProcessManager::spawn_service(const std::string& service_id,
                                        std::unique_ptr<Service> service) {
    if (boot_time_ == 0) boot_time_ = time(nullptr);

    ProcessEntry entry;
    entry.info.pid = next_pid_++;
    entry.info.app_id = service_id;
    entry.info.state = ProcessState::Running;
    entry.info.window_id = -1;
    entry.info.start_time = time(nullptr);
    entry.info.is_service = true;
    entry.service = std::move(service);

    uint32_t pid = entry.info.pid;
    entry.service->set_pid(pid);
    entry.service->on_start();

    processes_.push_back(std::move(entry));

    fprintf(stderr, "ProcessManager: started service '%s' pid=%u\n",
            service_id.c_str(), pid);
    return pid;
}

// ── Terminate (graceful) ────────────────────────────────────────

void ProcessManager::terminate(uint32_t pid, WindowManager& wm) {
    auto* entry = find_entry(pid);
    if (!entry || entry->info.state == ProcessState::Dead) return;

    entry->info.state = ProcessState::Terminating;

    if (entry->info.is_service) {
        if (entry->service) entry->service->on_stop();
        entry->info.state = ProcessState::Dead;
        fprintf(stderr, "ProcessManager: terminated service pid=%u\n", pid);
    } else {
        // Close the window (which calls on_close on the app)
        if (entry->info.window_id >= 0) {
            wm.close_window(entry->info.window_id);
            // If window still exists, app vetoed the close
            if (wm.find_window(entry->info.window_id)) {
                entry->info.state = ProcessState::Running;
                return;
            }
        }
        entry->info.state = ProcessState::Dead;
        fprintf(stderr, "ProcessManager: terminated pid=%u\n", pid);
    }

    remove_process(pid);
}

// ── Kill (forced) ───────────────────────────────────────────────

void ProcessManager::kill(uint32_t pid, WindowManager& wm) {
    auto* entry = find_entry(pid);
    if (!entry) return;

    if (entry->info.is_service) {
        // Don't call on_stop for force kill
    } else if (entry->info.window_id >= 0) {
        // Force-remove window without calling on_close
        // We need to remove it directly from WM
        // For now, close_window is the only way; the app already lost its chance
        wm.close_window(entry->info.window_id);
    }

    fprintf(stderr, "ProcessManager: killed pid=%u\n", pid);
    remove_process(pid);
}

// ── Queries ─────────────────────────────────────────────────────

const ProcessInfo* ProcessManager::get_process(uint32_t pid) const {
    auto* e = find_entry(pid);
    return e ? &e->info : nullptr;
}

std::vector<const ProcessInfo*> ProcessManager::list_processes() const {
    std::vector<const ProcessInfo*> result;
    result.reserve(processes_.size());
    for (auto& e : processes_)
        result.push_back(&e.info);
    return result;
}

std::vector<const ProcessInfo*> ProcessManager::list_by_app(const std::string& app_id) const {
    std::vector<const ProcessInfo*> result;
    for (auto& e : processes_)
        if (e.info.app_id == app_id) result.push_back(&e.info);
    return result;
}

uint32_t ProcessManager::find_pid_by_window(int window_id) const {
    for (auto& e : processes_)
        if (e.info.window_id == window_id) return e.info.pid;
    return 0;
}

// ── Per-frame tick ──────────────────────────────────────────────

void ProcessManager::tick(WindowManager& /*wm*/) {
    for (auto& e : processes_) {
        if (e.info.is_service && e.service && e.info.state == ProcessState::Running) {
            e.service->on_tick();
        }
    }
}

// ── Sync with WM ───────────────────────────────────────────────

void ProcessManager::sync(const WindowManager& wm, AppRegistry& registry) {
    std::vector<uint32_t> dead;
    for (auto& e : processes_) {
        if (!e.info.is_service && e.info.window_id >= 0) {
            if (!wm.find_window(e.info.window_id)) {
                dead.push_back(e.info.pid);
                registry.on_window_closed(e.info.window_id);
            }
        }
    }
    for (uint32_t pid : dead) {
        remove_process(pid);
    }
}

// ── Session management ─────────────────────────────────────────

std::vector<ProcessManager::SessionEntry> ProcessManager::save_session(
        const WindowManager& wm) const {
    std::vector<SessionEntry> session;
    for (auto& e : processes_) {
        if (e.info.is_service || e.info.window_id < 0) continue;
        auto* win = wm.find_window(e.info.window_id);
        if (!win) continue;

        SessionEntry se;
        se.app_id = e.info.app_id;
        se.x = win->rect.x;
        se.y = win->rect.y;
        se.w = win->rect.w;
        se.h = win->rect.h;
        se.maximized = win->maximized;
        session.push_back(se);
    }
    return session;
}

void ProcessManager::restore_session(const std::vector<SessionEntry>& session,
                                      AppRegistry& registry, WindowManager& wm,
                                      int screen_w, int screen_h) {
    for (auto& se : session) {
        int win_id = registry.launch(se.app_id, wm, screen_w, screen_h);
        if (win_id < 0) continue;

        auto* win = wm.find_window(win_id);
        if (!win) continue;

        if (se.maximized) {
            wm.maximize(win_id, screen_w, screen_h);
        } else {
            win->rect = {se.x, se.y, se.w, se.h};
        }

        // Track in process table
        ProcessEntry entry;
        entry.info.pid = next_pid_++;
        entry.info.app_id = se.app_id;
        entry.info.state = ProcessState::Running;
        entry.info.window_id = win_id;
        entry.info.start_time = time(nullptr);
        processes_.push_back(std::move(entry));
    }
}
