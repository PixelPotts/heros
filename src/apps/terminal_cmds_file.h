#pragma once
#include "terminal_shell.h"
#include <algorithm>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <cmath>

// ── Helpers ─────────────────────────────────────────────────────

static inline std::string human_size(uint64_t bytes) {
    const char* units[] = {"B", "K", "M", "G", "T"};
    int u = 0;
    double sz = (double)bytes;
    while (sz >= 1024.0 && u < 4) { sz /= 1024.0; u++; }
    char buf[32];
    if (u == 0) snprintf(buf, sizeof(buf), "%lu", (unsigned long)bytes);
    else snprintf(buf, sizeof(buf), "%.1f%s", sz, units[u]);
    return buf;
}

static inline std::string format_time(time_t t) {
    char buf[32];
    struct tm* tm = localtime(&t);
    strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
    return buf;
}

// ── Color codes for ls ──────────────────────────────────────────

static inline std::string ls_color(const FileInfo& f) {
    if (f.is_directory) return "\033[1;34m"; // bold blue
    auto dot = f.name.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = f.name.substr(dot);
        if (ext == ".so" || ext == ".exe") return "\033[1;32m"; // bold green
        if (ext == ".tar" || ext == ".gz" || ext == ".zip") return "\033[1;31m"; // bold red
        if (ext == ".h" || ext == ".cpp" || ext == ".py" || ext == ".js") return "\033[0;33m"; // yellow
        if (ext == ".conf" || ext == ".json" || ext == ".xml") return "\033[0;36m"; // cyan
        if (ext == ".jpg" || ext == ".png" || ext == ".svg") return "\033[1;35m"; // magenta
    }
    return "";
}

// ── 20 File Commands ────────────────────────────────────────────

