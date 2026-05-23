#pragma once
#include "terminal_shell.h"
#include "../process.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstdio>

// ── 15 System Commands ──────────────────────────────────────────

inline void register_system_commands(ShellEngine& shell) {

    // 1. ps — list processes
    shell.register_cmd("ps", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        if (!ctx.pm) { ctx.outln("ps: no process manager"); return 1; }
        auto procs = ctx.pm->list_processes();
        ctx.outln("  PID  STATE       APP ID                  CPU(ms)");
        ctx.outln("-----  ----------  ----------------------  -------");
        for (auto* p : procs) {
            char line[128];
            snprintf(line, sizeof(line), "%5u  %-10s  %-22s  %7.1f",
                     p->pid, process_state_str(p->state), p->app_id.c_str(), p->cpu_time_ms);
            ctx.outln(line);
        }
        return 0;
    });

    // 2. kill — send signal to process
    shell.register_cmd("kill", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.pm || !ctx.wm) return 1;
        bool force = false;
        uint32_t pid = 0;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-9" || args[i] == "-KILL") force = true;
            else {
                try { pid = (uint32_t)std::stoul(args[i]); } catch (...) {}
            }
        }

        if (pid == 0) { ctx.outln("kill: usage: kill [-9] PID"); return 1; }

        auto* p = ctx.pm->get_process(pid);
        if (!p) { ctx.outln("kill: no such process: " + std::to_string(pid)); return 1; }

        if (force) {
            ctx.pm->kill(pid, *ctx.wm);
        } else {
            ctx.pm->terminate(pid, *ctx.wm);
        }
        return 0;
    });

    // 3. top — process snapshot
    shell.register_cmd("top", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        if (!ctx.pm) return 1;
        auto procs = ctx.pm->list_processes();

        time_t uptime = time(nullptr) - ctx.pm->start_time();
        int hours = (int)(uptime / 3600);
        int mins = (int)((uptime % 3600) / 60);

        char buf[128];
        ctx.outln("top - HerOS Process Monitor");
        snprintf(buf, sizeof(buf), "Uptime: %dh %dm, Processes: %d", hours, mins, (int)procs.size());
        ctx.outln(buf);
        ctx.outln("");
        ctx.outln("  PID  STATE       APP ID                  CPU(ms)");
        ctx.outln("-----  ----------  ----------------------  -------");

        for (auto* p : procs) {
            snprintf(buf, sizeof(buf), "%5u  %-10s  %-22s  %7.1f",
                     p->pid, process_state_str(p->state), p->app_id.c_str(), p->cpu_time_ms);
            ctx.outln(buf);
        }
        return 0;
    });

    // 4. uptime — system uptime
    shell.register_cmd("uptime", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        if (!ctx.pm) { ctx.outln("uptime: unknown"); return 1; }
        time_t uptime = time(nullptr) - ctx.pm->start_time();
        int hours = (int)(uptime / 3600);
        int mins = (int)((uptime % 3600) / 60);
        int secs = (int)(uptime % 60);

        char buf[128];
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);

        snprintf(buf, sizeof(buf), " %s up %d:%02d:%02d, %d processes",
                 time_str, hours, mins, secs, ctx.pm->process_count());
        ctx.outln(buf);
        return 0;
    });

    // 5. whoami
    shell.register_cmd("whoami", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        auto it = ctx.env.find("USER");
        ctx.outln(it != ctx.env.end() ? it->second : "hero");
        return 0;
    });

    // 6. hostname
    shell.register_cmd("hostname", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        auto it = ctx.env.find("HOSTNAME");
        ctx.outln(it != ctx.env.end() ? it->second : "heros");
        return 0;
    });

    // 7. uname — system info
    shell.register_cmd("uname", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_a = false, flag_s = false, flag_r = false, flag_m = false;
        bool any = false;

        for (size_t i = 1; i < args.size(); i++) {
            for (size_t j = (args[i][0] == '-' ? 1 : 0); j < args[i].size(); j++) {
                switch (args[i][j]) {
                    case 'a': flag_a = true; any = true; break;
                    case 's': flag_s = true; any = true; break;
                    case 'r': flag_r = true; any = true; break;
                    case 'm': flag_m = true; any = true; break;
                }
            }
        }

        if (!any) flag_s = true;

        std::string out;
        if (flag_a || flag_s) out += "HerOS";
        if (flag_a) out += " heros";
        if (flag_a || flag_r) { if (!out.empty()) out += " "; out += "1.0.0"; }
        if (flag_a) out += " #1 SMP";
        if (flag_a || flag_m) { if (!out.empty()) out += " "; out += "x86_64"; }

        ctx.outln(out);
        return 0;
    });

    // 8. date — display date/time
    shell.register_cmd("date", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string fmt = "%a %b %d %H:%M:%S %Z %Y";

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '+') {
                fmt = args[i].substr(1);
            }
        }

        time_t now = time(nullptr);
        char buf[256];
        strftime(buf, sizeof(buf), fmt.c_str(), localtime(&now));
        ctx.outln(buf);
        return 0;
    });

    // 9. cal — calendar
    shell.register_cmd("cal", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        int year = tm->tm_year + 1900;
        int month = tm->tm_mon + 1;
        int today = tm->tm_mday;

        if (args.size() >= 3) {
            try { month = std::stoi(args[1]); year = std::stoi(args[2]); today = -1; }
            catch (...) {}
        } else if (args.size() >= 2) {
            try { year = std::stoi(args[1]); month = 1; today = -1; }
            catch (...) {}
        }

        static const char* months[] = {"","January","February","March","April","May","June",
            "July","August","September","October","November","December"};
        static const int days_in[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

        // Leap year
        int max_day = days_in[month];
        if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) max_day = 29;

        // Title
        char title[32];
        snprintf(title, sizeof(title), "   %s %d", months[month], year);
        ctx.outln(title);
        ctx.outln("Su Mo Tu We Th Fr Sa");

        // Day of week for 1st of month (Zeller's congruence)
        struct tm first = {};
        first.tm_year = year - 1900;
        first.tm_mon = month - 1;
        first.tm_mday = 1;
        mktime(&first);
        int dow = first.tm_wday; // 0=Sunday

        std::string line;
        for (int i = 0; i < dow; i++) line += "   ";

        for (int d = 1; d <= max_day; d++) {
            char day[32];
            if (d == today) {
                snprintf(day, sizeof(day), "\033[7m%2d\033[0m", d);
            } else {
                snprintf(day, sizeof(day), "%2d", d);
            }
            line += day;
            if ((dow + d) % 7 == 0) {
                ctx.outln(line);
                line.clear();
            } else {
                line += " ";
            }
        }
        if (!line.empty()) ctx.outln(line);
        return 0;
    });

    // 10. time — time a command
    shell.register_cmd("time", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("time: usage: time COMMAND"); return 1; }

        // Build sub-command line
        std::string cmdline;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) cmdline += " ";
            cmdline += args[i];
        }

        auto start = std::chrono::steady_clock::now();
        int status = shell.execute(cmdline, ctx);
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        char buf[64];
        snprintf(buf, sizeof(buf), "\nreal\t%.3fs", ms / 1000.0);
        ctx.outln(buf);
        return status;
    });

    // 11. sleep — pause execution (max 10s)
    shell.register_cmd("sleep", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("sleep: missing operand"); return 1; }
        double secs = 0;
        try { secs = std::stod(args[1]); } catch (...) { ctx.outln("sleep: invalid number"); return 1; }
        if (secs > 10.0) { secs = 10.0; ctx.outln("sleep: capped at 10 seconds"); }
        if (secs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(secs * 1000)));
        }
        return 0;
    });

    // 12. id — print user/group IDs
    shell.register_cmd("id", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        auto it = ctx.env.find("USER");
        std::string user = (it != ctx.env.end()) ? it->second : "hero";
        ctx.outln("uid=1000(" + user + ") gid=1000(" + user + ") groups=1000(" + user + "),27(sudo)");
        return 0;
    });

    // 13. w — who is logged in
    shell.register_cmd("w", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        time_t now = time(nullptr);
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));

        auto it = ctx.env.find("USER");
        std::string user = (it != ctx.env.end()) ? it->second : "hero";

        ctx.outln(std::string(" ") + time_str + " up 0:00, 1 user");
        ctx.outln("USER     TTY      FROM             LOGIN@  IDLE  WHAT");
        ctx.outln(user + "    tty1     :0               " + std::string(time_str) + "  0:00  herosh");
        return 0;
    });

    // 14. free — memory usage (simulated)
    shell.register_cmd("free", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_h = false;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-h") flag_h = true;
        }

        if (flag_h) {
            ctx.outln("              total        used        free      shared  buff/cache   available");
            ctx.outln("Mem:          4.0Gi       1.2Gi       2.1Gi       256Mi       700Mi       2.5Gi");
            ctx.outln("Swap:         2.0Gi          0B       2.0Gi");
        } else {
            ctx.outln("              total        used        free      shared  buff/cache   available");
            ctx.outln("Mem:        4194304     1258291     2202009      262144      734004     2621440");
            ctx.outln("Swap:       2097152           0     2097152");
        }
        return 0;
    });

    // 15. lsof — list open files (simulated)
    shell.register_cmd("lsof", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        if (!ctx.pm) return 1;
        auto procs = ctx.pm->list_processes();
        ctx.outln("COMMAND     PID   USER   FD   TYPE   NAME");
        for (auto* p : procs) {
            char line[128];
            snprintf(line, sizeof(line), "%-10s %5u  hero   cwd   DIR    /",
                     p->app_id.c_str(), p->pid);
            ctx.outln(line);
        }
        return 0;
    });
}
