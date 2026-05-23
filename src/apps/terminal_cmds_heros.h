#pragma once
#include "terminal_shell.h"
#include "../app_registry.h"
#include "../process.h"
#include "../event_bus.h"
#include <ctime>
#include <sstream>
#include <algorithm>

// ── 8 HerOS-Specific Commands ───────────────────────────────────

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

        ctx.outln("\033[1;36mHerOS Terminal — 100 Built-in Commands\033[0m");
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

        ctx.outln("\033[1;33mHerOS (8):\033[0m");
        ctx.outln("  launch apps notify theme wallpaper help man neofetch");
        ctx.outln("");

        ctx.outln("Type '\033[1mman COMMAND\033[0m' for details on any command.");
        ctx.outln("Supports pipes (|), redirects (> >>), semicolons (;), and env vars ($VAR).");

        (void)shell;
        return 0;
    });

    // 7. man — manual page for a command
    shell.register_cmd("man", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("man: what manual page do you want?"); return 1; }

        struct ManPage { const char* name; const char* synopsis; const char* desc; };
        static const ManPage pages[] = {
            {"echo", "echo [-n] [-e] [STRING...]", "Print STRING(s) to standard output. -n: no newline, -e: interpret escapes."},
            {"printf", "printf FORMAT [ARG...]", "Format and print data. Supports %s, %d, \\n, \\t."},
            {"env", "env", "Print all environment variables."},
            {"export", "export NAME=VALUE", "Set an environment variable."},
            {"unset", "unset NAME", "Remove an environment variable."},
            {"alias", "alias NAME=VALUE", "Create a command alias."},
            {"unalias", "unalias NAME", "Remove a command alias."},
            {"history", "history", "Show command history (use Up/Down arrows)."},
            {"which", "which COMMAND", "Show the path of a command."},
            {"type", "type COMMAND", "Describe how a command name is interpreted."},
            {"source", "source FILE", "Execute commands from a file in the current shell."},
            {"read", "read [VAR]", "Read a line from stdin into VAR (default: REPLY)."},
            {"test", "test EXPR  or  [ EXPR ]", "Evaluate conditional expression. Supports -z, -n, -e, -f, -d, =, !=, -eq, -lt, etc."},
            {"true", "true", "Return success (exit code 0)."},
            {"false", "false", "Return failure (exit code 1)."},
            {"exit", "exit", "Close the terminal."},
            {"clear", "clear", "Clear the terminal screen."},
            {"set", "set", "Display all shell variables."},
            {"yes", "yes [STRING]", "Output STRING (default: 'y') repeatedly (max 100 lines)."},
            {"seq", "seq [FIRST [INC]] LAST", "Print a sequence of numbers."},
            {"ls", "ls [-l] [-a] [-R] [-h] [PATH]", "List directory contents. -l: long format, -a: show hidden, -R: recursive, -h: human sizes."},
            {"cd", "cd [DIR]", "Change working directory. Supports ~, -, and .."},
            {"pwd", "pwd", "Print the current working directory."},
            {"cat", "cat [-n] [FILE...]", "Concatenate and print files. -n: number lines."},
            {"cp", "cp [-r] SRC DST", "Copy files. -r: recursive for directories."},
            {"mv", "mv SRC DST", "Move or rename files."},
            {"rm", "rm [-r] [-f] FILE...", "Remove files. -r: recursive, -f: force."},
            {"mkdir", "mkdir [-p] DIR...", "Create directories. -p: create parents."},
            {"rmdir", "rmdir DIR", "Remove empty directories."},
            {"touch", "touch FILE...", "Create empty files or update timestamps."},
            {"find", "find [PATH] [-name PAT] [-type f|d]", "Search for files. Supports * glob in -name."},
            {"stat", "stat FILE", "Display file metadata."},
            {"file", "file FILE", "Determine file type."},
            {"tree", "tree [DIR]", "Display directory tree structure."},
            {"du", "du [-h] [PATH]", "Estimate file space usage. -h: human-readable."},
            {"df", "df [-h]", "Report filesystem disk space. -h: human-readable."},
            {"realpath", "realpath PATH", "Resolve a path to its absolute form."},
            {"grep", "grep [-i] [-n] [-v] [-c] [-r] PATTERN [FILE...]", "Search for PATTERN in files. -i: ignore case, -n: line numbers, -v: invert, -c: count, -r: recursive."},
            {"sed", "sed 's/PAT/REPL/[g]' [FILE]", "Stream editor. Supports s/pattern/replacement/ substitution."},
            {"awk", "awk '{print $N}' [FILE]", "Simplified field processor. Supports -F delimiter and $N field references."},
            {"sort", "sort [-r] [-n] [-u] [FILE]", "Sort lines. -r: reverse, -n: numeric, -u: unique."},
            {"uniq", "uniq [-c] [-d] [FILE]", "Remove duplicate adjacent lines. -c: count, -d: duplicates only."},
            {"wc", "wc [-l] [-w] [-c] [FILE...]", "Count lines, words, bytes."},
            {"head", "head [-n N] [FILE]", "Output first N lines (default: 10)."},
            {"tail", "tail [-n N] [FILE]", "Output last N lines (default: 10)."},
            {"cut", "cut -d DELIM -f FIELDS [FILE]", "Select fields from each line."},
            {"tr", "tr [-d] SET1 [SET2]", "Translate or delete characters. -d: delete."},
            {"diff", "diff FILE1 FILE2", "Compare files line by line (unified format)."},
            {"tee", "tee [-a] FILE...", "Copy stdin to stdout and files. -a: append."},
            {"rev", "rev [FILE]", "Reverse each line of input."},
            {"fold", "fold [-w WIDTH] [FILE]", "Wrap lines to specified width (default: 80)."},
            {"column", "column [-t] [FILE]", "Format input into columns. -t: table mode."},
            {"nl", "nl [FILE]", "Number non-empty lines."},
            {"strings", "strings FILE", "Print printable character sequences (4+ chars)."},
            {"base64", "base64 [-d] [FILE]", "Base64 encode or decode. -d: decode."},
            {"xargs", "xargs COMMAND [ARGS]", "Build and execute commands from stdin."},
            {"ps", "ps", "List running processes."},
            {"kill", "kill [-9] PID", "Terminate a process. -9: force kill."},
            {"top", "top", "Display process snapshot with system info."},
            {"uptime", "uptime", "Show system uptime and process count."},
            {"whoami", "whoami", "Print the current username."},
            {"hostname", "hostname", "Print the system hostname."},
            {"uname", "uname [-a] [-s] [-r] [-m]", "Print system information. -a: all, -s: name, -r: release, -m: machine."},
            {"date", "date [+FORMAT]", "Display current date/time. Supports strftime format."},
            {"cal", "cal [MONTH YEAR]", "Display a calendar."},
            {"time", "time COMMAND", "Time a command's execution."},
            {"sleep", "sleep SECONDS", "Pause for SECONDS (max 10s)."},
            {"id", "id", "Print user and group IDs."},
            {"w", "w", "Show who is logged in."},
            {"free", "free [-h]", "Display memory usage (simulated). -h: human-readable."},
            {"lsof", "lsof", "List open files by process."},
            {"curl", "curl [-o FILE] [-s] [-X METHOD] [-H HEADER] [-d DATA] URL", "HTTP client (libcurl). Supports GET, POST, PUT, DELETE."},
            {"wget", "wget [-O FILE] [-q] URL", "Download files. -O: output filename, -q: quiet."},
            {"ping", "ping [-c COUNT] HOST", "Simulated ping with real DNS resolution."},
            {"dig", "dig HOSTNAME", "DNS lookup (real resolution via getaddrinfo)."},
            {"ifconfig", "ifconfig", "Display network interfaces (simulated)."},
            {"netstat", "netstat", "Display network connections (simulated)."},
            {"nc", "nc", "Netcat (stub — use curl/wget instead)."},
            {"host", "host HOSTNAME", "DNS lookup (real resolution via getaddrinfo)."},
            {"traceroute", "traceroute HOST", "Simulated traceroute with real DNS resolution."},
            {"nslookup", "nslookup HOSTNAME", "DNS lookup (real resolution via getaddrinfo)."},
            {"tar", "tar [cxt] -f FILE [FILES...]", "Create/extract/list archives. Custom VFS format."},
            {"gzip", "gzip FILE", "Compress file using zlib."},
            {"gunzip", "gunzip FILE.gz", "Decompress gzip file."},
            {"zip", "zip ARCHIVE FILE...", "Create zip archive (libarchive)."},
            {"unzip", "unzip ARCHIVE.zip", "Extract zip archive (libarchive)."},
            {"md5sum", "md5sum [FILE...]", "Compute MD5 hash (OpenSSL)."},
            {"sha256sum", "sha256sum [FILE...]", "Compute SHA-256 hash (OpenSSL)."},
            {"launch", "launch APP_ID", "Launch a HerOS app. Short names auto-prefixed with com.heros."},
            {"apps", "apps", "List all installed HerOS apps and their status."},
            {"notify", "notify TITLE [BODY]", "Send a toast notification."},
            {"theme", "theme [list|set NAME]", "List or set the desktop theme."},
            {"wallpaper", "wallpaper", "Display wallpaper info."},
            {"help", "help", "Display categorized list of all 100 commands."},
            {"man", "man COMMAND", "Display manual page for a command."},
            {"neofetch", "neofetch", "Display system info with ASCII art."},
            {"ln", "ln", "Create links (not supported in VFS)."},
            {"chmod", "chmod MODE FILE", "Change permissions (simulated)."},
            {"chown", "chown OWNER FILE", "Change ownership (simulated)."},
            {"paste", "paste [-d DELIM] FILE...", "Merge lines of files side by side."},
        };

        std::string cmd = args[1];
        for (const auto& p : pages) {
            if (cmd == p.name) {
                ctx.outln("\033[1m" + std::string(p.name) + "\033[0m(1) — HerOS Manual");
                ctx.outln("");
                ctx.outln("\033[1mSYNOPSIS\033[0m");
                ctx.outln("  " + std::string(p.synopsis));
                ctx.outln("");
                ctx.outln("\033[1mDESCRIPTION\033[0m");
                ctx.outln("  " + std::string(p.desc));
                return 0;
            }
        }

        ctx.outln("No manual entry for " + cmd);
        return 1;
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

        const char* info[] = {
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
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