inline void register_file_commands(ShellEngine& shell) {

    // 1. ls — list directory contents
    shell.register_cmd("ls", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_l = false, flag_a = false, flag_R = false, flag_h = false;
        std::vector<std::string> paths;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') {
                for (size_t j = 1; j < args[i].size(); j++) {
                    switch (args[i][j]) {
                        case 'l': flag_l = true; break;
                        case 'a': flag_a = true; break;
                        case 'R': flag_R = true; break;
                        case 'h': flag_h = true; break;
                    }
                }
            } else {
                paths.push_back(args[i]);
            }
        }
        if (paths.empty()) paths.push_back(".");

        std::function<void(const std::string&, bool)> list_dir;
        list_dir = [&](const std::string& dir, bool show_header) {
            std::string abs = resolve_path(ctx.cwd, dir);
            auto entries = ctx.fs->list(abs);

            std::sort(entries.begin(), entries.end(),
                [](const FileInfo& a, const FileInfo& b) { return a.name < b.name; });

            if (show_header) ctx.outln(abs + ":");

            for (auto& f : entries) {
                if (!flag_a && !f.name.empty() && f.name[0] == '.') continue;

                if (flag_l) {
                    std::string perms = f.is_directory ? "drwxr-xr-x" : "-rw-r--r--";
                    std::string sz = flag_h ? human_size(f.size) : std::to_string(f.size);
                    char line[256];
                    snprintf(line, sizeof(line), "%s 1 hero hero %8s %s ",
                             perms.c_str(), sz.c_str(), format_time(f.modified).c_str());
                    std::string color = ls_color(f);
                    if (!color.empty()) {
                        ctx.out(std::string(line) + color + f.name + "\033[0m\n");
                    } else {
                        ctx.outln(std::string(line) + f.name);
                    }
                } else {
                    std::string color = ls_color(f);
                    if (!color.empty()) {
                        ctx.out(color + f.name + "\033[0m  ");
                    } else {
                        ctx.out(f.name + "  ");
                    }
                }
            }
            if (!flag_l) ctx.out("\n");

            if (flag_R) {
                for (auto& f : entries) {
                    if (!flag_a && !f.name.empty() && f.name[0] == '.') continue;
                    if (f.is_directory) {
                        ctx.out("\n");
                        std::string sub = abs;
                        if (sub.back() != '/') sub += '/';
                        sub += f.name;
                        list_dir(sub, true);
                    }
                }
            }
        };

        list_dir(paths[0], paths.size() > 1 || flag_R);
        return 0;
    });

    // 2. cd — change directory
    shell.register_cmd("cd", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string target;
        if (args.size() < 2 || args[1] == "~") {
            target = shell.env().count("HOME") ? shell.env()["HOME"] : "/home";
        } else if (args[1] == "-") {
            target = shell.env().count("OLDPWD") ? shell.env()["OLDPWD"] : ctx.cwd;
        } else {
            target = resolve_path(ctx.cwd, args[1]);
        }

        if (ctx.fs && ctx.fs->exists(target)) {
            auto info = ctx.fs->stat(target);
            if (!info.is_directory) {
                ctx.outln("cd: not a directory: " + args[1]);
                return 1;
            }
            shell.env()["OLDPWD"] = ctx.cwd;
            ctx.cwd = target;
        } else {
            // Even if VFS doesn't know about it, allow navigation
            shell.env()["OLDPWD"] = ctx.cwd;
            ctx.cwd = target;
        }
        return 0;
    });

    // 3. pwd — print working directory
    shell.register_cmd("pwd", [](std::vector<std::string>&, CmdContext& ctx) -> int {
        ctx.outln(ctx.cwd);
        return 0;
    });

    // 4. cat — concatenate files
    shell.register_cmd("cat", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_n = false;

        // If no file args, pass through stdin
        if (args.size() < 2) {
            if (!ctx.stdin_data.empty()) ctx.out(ctx.stdin_data);
            return 0;
        }

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-n") { flag_n = true; continue; }
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            if (content.empty() && !ctx.fs->exists(path)) {
                ctx.outln("cat: " + args[i] + ": No such file or directory");
                return 1;
            }
            if (flag_n) {
                std::istringstream ss(content);
                std::string line;
                int n = 1;
                while (std::getline(ss, line)) {
                    char num[16];
                    snprintf(num, sizeof(num), "%6d\t", n++);
                    ctx.outln(std::string(num) + line);
                }
            } else {
                ctx.out(content);
                if (!content.empty() && content.back() != '\n') ctx.out("\n");
            }
        }
        return 0;
    });

    // 5. cp — copy files
    shell.register_cmd("cp", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_r = false;
        std::vector<std::string> paths;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-r" || args[i] == "-R") flag_r = true;
            else paths.push_back(args[i]);
        }

        if (paths.size() < 2) { ctx.outln("cp: missing operand"); return 1; }
        std::string src = resolve_path(ctx.cwd, paths[0]);
        std::string dst = resolve_path(ctx.cwd, paths[1]);

        auto info = ctx.fs->stat(src);
        if (info.is_directory && !flag_r) {
            ctx.outln("cp: omitting directory '" + paths[0] + "' (use -r)");
            return 1;
        }

        std::string content = ctx.fs->read(src);
        if (!ctx.fs->write(dst, content)) {
            ctx.outln("cp: cannot copy to '" + paths[1] + "'");
            return 1;
        }
        return 0;
    });

    // 6. mv — move/rename files
    shell.register_cmd("mv", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        if (args.size() < 3) { ctx.outln("mv: missing operand"); return 1; }
        std::string src = resolve_path(ctx.cwd, args[1]);
        std::string dst = resolve_path(ctx.cwd, args[2]);

        std::string content = ctx.fs->read(src);
        if (!ctx.fs->exists(src)) {
            ctx.outln("mv: cannot stat '" + args[1] + "': No such file or directory");
            return 1;
        }
        ctx.fs->write(dst, content);
        ctx.fs->remove(src);
        return 0;
    });

    // 7. rm — remove files
    shell.register_cmd("rm", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_r = false, flag_f = false;
        std::vector<std::string> paths;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') {
                for (size_t j = 1; j < args[i].size(); j++) {
                    if (args[i][j] == 'r' || args[i][j] == 'R') flag_r = true;
                    if (args[i][j] == 'f') flag_f = true;
                }
            } else {
                paths.push_back(args[i]);
            }
        }

        for (auto& p : paths) {
            std::string abs = resolve_path(ctx.cwd, p);
            if (!ctx.fs->exists(abs)) {
                if (!flag_f) ctx.outln("rm: cannot remove '" + p + "': No such file or directory");
                continue;
            }
            auto info = ctx.fs->stat(abs);
            if (info.is_directory && !flag_r) {
                ctx.outln("rm: cannot remove '" + p + "': Is a directory (use -r)");
                continue;
            }
            ctx.fs->remove(abs);
        }
        return 0;
    });

    // 8. mkdir — make directories
    shell.register_cmd("mkdir", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_p = false;
        std::vector<std::string> paths;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-p") flag_p = true;
            else paths.push_back(args[i]);
        }

        for (auto& p : paths) {
            std::string abs = resolve_path(ctx.cwd, p);
            if (flag_p) {
                // Create parent directories
                std::string build;
                std::istringstream ss(abs);
                std::string part;
                while (std::getline(ss, part, '/')) {
                    if (part.empty()) continue;
                    build += "/" + part;
                    ctx.fs->mkdir(build);
                }
            } else {
                if (!ctx.fs->mkdir(abs)) {
                    ctx.outln("mkdir: cannot create directory '" + p + "'");
                }
            }
        }
        return 0;
    });

    // 9. rmdir — remove empty directories
    shell.register_cmd("rmdir", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        for (size_t i = 1; i < args.size(); i++) {
            std::string abs = resolve_path(ctx.cwd, args[i]);
            auto entries = ctx.fs->list(abs);
            if (!entries.empty()) {
                ctx.outln("rmdir: failed to remove '" + args[i] + "': Directory not empty");
            } else {
                ctx.fs->remove(abs);
            }
        }
        return 0;
    });

    // 10. touch — create empty file or update timestamp
    shell.register_cmd("touch", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        for (size_t i = 1; i < args.size(); i++) {
            std::string abs = resolve_path(ctx.cwd, args[i]);
            if (!ctx.fs->exists(abs)) {
                ctx.fs->write(abs, "");
            }
            // VFS doesn't have utime, but write creates/updates the file
        }
        return 0;
    });

    // 11. find — search for files
    shell.register_cmd("find", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        std::string start_dir = ".";
        std::string name_pattern;
        std::string type_filter; // "f" or "d"

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-name" && i + 1 < args.size()) {
                name_pattern = args[++i];
            } else if (args[i] == "-type" && i + 1 < args.size()) {
                type_filter = args[++i];
            } else if (args[i][0] != '-') {
                start_dir = args[i];
            }
        }

        std::string abs_start = resolve_path(ctx.cwd, start_dir);

        std::function<void(const std::string&)> search;
        int count = 0;
        search = [&](const std::string& dir) {
            if (count > 1000) return;
            auto entries = ctx.fs->list(dir);
            for (auto& f : entries) {
                if (f.name == "." || f.name == "..") continue;
                count++;
                std::string full = dir;
                if (full.back() != '/') full += '/';
                full += f.name;

                bool match = true;
                if (!name_pattern.empty()) {
                    // Simple glob: * at start/end
                    if (name_pattern[0] == '*') {
                        std::string suffix = name_pattern.substr(1);
                        match = f.name.size() >= suffix.size() &&
                                f.name.substr(f.name.size() - suffix.size()) == suffix;
                    } else if (name_pattern.back() == '*') {
                        std::string prefix = name_pattern.substr(0, name_pattern.size() - 1);
                        match = f.name.substr(0, prefix.size()) == prefix;
                    } else {
                        match = (f.name == name_pattern);
                    }
                }
                if (!type_filter.empty()) {
                    if (type_filter == "f" && f.is_directory) match = false;
                    if (type_filter == "d" && !f.is_directory) match = false;
                }

                if (match) ctx.outln(full);
                if (f.is_directory) search(full);
            }
        };

        search(abs_start);
        return 0;
    });

    // 12. ln — create link (stub)
    shell.register_cmd("ln", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("ln: symbolic links not supported in VFS");
        return 1;
    });

    // 13. chmod — change permissions (stub)
    shell.register_cmd("chmod", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 3) { ctx.outln("chmod: missing operand"); return 1; }
        ctx.outln("chmod: permissions simulated (VFS does not track permissions)");
        return 0;
    });

    // 14. chown — change ownership (stub)
    shell.register_cmd("chown", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 3) { ctx.outln("chown: missing operand"); return 1; }
        ctx.outln("chown: ownership simulated (VFS single-user)");
        return 0;
    });

    // 15. stat — display file status
    shell.register_cmd("stat", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("stat: missing file operand"); return 1; }
        std::string path = resolve_path(ctx.cwd, args[1]);
        if (!ctx.fs->exists(path)) {
            ctx.outln("stat: cannot stat '" + args[1] + "': No such file or directory");
            return 1;
        }
        auto info = ctx.fs->stat(path);
        ctx.outln("  File: " + info.name);
        ctx.outln("  Size: " + std::to_string(info.size) + "\t" +
                  (info.is_directory ? "directory" : "regular file"));
        ctx.outln("  Path: " + info.path);
        ctx.outln("Modify: " + format_time(info.modified));
        ctx.outln("  MIME: " + (info.mime_type.empty() ? "application/octet-stream" : info.mime_type));
        return 0;
    });

    // 16. file — determine file type
    shell.register_cmd("file", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("file: missing operand"); return 1; }
        std::string path = resolve_path(ctx.cwd, args[1]);
        if (!ctx.fs->exists(path)) {
            ctx.outln(args[1] + ": cannot open (No such file)");
            return 1;
        }
        auto info = ctx.fs->stat(path);
        if (info.is_directory) {
            ctx.outln(args[1] + ": directory");
        } else if (!info.mime_type.empty()) {
            ctx.outln(args[1] + ": " + info.mime_type);
        } else {
            std::string content = ctx.fs->read(path);
            bool is_text = true;
            for (size_t i = 0; i < std::min(content.size(), (size_t)512); i++) {
                unsigned char c = content[i];
                if (c < 0x09 || (c > 0x0d && c < 0x20 && c != 0x1b)) {
                    is_text = false; break;
                }
            }
            ctx.outln(args[1] + ": " + (is_text ? "ASCII text" : "data"));
        }
        return 0;
    });

    // 17. tree — display directory tree
    shell.register_cmd("tree", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        std::string start = (args.size() > 1) ? args[1] : ".";
        std::string abs = resolve_path(ctx.cwd, start);

        int dirs = 0, files = 0;
        std::function<void(const std::string&, const std::string&)> show;
        show = [&](const std::string& dir, const std::string& prefix) {
            auto entries = ctx.fs->list(dir);
            std::sort(entries.begin(), entries.end(),
                [](const FileInfo& a, const FileInfo& b) { return a.name < b.name; });

            for (size_t i = 0; i < entries.size(); i++) {
                auto& f = entries[i];
                if (f.name == "." || f.name == "..") continue;
                bool last = (i == entries.size() - 1);
                std::string connector = last ? "└── " : "├── ";
                std::string color = f.is_directory ? "\033[1;34m" : "";
                std::string reset = f.is_directory ? "\033[0m" : "";
                ctx.outln(prefix + connector + color + f.name + reset);

                if (f.is_directory) {
                    dirs++;
                    std::string sub = dir;
                    if (sub.back() != '/') sub += '/';
                    sub += f.name;
                    show(sub, prefix + (last ? "    " : "│   "));
                } else {
                    files++;
                }
            }
        };

        ctx.outln(abs);
        show(abs, "");
        ctx.outln("\n" + std::to_string(dirs) + " directories, " + std::to_string(files) + " files");
        return 0;
    });

    // 18. du — disk usage
    shell.register_cmd("du", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_h = false;
        std::string target = ".";

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-h") flag_h = true;
            else target = args[i];
        }

        std::string abs = resolve_path(ctx.cwd, target);

        std::function<uint64_t(const std::string&)> calc;
        calc = [&](const std::string& dir) -> uint64_t {
            uint64_t total = 0;
            auto entries = ctx.fs->list(dir);
            for (auto& f : entries) {
                if (f.name == "." || f.name == "..") continue;
                std::string sub = dir;
                if (sub.back() != '/') sub += '/';
                sub += f.name;
                if (f.is_directory) {
                    uint64_t sub_total = calc(sub);
                    total += sub_total;
                    std::string sz = flag_h ? human_size(sub_total) : std::to_string(sub_total / 1024);
                    ctx.outln(sz + "\t" + sub);
                } else {
                    total += f.size;
                }
            }
            return total;
        };

        uint64_t total = calc(abs);
        std::string sz = flag_h ? human_size(total) : std::to_string(total / 1024);
        ctx.outln(sz + "\t" + abs);
        return 0;
    });

    // 19. df — disk free (simulated)
    shell.register_cmd("df", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_h = false;
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-h") flag_h = true;
        }

        if (flag_h) {
            ctx.outln("Filesystem      Size  Used Avail Use% Mounted on");
            ctx.outln("heros-vfs       2.0G  128M  1.9G   7% /");
            ctx.outln("tmpfs           512M     0  512M   0% /tmp");
        } else {
            ctx.outln("Filesystem     1K-blocks    Used Available Use% Mounted on");
            ctx.outln("heros-vfs        2097152  131072   1966080   7% /");
            ctx.outln("tmpfs             524288       0    524288   0% /tmp");
        }
        return 0;
    });

    // 20. realpath — resolve path
    shell.register_cmd("realpath", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("realpath: missing operand"); return 1; }
        for (size_t i = 1; i < args.size(); i++) {
            ctx.outln(resolve_path(ctx.cwd, args[i]));
        }
        return 0;
    });
}
