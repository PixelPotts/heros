#pragma once
#include "terminal_shell.h"
#include <cstdio>
#include <cstring>
#include <sstream>

// ── 20 Shell/Environment Commands ───────────────────────────────

inline void register_shell_commands(ShellEngine& shell) {

    // 1. echo — print arguments
    shell.register_cmd("echo", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        bool newline = true;
        bool interpret_escapes = false;
        std::string output;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-n") { newline = false; continue; }
            if (args[i] == "-e") { interpret_escapes = true; continue; }
            if (!output.empty()) output += ' ';

            if (interpret_escapes) {
                for (size_t j = 0; j < args[i].size(); j++) {
                    if (args[i][j] == '\\' && j + 1 < args[i].size()) {
                        j++;
                        switch (args[i][j]) {
                            case 'n': output += '\n'; break;
                            case 't': output += '\t'; break;
                            case '\\': output += '\\'; break;
                            case 'e': output += '\033'; break;
                            default: output += '\\'; output += args[i][j]; break;
                        }
                    } else {
                        output += args[i][j];
                    }
                }
            } else {
                output += args[i];
            }
        }

        ctx.out(output);
        if (newline) ctx.out("\n");
        return 0;
    });

    // 2. printf — formatted print (simplified)
    shell.register_cmd("printf", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("printf: usage: printf FORMAT [ARG...]"); return 1; }
        std::string fmt = args[1];
        size_t ai = 2;
        std::string output;

        for (size_t i = 0; i < fmt.size(); i++) {
            if (fmt[i] == '\\') {
                if (i + 1 < fmt.size()) {
                    i++;
                    switch (fmt[i]) {
                        case 'n': output += '\n'; break;
                        case 't': output += '\t'; break;
                        case '\\': output += '\\'; break;
                        default: output += '\\'; output += fmt[i]; break;
                    }
                }
            } else if (fmt[i] == '%' && i + 1 < fmt.size()) {
                i++;
                std::string arg = (ai < args.size()) ? args[ai++] : "";
                switch (fmt[i]) {
                    case 's': output += arg; break;
                    case 'd': {
                        try { output += std::to_string(std::stoi(arg)); }
                        catch (...) { output += "0"; }
                        break;
                    }
                    case '%': output += '%'; break;
                    default: output += '%'; output += fmt[i]; break;
                }
            } else {
                output += fmt[i];
            }
        }

        ctx.out(output);
        return 0;
    });

    // 3. env — print environment
    shell.register_cmd("env", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        for (auto& [k, v] : shell.env()) {
            ctx.outln(k + "=" + v);
        }
        return 0;
    });

    // 4. export — set env variable
    shell.register_cmd("export", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) {
            for (auto& [k, v] : shell.env()) ctx.outln("export " + k + "=\"" + v + "\"");
            return 0;
        }
        for (size_t i = 1; i < args.size(); i++) {
            auto eq = args[i].find('=');
            if (eq != std::string::npos) {
                shell.env()[args[i].substr(0, eq)] = args[i].substr(eq + 1);
            }
        }
        return 0;
    });

    // 5. unset — remove env variable
    shell.register_cmd("unset", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)ctx;
        for (size_t i = 1; i < args.size(); i++) {
            shell.env().erase(args[i]);
        }
        return 0;
    });

    // 6. alias — create alias
    shell.register_cmd("alias", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) {
            for (auto& [k, v] : shell.aliases()) ctx.outln("alias " + k + "='" + v + "'");
            return 0;
        }
        for (size_t i = 1; i < args.size(); i++) {
            auto eq = args[i].find('=');
            if (eq != std::string::npos) {
                shell.aliases()[args[i].substr(0, eq)] = args[i].substr(eq + 1);
            }
        }
        return 0;
    });

    // 7. unalias — remove alias
    shell.register_cmd("unalias", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)ctx;
        for (size_t i = 1; i < args.size(); i++) {
            shell.aliases().erase(args[i]);
        }
        return 0;
    });

    // 8. history — show command history (placeholder, real history managed by TerminalApp)
    shell.register_cmd("history", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("(history is managed by the terminal UI)");
        ctx.outln("Use Up/Down arrow keys to navigate history.");
        return 0;
    });

    // 9. which — show command location
    shell.register_cmd("which", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("which: usage: which COMMAND"); return 1; }
        for (size_t i = 1; i < args.size(); i++) {
            if (shell.has_cmd(args[i])) {
                ctx.outln("/bin/" + args[i]);
            } else {
                ctx.outln("which: no " + args[i] + " in PATH");
            }
        }
        return 0;
    });

    // 10. type — describe command type
    shell.register_cmd("type", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("type: usage: type COMMAND"); return 1; }
        for (size_t i = 1; i < args.size(); i++) {
            if (shell.aliases().count(args[i])) {
                ctx.outln(args[i] + " is aliased to '" + shell.aliases()[args[i]] + "'");
            } else if (shell.has_cmd(args[i])) {
                ctx.outln(args[i] + " is a shell builtin");
            } else {
                ctx.outln("herosh: type: " + args[i] + ": not found");
            }
        }
        return 0;
    });

    // 11. source — execute file as commands (simplified)
    shell.register_cmd("source", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("source: usage: source FILE"); return 1; }
        if (!ctx.fs) return 1;
        std::string path = resolve_path(ctx.cwd, args[1]);
        std::string content = ctx.fs->read(path);
        if (content.empty()) { ctx.outln("source: " + args[1] + ": No such file"); return 1; }
        // Execute line by line
        std::istringstream ss(content);
        std::string line;
        int status = 0;
        while (std::getline(ss, line)) {
            if (line.empty() || line[0] == '#') continue;
            status = shell.execute(line, ctx);
        }
        return status;
    });

    // 12. read — read a line from stdin into variable
    shell.register_cmd("read", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string varname = (args.size() >= 2) ? args[1] : "REPLY";
        // Read from stdin_data (first line)
        std::string line;
        auto nl = ctx.stdin_data.find('\n');
        if (nl != std::string::npos) {
            line = ctx.stdin_data.substr(0, nl);
        } else {
            line = ctx.stdin_data;
        }
        shell.env()[varname] = line;
        return 0;
    });

    // 13. test / [ — conditional expression
    auto test_fn = [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)ctx;
        // Remove trailing ] if present
        auto a = args;
        if (!a.empty() && a.back() == "]") a.pop_back();
        if (a.size() < 2) return 1;

        // Unary: -z STRING, -n STRING, -e FILE, -f FILE, -d FILE
        if (a.size() == 3) {
            if (a[1] == "-z") return a[2].empty() ? 0 : 1;
            if (a[1] == "-n") return a[2].empty() ? 1 : 0;
            if ((a[1] == "-e" || a[1] == "-f" || a[1] == "-d") && ctx.fs) {
                std::string p = resolve_path(ctx.cwd, a[2]);
                return ctx.fs->exists(p) ? 0 : 1;
            }
        }

        // Binary: STRING = STRING, STRING != STRING, INT -eq/-ne/-lt/-gt/-le/-ge INT
        if (a.size() == 4) {
            if (a[2] == "=" || a[2] == "==") return (a[1] == a[3]) ? 0 : 1;
            if (a[2] == "!=") return (a[1] != a[3]) ? 0 : 1;
            try {
                int l = std::stoi(a[1]), r = std::stoi(a[3]);
                if (a[2] == "-eq") return l == r ? 0 : 1;
                if (a[2] == "-ne") return l != r ? 0 : 1;
                if (a[2] == "-lt") return l < r ? 0 : 1;
                if (a[2] == "-gt") return l > r ? 0 : 1;
                if (a[2] == "-le") return l <= r ? 0 : 1;
                if (a[2] == "-ge") return l >= r ? 0 : 1;
            } catch (...) {}
        }
        return 1;
    };
    shell.register_cmd("test", test_fn);
    shell.register_cmd("[", test_fn);

    // 14. true
    shell.register_cmd("true", [](std::vector<std::string>&, CmdContext&) -> int { return 0; });

    // 15. false
    shell.register_cmd("false", [](std::vector<std::string>&, CmdContext&) -> int { return 1; });

    // 16. exit — close terminal
    shell.register_cmd("exit", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("exit");
        // The terminal app will check for this and close
        return -999; // sentinel for exit
    });

    // 17. clear — clear screen (sentinel)
    shell.register_cmd("clear", [](std::vector<std::string>&, CmdContext&) -> int {
        return -998; // sentinel for clear
    });

    // 18. set — show all variables
    shell.register_cmd("set", [&shell](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        std::vector<std::string> keys;
        for (auto& [k, v] : shell.env()) keys.push_back(k);
        std::sort(keys.begin(), keys.end());
        for (auto& k : keys) {
            ctx.outln(k + "=" + shell.env()[k]);
        }
        return 0;
    });

    // 19. yes — output string repeatedly (capped at 100 lines)
    shell.register_cmd("yes", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        std::string word = "y";
        if (args.size() >= 2) word = args[1];
        for (int i = 0; i < 100; i++) ctx.outln(word);
        return 0;
    });

    // 20. seq — print sequence of numbers
    shell.register_cmd("seq", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("seq: usage: seq [FIRST [INCREMENT]] LAST"); return 1; }

        int first = 1, inc = 1, last = 1;
        try {
            if (args.size() == 2) {
                last = std::stoi(args[1]);
            } else if (args.size() == 3) {
                first = std::stoi(args[1]);
                last = std::stoi(args[2]);
            } else {
                first = std::stoi(args[1]);
                inc = std::stoi(args[2]);
                last = std::stoi(args[3]);
            }
        } catch (...) { ctx.outln("seq: invalid number"); return 1; }

        if (inc == 0) { ctx.outln("seq: increment must not be 0"); return 1; }

        int count = 0;
        for (int i = first; (inc > 0 ? i <= last : i >= last) && count < 10000; i += inc, count++) {
            ctx.outln(std::to_string(i));
        }
        return 0;
    });
}
