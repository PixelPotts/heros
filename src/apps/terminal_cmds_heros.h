#pragma once
#include "terminal_shell.h"
#include "../app_registry.h"
#include "../process.h"
#include "../event_bus.h"
#include <ctime>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

// ── Helper: locate and parse commands.yaml ──────────────────────

static inline std::string find_commands_yaml() {
    // 1. Env override (for tests)
    const char* env = std::getenv("HEROS_COMMANDS_YAML");
    if (env && std::ifstream(env).good()) return env;
    // 2. App bundle
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/home";
    std::string bundle = home + "/.heros/apps/com.heros.terminal/commands.yaml";
    if (std::ifstream(bundle).good()) return bundle;
    // 3. Source tree (dev fallback)
    return "";
}

// ── 9 HerOS-Specific Commands (incl. manall) ────────────────────

inline void register_heros_commands(ShellEngine& shell) {

    // 1. launch — launch an app
    shell.register_cmd("launch", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("launch: usage: launch APP_ID"); return 1; }
        if (!ctx.registry || !ctx.wm) { ctx.outln("launch: no app registry"); return 1; }

        std::string app_id = args[1];
        // Try with com.heros. prefix if not fully qualified
        if (app_id.find('.') == std::string::npos) {
            app_id = "com.heros." + app_id;
        }

        if (!ctx.registry->has_app(app_id)) {
            ctx.outln("launch: unknown app: " + app_id);
            ctx.outln("Use 'apps' to list available apps.");
            return 1;
        }

        int wid = ctx.registry->launch(app_id, *ctx.wm, ctx.screen_w, ctx.screen_h);
        if (wid >= 0) {
            ctx.outln("Launched " + app_id + " (window " + std::to_string(wid) + ")");
        } else {
            ctx.outln("launch: failed to launch " + app_id);
            return 1;
        }
        return 0;
    });

    // 2. apps — list installed apps
    shell.register_cmd("apps", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        if (!ctx.registry) { ctx.outln("apps: no app registry"); return 1; }

        auto manifests = ctx.registry->list_apps();
        std::sort(manifests.begin(), manifests.end(),
            [](const AppManifest* a, const AppManifest* b) { return a->dock_order < b->dock_order; });

        ctx.outln("Installed Apps:");
        ctx.outln("  APP ID                       NAME              STATUS");
        ctx.outln("  ---------------------------  ----------------  -------");

        for (auto* m : manifests) {
            bool running = ctx.registry->is_running(m->app_id);
            char line[128];
            snprintf(line, sizeof(line), "  %-27s  %-16s  %s",
                     m->app_id.c_str(), m->name.c_str(),
                     running ? "\033[32mrunning\033[0m" : "stopped");
            ctx.outln(line);
        }
        return 0;
    });

    // 3. notify — send a toast notification
    shell.register_cmd("notify", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("notify: usage: notify TITLE [BODY]"); return 1; }
        if (!ctx.notifications) { ctx.outln("notify: notifications not available"); return 1; }

        std::string title = args[1];
        std::string body;
        for (size_t i = 2; i < args.size(); i++) {
            if (!body.empty()) body += " ";
            body += args[i];
        }

        ctx.notifications->notify(title, body, "com.heros.terminal");
        ctx.outln("Notification sent: " + title);
        return 0;
    });

    // 4. theme — list/set themes
    shell.register_cmd("theme", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)ctx;
        if (args.size() < 2) {
            ctx.outln("Available themes:");
            ctx.outln("  default    — Dark blue (current)");
            ctx.outln("  midnight   — Deep black");
            ctx.outln("  forest     — Dark green");
            ctx.outln("  ocean      — Teal blue");
            ctx.outln("");
            ctx.outln("Usage: theme set THEME_NAME");
            return 0;
        }

        if (args[1] == "list") {
            ctx.outln("default midnight forest ocean");
            return 0;
        }

        if (args[1] == "set" && args.size() >= 3) {
            ctx.outln("Theme set to: " + args[2]);
            ctx.outln("(Visual change requires restart)");
            return 0;
        }

        ctx.outln("theme: unknown subcommand: " + args[1]);
        return 1;
    });

    // 5. wallpaper — info about wallpaper
    shell.register_cmd("wallpaper", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("Current wallpaper: assets/wallpaper.jpg");
        ctx.outln("Source: Unsplash");
        ctx.outln("Resolution: 1920x1080");
        return 0;
    });

    // 6. help — categorized command listing
    shell.register_cmd("help", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;

        ctx.outln("\033[1;36mHerOS Terminal — 101 Built-in Commands\033[0m");
        ctx.outln("");

        ctx.outln("\033[1;33mShell & Environment (20):\033[0m");
        ctx.outln("  echo printf env export unset alias unalias history");
        ctx.outln("  which type source read test true false exit clear set yes seq");
        ctx.outln("");

        ctx.outln("\033[1;33mFile Operations (20):\033[0m");
        ctx.outln("  ls cd pwd cat cp mv rm mkdir rmdir touch find");
        ctx.outln("  ln chmod chown stat file tree du df realpath");
        ctx.outln("");

        ctx.outln("\033[1;33mText Processing (20):\033[0m");
        ctx.outln("  grep sed awk sort uniq wc head tail cut tr");
        ctx.outln("  diff tee paste rev fold column nl strings base64 xargs");
        ctx.outln("");

        ctx.outln("\033[1;33mSystem (15):\033[0m");
        ctx.outln("  ps kill top uptime whoami hostname uname date");
        ctx.outln("  cal time sleep id w free lsof");
        ctx.outln("");

        ctx.outln("\033[1;33mNetworking (10):\033[0m");
        ctx.outln("  curl wget ping dig ifconfig netstat nc host traceroute nslookup");
        ctx.outln("");

        ctx.outln("\033[1;33mArchive & Checksum (7):\033[0m");
        ctx.outln("  tar gzip gunzip zip unzip md5sum sha256sum");
        ctx.outln("");

        ctx.outln("\033[1;33mHerOS (9):\033[0m");
        ctx.outln("  launch apps notify theme wallpaper help man manall neofetch");
        ctx.outln("");

        ctx.outln("Type '\033[1mman COMMAND\033[0m' for details on any command.");
        ctx.outln("Supports pipes (|), redirects (> >>), semicolons (;), and env vars ($VAR).");

        (void)shell;
        return 0;
    });

    // 7. man — manual page for a command (reads from commands.yaml)
    shell.register_cmd("man", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("man: what manual page do you want?"); return 1; }

        std::string yaml_path = find_commands_yaml();
        if (yaml_path.empty()) {
            ctx.outln("man: commands.yaml not found");
            return 1;
        }

        YAML::Node root;
        try { root = YAML::LoadFile(yaml_path); }
        catch (const std::exception& e) {
            ctx.outln(std::string("man: failed to parse commands.yaml: ") + e.what());
            return 1;
        }

        std::string target = args[1];
        for (const auto& cat : root["categories"]) {
            for (const auto& cmd : cat["commands"]) {
                if (cmd["name"].as<std::string>() == target) {
                    std::string name = cmd["name"].as<std::string>();
                    std::string synopsis = cmd["synopsis"].as<std::string>();
                    std::string desc = cmd["description"].as<std::string>();

                    ctx.outln("\033[1m" + name + "\033[0m(1) \u2014 HerOS Manual");
                    ctx.outln("");
                    ctx.outln("\033[1mSYNOPSIS\033[0m");
                    ctx.outln("  " + synopsis);
                    ctx.outln("");
                    ctx.outln("\033[1mDESCRIPTION\033[0m");
                    ctx.outln("  " + desc);

                    if (cmd["flags"] && cmd["flags"].size() > 0) {
                        ctx.outln("");
                        ctx.outln("\033[1mFLAGS\033[0m");
                        for (const auto& f : cmd["flags"]) {
                            ctx.outln("  \033[1;36m" + f["flag"].as<std::string>() +
                                      "\033[0m  " + f["desc"].as<std::string>());
                        }
                    }
                    return 0;
                }
            }
        }

        ctx.outln("No manual entry for " + target);
        return 1;
    });

    // 8. manall — full command reference from commands.yaml
    shell.register_cmd("manall", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string yaml_path = find_commands_yaml();
        if (yaml_path.empty()) {
            ctx.outln("manall: commands.yaml not found");
            return 1;
        }

        YAML::Node root;
        try { root = YAML::LoadFile(yaml_path); }
        catch (const std::exception& e) {
            ctx.outln(std::string("manall: failed to parse commands.yaml: ") + e.what());
            return 1;
        }

        // Optional filter: manall grep
        std::string filter;
        if (args.size() >= 2) filter = args[1];

        bool found_any = false;
        for (const auto& cat : root["categories"]) {
            std::string cat_name = cat["name"].as<std::string>();
            bool cat_header_printed = false;

            for (const auto& cmd : cat["commands"]) {
                std::string name = cmd["name"].as<std::string>();
                if (!filter.empty() && name != filter) continue;

                // Print category header on first match
                if (!cat_header_printed) {
                    if (found_any) ctx.outln("");
                    ctx.outln("\033[1;33m\u2550\u2550 " + cat_name + " \u2550\u2550\033[0m");
                    cat_header_printed = true;
                }
                found_any = true;

                std::string synopsis = cmd["synopsis"].as<std::string>();
                std::string desc = cmd["description"].as<std::string>();

                ctx.outln("");
                ctx.outln("  \033[1;36m" + name + "\033[0m  " + synopsis);
                ctx.outln("    " + desc);

                if (cmd["flags"] && cmd["flags"].size() > 0) {
                    for (const auto& f : cmd["flags"]) {
                        ctx.outln("      \033[1m" + f["flag"].as<std::string>() +
                                  "\033[0m  " + f["desc"].as<std::string>());
                    }
                }
            }
        }

        if (!found_any) {
            if (!filter.empty()) {
                ctx.outln("manall: no entry for '" + filter + "'");
                return 1;
            }
            ctx.outln("manall: no commands found in YAML");
            return 1;
        }

        ctx.outln("");
        return 0;
    });

    // 8. neofetch — system info with ASCII art
    shell.register_cmd("neofetch", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;

        // Get info
        auto it_user = ctx.env.find("USER");
        std::string user = (it_user != ctx.env.end()) ? it_user->second : "hero";
        auto it_host = ctx.env.find("HOSTNAME");
        std::string hostname = (it_host != ctx.env.end()) ? it_host->second : "heros";

        int procs = ctx.pm ? ctx.pm->process_count() : 0;
        std::string uptime_str = "0m";
        if (ctx.pm) {
            time_t up = time(nullptr) - ctx.pm->start_time();
            int h = (int)(up / 3600), m = (int)((up % 3600) / 60);
            if (h > 0) uptime_str = std::to_string(h) + "h " + std::to_string(m) + "m";
            else uptime_str = std::to_string(m) + "m";
        }

        time_t now = time(nullptr);
        char date_buf[32];
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

        // ASCII art logo
        const char* logo[] = {
            "\033[1;36m    _  _          ___  ___  \033[0m",
            "\033[1;36m   | || |___ _ _ / _ \\/ __| \033[0m",
            "\033[1;36m   | __ / -_) '_| (_) \\__ \\ \033[0m",
            "\033[1;36m   |_||_\\___|_|  \\___/|___/ \033[0m",
            "\033[1;36m                            \033[0m",
        };

        std::string line0 = "\033[1;36m" + user + "\033[0m@\033[1;36m" + hostname + "\033[0m";
        std::string line1 = "-------------------";
        std::string line2 = "\033[1;33mOS\033[0m:      HerOS 1.0.0 x86_64";
        std::string line3 = "\033[1;33mKernel\033[0m:  HerOS Microkernel";
        std::string line4 = "\033[1;33mUptime\033[0m:  " + uptime_str;
        std::string line5 = "\033[1;33mShell\033[0m:   herosh 1.0";
        std::string line6 = "\033[1;33mProcs\033[0m:   " + std::to_string(procs);
        std::string line7 = "\033[1;33mTerminal\033[0m: HerOS Terminal";
        std::string line8 = "\033[1;33mDate\033[0m:    " + std::string(date_buf);
        std::string line9 = "";
        // Color palette
        std::string palette1 = "\033[40m  \033[41m  \033[42m  \033[43m  \033[44m  \033[45m  \033[46m  \033[47m  \033[0m";
        std::string palette2 = "\033[100m  \033[101m  \033[102m  \033[103m  \033[104m  \033[105m  \033[106m  \033[107m  \033[0m";

        std::string info_lines[] = {
            line0, line1, line2, line3, line4, line5, line6, line7, line8, line9, palette1, palette2
        };

        int logo_count = 5;
        int info_count = 12;
        int max_lines = std::max(logo_count, info_count);

        for (int i = 0; i < max_lines; i++) {
            std::string left = (i < logo_count) ? logo[i] : "                            ";
            std::string right = (i < info_count) ? info_lines[i] : "";
            ctx.outln(left + "  " + right);
        }

        return 0;
    });
}
