#pragma once
#include "terminal_shell.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <set>
#include <cstdlib>

// ── 20 Text Processing Commands ─────────────────────────────────

inline void register_text_commands(ShellEngine& shell) {

    // 1. grep — search patterns
    shell.register_cmd("grep", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs) return 1;
        bool flag_i = false, flag_n = false, flag_v = false, flag_c = false, flag_r = false;
        std::string pattern;
        std::vector<std::string> files;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-' && args[i].size() > 1) {
                for (size_t j = 1; j < args[i].size(); j++) {
                    switch (args[i][j]) {
                        case 'i': flag_i = true; break;
                        case 'n': flag_n = true; break;
                        case 'v': flag_v = true; break;
                        case 'c': flag_c = true; break;
                        case 'r': flag_r = true; break;
                    }
                }
            } else if (pattern.empty()) {
                pattern = args[i];
            } else {
                files.push_back(args[i]);
            }
        }

        if (pattern.empty()) { ctx.outln("grep: missing pattern"); return 1; }

        auto flags = std::regex::ECMAScript;
        if (flag_i) flags |= std::regex::icase;
        std::regex re;
        try { re = std::regex(pattern, flags); }
        catch (...) { ctx.outln("grep: invalid pattern"); return 2; }

        auto grep_content = [&](const std::string& content, const std::string& prefix) {
            std::istringstream ss(content);
            std::string line;
            int n = 0, count = 0;
            while (std::getline(ss, line)) {
                n++;
                bool match = std::regex_search(line, re);
                if (flag_v) match = !match;
                if (match) {
                    count++;
                    if (!flag_c) {
                        std::string out;
                        if (!prefix.empty()) out += prefix + ":";
                        if (flag_n) out += std::to_string(n) + ":";
                        out += line;
                        ctx.outln(out);
                    }
                }
            }
            if (flag_c) {
                std::string out;
                if (!prefix.empty()) out += prefix + ":";
                out += std::to_string(count);
                ctx.outln(out);
            }
            return count > 0 ? 0 : 1;
        };

        if (files.empty()) {
            // grep from stdin
            return grep_content(ctx.stdin_data, "");
        }

        int result = 1;
        std::function<void(const std::string&)> process_file;
        process_file = [&](const std::string& filepath) {
            std::string abs = resolve_path(ctx.cwd, filepath);
            if (flag_r) {
                auto info = ctx.fs->stat(abs);
                if (info.is_directory) {
                    auto entries = ctx.fs->list(abs);
                    for (auto& f : entries) {
                        if (f.name == "." || f.name == "..") continue;
                        std::string sub = abs;
                        if (sub.back() != '/') sub += '/';
                        sub += f.name;
                        process_file(sub);
                    }
                    return;
                }
            }
            std::string content = ctx.fs->read(abs);
            if (grep_content(content, files.size() > 1 || flag_r ? filepath : "") == 0) result = 0;
        };

        for (auto& f : files) process_file(f);
        return result;
    });

    // 2. sed — stream editor (s/pat/repl/g)
    shell.register_cmd("sed", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("sed: missing expression"); return 1; }

        std::string expr = args[1];
        std::string input;
        if (args.size() > 2 && ctx.fs) {
            std::string path = resolve_path(ctx.cwd, args[2]);
            input = ctx.fs->read(path);
        } else {
            input = ctx.stdin_data;
        }

        // Parse s/pattern/replacement/flags
        if (expr.size() >= 4 && expr[0] == 's') {
            char delim = expr[1];
            size_t p2 = expr.find(delim, 2);
            if (p2 == std::string::npos) { ctx.outln("sed: invalid expression"); return 1; }
            size_t p3 = expr.find(delim, p2 + 1);
            std::string pat = expr.substr(2, p2 - 2);
            std::string repl = (p3 != std::string::npos) ? expr.substr(p2 + 1, p3 - p2 - 1) : expr.substr(p2 + 1);
            bool global = (p3 != std::string::npos && expr.find('g', p3) != std::string::npos);

            try {
                std::regex re(pat);
                std::istringstream ss(input);
                std::string line;
                while (std::getline(ss, line)) {
                    if (global) {
                        ctx.outln(std::regex_replace(line, re, repl));
                    } else {
                        ctx.outln(std::regex_replace(line, re, repl, std::regex_constants::format_first_only));
                    }
                }
            } catch (...) { ctx.outln("sed: invalid regex"); return 1; }
        } else {
            ctx.out(input);
        }
        return 0;
    });

    // 3. awk — simplified field processing
    shell.register_cmd("awk", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("awk: missing program"); return 1; }

        std::string program = args[1];
        std::string input;
        if (args.size() > 2 && ctx.fs) {
            std::string path = resolve_path(ctx.cwd, args[2]);
            input = ctx.fs->read(path);
        } else {
            input = ctx.stdin_data;
        }

        // Simplified: parse {print $N} patterns
        // Extract field separator (default: whitespace)
        std::string fs_delim = " ";
        for (size_t i = 1; i < args.size() - 1; i++) {
            if (args[i] == "-F" && i + 1 < args.size()) {
                fs_delim = args[i + 1];
                // shift program
                if (args.size() > i + 2) program = args[i + 2];
                break;
            }
        }

        // Extract print fields from {print $1, $2, ...}
        std::vector<int> fields;
        bool print_all = false;
        size_t brace_start = program.find('{');
        size_t brace_end = program.rfind('}');
        if (brace_start != std::string::npos && brace_end != std::string::npos) {
            std::string body = program.substr(brace_start + 1, brace_end - brace_start - 1);
            // look for "print" keyword
            size_t print_pos = body.find("print");
            if (print_pos != std::string::npos) {
                std::string rest = body.substr(print_pos + 5);
                if (rest.find('$') == std::string::npos) {
                    print_all = true;
                } else {
                    // parse $N references
                    for (size_t i = 0; i < rest.size(); i++) {
                        if (rest[i] == '$' && i + 1 < rest.size()) {
                            i++;
                            std::string num;
                            while (i < rest.size() && isdigit(rest[i])) num += rest[i++];
                            if (!num.empty()) fields.push_back(std::stoi(num));
                            i--;
                        }
                    }
                }
            }
        }

        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) {
            if (print_all) {
                ctx.outln(line);
                continue;
            }

            // Split line into fields
            std::vector<std::string> parts;
            if (fs_delim == " ") {
                std::istringstream ls(line);
                std::string tok;
                while (ls >> tok) parts.push_back(tok);
            } else {
                size_t pos = 0;
                while (pos < line.size()) {
                    size_t next = line.find(fs_delim, pos);
                    if (next == std::string::npos) {
                        parts.push_back(line.substr(pos));
                        break;
                    }
                    parts.push_back(line.substr(pos, next - pos));
                    pos = next + fs_delim.size();
                }
            }

            std::string output;
            for (size_t i = 0; i < fields.size(); i++) {
                if (i > 0) output += " ";
                int f = fields[i];
                if (f == 0) output += line; // $0 = whole line
                else if (f > 0 && f <= (int)parts.size()) output += parts[f - 1];
            }
            ctx.outln(output);
        }
        return 0;
    });

    // 4. sort — sort lines
    shell.register_cmd("sort", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_r = false, flag_n = false, flag_u = false;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') {
                for (size_t j = 1; j < args[i].size(); j++) {
                    switch (args[i][j]) {
                        case 'r': flag_r = true; break;
                        case 'n': flag_n = true; break;
                        case 'u': flag_u = true; break;
                    }
                }
            } else if (ctx.fs) {
                input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
            }
        }

        if (input.empty()) input = ctx.stdin_data;

        std::vector<std::string> lines;
        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) lines.push_back(line);

        if (flag_n) {
            std::sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
                try { return std::stod(a) < std::stod(b); }
                catch (...) { return a < b; }
            });
        } else {
            std::sort(lines.begin(), lines.end());
        }

        if (flag_r) std::reverse(lines.begin(), lines.end());

        if (flag_u) {
            auto it = std::unique(lines.begin(), lines.end());
            lines.erase(it, lines.end());
        }

        for (auto& l : lines) ctx.outln(l);
        return 0;
    });

    // 5. uniq — remove duplicates
    shell.register_cmd("uniq", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_c = false, flag_d = false;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-c") flag_c = true;
            else if (args[i] == "-d") flag_d = true;
            else if (ctx.fs) input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
        }

        if (input.empty()) input = ctx.stdin_data;

        std::istringstream ss(input);
        std::string line, prev;
        int count = 0;
        bool first = true;

        auto flush = [&]() {
            if (!first) {
                if (flag_d && count < 2) return;
                if (flag_c) {
                    char buf[16]; snprintf(buf, sizeof(buf), "%7d ", count);
                    ctx.outln(std::string(buf) + prev);
                } else {
                    ctx.outln(prev);
                }
            }
        };

        while (std::getline(ss, line)) {
            if (first || line != prev) {
                flush();
                prev = line;
                count = 1;
                first = false;
            } else {
                count++;
            }
        }
        flush();
        return 0;
    });

    // 6. wc — word/line/byte count
    shell.register_cmd("wc", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_l = false, flag_w = false, flag_c = false;
        bool any_flag = false;
        std::vector<std::string> files;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') {
                for (size_t j = 1; j < args[i].size(); j++) {
                    switch (args[i][j]) {
                        case 'l': flag_l = true; any_flag = true; break;
                        case 'w': flag_w = true; any_flag = true; break;
                        case 'c': flag_c = true; any_flag = true; break;
                    }
                }
            } else {
                files.push_back(args[i]);
            }
        }
        if (!any_flag) { flag_l = flag_w = flag_c = true; }

        auto count_str = [&](const std::string& content, const std::string& name) {
            int lines = 0, words = 0, bytes = (int)content.size();
            bool in_word = false;
            for (char c : content) {
                if (c == '\n') lines++;
                if (c == ' ' || c == '\n' || c == '\t') { in_word = false; }
                else if (!in_word) { in_word = true; words++; }
            }
            std::string out;
            if (flag_l) { char b[16]; snprintf(b, sizeof(b), "%8d", lines); out += b; }
            if (flag_w) { char b[16]; snprintf(b, sizeof(b), "%8d", words); out += b; }
            if (flag_c) { char b[16]; snprintf(b, sizeof(b), "%8d", bytes); out += b; }
            if (!name.empty()) out += " " + name;
            ctx.outln(out);
        };

        if (files.empty()) {
            count_str(ctx.stdin_data, "");
        } else {
            for (auto& f : files) {
                if (!ctx.fs) continue;
                std::string path = resolve_path(ctx.cwd, f);
                std::string content = ctx.fs->read(path);
                count_str(content, f);
            }
        }
        return 0;
    });

    // 7. head — output first lines
    shell.register_cmd("head", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        int n = 10;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-n" && i + 1 < args.size()) {
                try { n = std::stoi(args[++i]); } catch (...) {}
            } else if (ctx.fs) {
                input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
            }
        }
        if (input.empty()) input = ctx.stdin_data;

        std::istringstream ss(input);
        std::string line;
        int count = 0;
        while (std::getline(ss, line) && count < n) {
            ctx.outln(line);
            count++;
        }
        return 0;
    });

    // 8. tail — output last lines
    shell.register_cmd("tail", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        int n = 10;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-n" && i + 1 < args.size()) {
                try { n = std::stoi(args[++i]); } catch (...) {}
            } else if (ctx.fs) {
                input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
            }
        }
        if (input.empty()) input = ctx.stdin_data;

        std::vector<std::string> lines;
        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) lines.push_back(line);

        int start = std::max(0, (int)lines.size() - n);
        for (int i = start; i < (int)lines.size(); i++) {
            ctx.outln(lines[i]);
        }
        return 0;
    });

    // 9. cut — select fields
    shell.register_cmd("cut", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string delim = "\t";
        std::vector<int> fields;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-d" && i + 1 < args.size()) {
                delim = args[++i];
            } else if (args[i] == "-f" && i + 1 < args.size()) {
                std::string spec = args[++i];
                std::istringstream fs(spec);
                std::string f;
                while (std::getline(fs, f, ',')) {
                    try { fields.push_back(std::stoi(f)); } catch (...) {}
                }
            } else if (ctx.fs) {
                input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
            }
        }
        if (input.empty()) input = ctx.stdin_data;
        if (fields.empty()) { ctx.out(input); return 0; }

        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) {
            std::vector<std::string> parts;
            size_t pos = 0;
            while (pos < line.size()) {
                size_t next = line.find(delim, pos);
                if (next == std::string::npos) { parts.push_back(line.substr(pos)); break; }
                parts.push_back(line.substr(pos, next - pos));
                pos = next + delim.size();
            }

            std::string out;
            for (size_t i = 0; i < fields.size(); i++) {
                if (i > 0) out += delim;
                int f = fields[i] - 1;
                if (f >= 0 && f < (int)parts.size()) out += parts[f];
            }
            ctx.outln(out);
        }
        return 0;
    });

    // 10. tr — translate characters
    shell.register_cmd("tr", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("tr: missing operand"); return 1; }

        // Expand ranges like a-z → abcdefghijklmnopqrstuvwxyz
        auto expand_ranges = [](const std::string& s) -> std::string {
            std::string result;
            for (size_t i = 0; i < s.size(); i++) {
                if (i + 2 < s.size() && s[i + 1] == '-' && s[i + 2] >= s[i]) {
                    for (char c = s[i]; c <= s[i + 2]; c++) result += c;
                    i += 2;
                } else {
                    result += s[i];
                }
            }
            return result;
        };

        bool flag_d = false;
        std::string set1, set2;
        int si = 1;

        if (args[si] == "-d") { flag_d = true; si++; }
        if (si < (int)args.size()) set1 = expand_ranges(args[si++]);
        if (si < (int)args.size()) set2 = expand_ranges(args[si++]);

        std::string input = ctx.stdin_data;
        std::string output;
        output.reserve(input.size());

        for (char c : input) {
            auto pos = set1.find(c);
            if (pos != std::string::npos) {
                if (flag_d) continue;
                if (pos < set2.size()) output += set2[pos];
                else if (!set2.empty()) output += set2.back();
                else output += c;
            } else {
                output += c;
            }
        }

        ctx.out(output);
        return 0;
    });

    // 11. diff — compare files (simplified unified diff)
    shell.register_cmd("diff", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 3) { ctx.outln("diff: need two files"); return 1; }
        std::string path1 = resolve_path(ctx.cwd, args[1]);
        std::string path2 = resolve_path(ctx.cwd, args[2]);
        std::string c1 = ctx.fs->read(path1), c2 = ctx.fs->read(path2);

        std::vector<std::string> lines1, lines2;
        { std::istringstream s(c1); std::string l; while (std::getline(s, l)) lines1.push_back(l); }
        { std::istringstream s(c2); std::string l; while (std::getline(s, l)) lines2.push_back(l); }

        if (lines1 == lines2) return 0;

        ctx.outln("--- " + args[1]);
        ctx.outln("+++ " + args[2]);

        size_t max_lines = std::max(lines1.size(), lines2.size());
        for (size_t i = 0; i < max_lines; i++) {
            std::string l1 = (i < lines1.size()) ? lines1[i] : "";
            std::string l2 = (i < lines2.size()) ? lines2[i] : "";
            if (l1 != l2) {
                if (i < lines1.size()) ctx.outln("\033[31m-" + l1 + "\033[0m");
                if (i < lines2.size()) ctx.outln("\033[32m+" + l2 + "\033[0m");
            } else {
                ctx.outln(" " + l1);
            }
        }
        return 1;
    });

    // 12. tee — copy stdin to stdout and file
    shell.register_cmd("tee", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_a = false;
        std::vector<std::string> files;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-a") flag_a = true;
            else files.push_back(args[i]);
        }

        ctx.out(ctx.stdin_data);

        if (ctx.fs) {
            for (auto& f : files) {
                std::string path = resolve_path(ctx.cwd, f);
                if (flag_a) {
                    std::string existing = ctx.fs->read(path);
                    ctx.fs->write(path, existing + ctx.stdin_data);
                } else {
                    ctx.fs->write(path, ctx.stdin_data);
                }
            }
        }
        return 0;
    });

    // 13. paste — merge lines
    shell.register_cmd("paste", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string delim = "\t";
        std::vector<std::vector<std::string>> file_lines;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-d" && i + 1 < args.size()) { delim = args[++i]; continue; }
            if (!ctx.fs) continue;
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            std::vector<std::string> lines;
            std::istringstream ss(content);
            std::string line;
            while (std::getline(ss, line)) lines.push_back(line);
            file_lines.push_back(lines);
        }

        if (file_lines.empty()) { ctx.out(ctx.stdin_data); return 0; }

        size_t max_lines = 0;
        for (auto& fl : file_lines) max_lines = std::max(max_lines, fl.size());

        for (size_t i = 0; i < max_lines; i++) {
            std::string out;
            for (size_t f = 0; f < file_lines.size(); f++) {
                if (f > 0) out += delim;
                if (i < file_lines[f].size()) out += file_lines[f][i];
            }
            ctx.outln(out);
        }
        return 0;
    });

    // 14. rev — reverse lines
    shell.register_cmd("rev", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string input;
        if (args.size() > 1 && ctx.fs) {
            input = ctx.fs->read(resolve_path(ctx.cwd, args[1]));
        } else {
            input = ctx.stdin_data;
        }

        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) {
            std::reverse(line.begin(), line.end());
            ctx.outln(line);
        }
        return 0;
    });

    // 15. fold — wrap lines
    shell.register_cmd("fold", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        int width = 80;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-w" && i + 1 < args.size()) {
                try { width = std::stoi(args[++i]); } catch (...) {}
            } else if (ctx.fs) {
                input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
            }
        }
        if (input.empty()) input = ctx.stdin_data;

        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) {
            for (size_t i = 0; i < line.size(); i += width) {
                ctx.outln(line.substr(i, width));
            }
        }
        return 0;
    });

    // 16. column — columnate
    shell.register_cmd("column", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool flag_t = false;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-t") flag_t = true;
            else if (ctx.fs) input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
        }
        if (input.empty()) input = ctx.stdin_data;

        if (!flag_t) { ctx.out(input); return 0; }

        // Split into rows/cols and align
        std::vector<std::vector<std::string>> rows;
        std::vector<size_t> widths;

        std::istringstream ss(input);
        std::string line;
        while (std::getline(ss, line)) {
            std::vector<std::string> cols;
            std::istringstream ls(line);
            std::string tok;
            while (ls >> tok) cols.push_back(tok);
            for (size_t i = 0; i < cols.size(); i++) {
                if (i >= widths.size()) widths.push_back(0);
                widths[i] = std::max(widths[i], cols[i].size());
            }
            rows.push_back(cols);
        }

        for (auto& row : rows) {
            std::string out;
            for (size_t i = 0; i < row.size(); i++) {
                if (i > 0) out += "  ";
                out += row[i];
                if (i + 1 < row.size()) {
                    int pad = (int)widths[i] - (int)row[i].size();
                    for (int p = 0; p < pad; p++) out += ' ';
                }
            }
            ctx.outln(out);
        }
        return 0;
    });

    // 17. nl — number lines
    shell.register_cmd("nl", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string input;
        if (args.size() > 1 && ctx.fs) {
            input = ctx.fs->read(resolve_path(ctx.cwd, args[1]));
        } else {
            input = ctx.stdin_data;
        }

        std::istringstream ss(input);
        std::string line;
        int n = 1;
        while (std::getline(ss, line)) {
            if (line.empty()) {
                ctx.outln("");
            } else {
                char buf[16]; snprintf(buf, sizeof(buf), "%6d\t", n++);
                ctx.outln(std::string(buf) + line);
            }
        }
        return 0;
    });

    // 18. strings — print printable strings
    shell.register_cmd("strings", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("strings: missing file"); return 1; }
        std::string path = resolve_path(ctx.cwd, args[1]);
        std::string content = ctx.fs->read(path);

        std::string current;
        for (char c : content) {
            if (c >= 32 && c <= 126) {
                current += c;
            } else {
                if (current.size() >= 4) ctx.outln(current);
                current.clear();
            }
        }
        if (current.size() >= 4) ctx.outln(current);
        return 0;
    });

    // 19. base64 — encode/decode base64
    shell.register_cmd("base64", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool decode = false;
        std::string input;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-d" || args[i] == "--decode") decode = true;
            else if (ctx.fs) input = ctx.fs->read(resolve_path(ctx.cwd, args[i]));
        }
        if (input.empty()) input = ctx.stdin_data;
        // Remove trailing newline
        while (!input.empty() && input.back() == '\n') input.pop_back();

        static const char enc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        if (!decode) {
            // Encode
            std::string out;
            size_t i = 0;
            while (i < input.size()) {
                uint32_t a = (uint8_t)input[i++];
                uint32_t b = (i < input.size()) ? (uint8_t)input[i++] : 0;
                uint32_t c = (i < input.size()) ? (uint8_t)input[i++] : 0;
                uint32_t triple = (a << 16) | (b << 8) | c;
                int pad = 3 - (int)(i - (i - (i > input.size() + 1 ? 2 : (i > input.size() ? 1 : 0))));
                (void)pad;

                out += enc[(triple >> 18) & 0x3F];
                out += enc[(triple >> 12) & 0x3F];
                out += (i > input.size() + 1) ? '=' : enc[(triple >> 6) & 0x3F];
                out += (i > input.size()) ? '=' : enc[triple & 0x3F];
            }
            ctx.outln(out);
        } else {
            // Decode
            auto decode_char = [](char c) -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A';
                if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                if (c >= '0' && c <= '9') return c - '0' + 52;
                if (c == '+') return 62;
                if (c == '/') return 63;
                return -1;
            };

            std::string out;
            size_t i = 0;
            while (i + 3 < input.size()) {
                int a = decode_char(input[i++]);
                int b = decode_char(input[i++]);
                int c = decode_char(input[i++]);
                int d = decode_char(input[i++]);
                if (a < 0 || b < 0) break;
                uint32_t triple = (a << 18) | (b << 12) | ((c < 0 ? 0 : c) << 6) | (d < 0 ? 0 : d);
                out += (char)((triple >> 16) & 0xFF);
                if (c >= 0) out += (char)((triple >> 8) & 0xFF);
                if (d >= 0) out += (char)(triple & 0xFF);
            }
            ctx.outln(out);
        }
        return 0;
    });

    // 20. xargs — build commands from stdin
    shell.register_cmd("xargs", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string cmd = "echo";
        std::vector<std::string> extra_args;

        for (size_t i = 1; i < args.size(); i++) {
            if (cmd == "echo" && i == 1) cmd = args[i];
            else extra_args.push_back(args[i]);
        }

        // Split stdin into words
        std::vector<std::string> words;
        std::istringstream ss(ctx.stdin_data);
        std::string word;
        while (ss >> word) words.push_back(word);

        // Build and execute command
        std::vector<std::string> full_args = {cmd};
        full_args.insert(full_args.end(), extra_args.begin(), extra_args.end());
        full_args.insert(full_args.end(), words.begin(), words.end());

        ctx.stdin_data.clear();
        auto it = shell.all_commands().find(cmd);
        if (it != shell.all_commands().end()) {
            return it->second(full_args, ctx);
        }
        ctx.outln("xargs: command not found: " + cmd);
        return 127;
    });
}
