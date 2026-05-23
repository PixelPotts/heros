#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <algorithm>

// ── Forward declarations ────────────────────────────────────────

class FileSystem;
class ProcessManager;
class AppRegistry;
class WindowManager;
class NotificationManager;
class EventBus;
class ThemeManager;

// ── Command context — everything a command needs ────────────────

struct CmdContext {
    FileSystem* fs = nullptr;
    ProcessManager* pm = nullptr;
    AppRegistry* registry = nullptr;
    WindowManager* wm = nullptr;
    NotificationManager* notifications = nullptr;
    EventBus* bus = nullptr;
    int screen_w = 0, screen_h = 0;
    int window_id = 0;

    std::string& cwd;       // reference to shell's cwd
    std::unordered_map<std::string, std::string>& env;  // reference to shell's env

    std::string stdin_data;  // input from pipe
    std::string stdout_data; // output (command appends here)

    // Convenience: append to stdout
    void out(const std::string& s) { stdout_data += s; }
    void outln(const std::string& s) { stdout_data += s + "\n"; }
};

// ── Command function signature ──────────────────────────────────

using CmdFunc = std::function<int(std::vector<std::string>& args, CmdContext& ctx)>;

// ── Path resolution utility ─────────────────────────────────────

inline std::string resolve_path(const std::string& cwd, const std::string& path) {
    if (path.empty()) return cwd;

    std::string working;
    if (path[0] == '/') {
        working = path;
    } else {
        working = cwd;
        if (working.back() != '/') working += '/';
        working += path;
    }

    // Normalize: split on '/', resolve . and ..
    std::vector<std::string> parts;
    std::istringstream ss(working);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += "/";
        result += parts[i];
    }
    return result;
}

// ── Token types for the parser ──────────────────────────────────

enum class TokenType {
    Word,       // a plain word or quoted string
    Pipe,       // |
    RedirectOut,    // >
    RedirectAppend, // >>
    Semicolon,  // ;
    And,        // &&
    Or,         // ||
};

struct Token {
    TokenType type;
    std::string value;
};

// ── Simple command (one segment of a pipeline) ──────────────────

struct SimpleCommand {
    std::vector<std::string> args;       // command + arguments
    std::string redirect_out;            // > filename
    bool redirect_append = false;        // >> vs >
    std::string redirect_in;             // < filename (stub)
};

// ── Pipeline (commands connected by |) ──────────────────────────

struct Pipeline {
    std::vector<SimpleCommand> commands;
};

// ── Shell Engine ────────────────────────────────────────────────

class ShellEngine {
public:
    ShellEngine() {
        cwd_ = "/";
        env_["HOME"] = "/home";
        env_["USER"] = "hero";
        env_["SHELL"] = "/bin/herosh";
        env_["PATH"] = "/bin:/usr/bin";
        env_["TERM"] = "heros-256color";
        env_["HOSTNAME"] = "heros";
        env_["PS1"] = "\\u@\\h:\\w$ ";
    }

    // Register a command
    void register_cmd(const std::string& name, CmdFunc func) {
        commands_[name] = std::move(func);
    }

    // Check if command exists
    bool has_cmd(const std::string& name) const {
        if (commands_.count(name)) return true;
        if (aliases_.count(name)) return true;
        return false;
    }

    // Execute a full command line (handles pipes, redirects, semicolons)
    int execute(const std::string& line, CmdContext& ctx) {
        // Variable expansion
        std::string expanded = expand_variables(line);

        // Tokenize
        auto tokens = tokenize(expanded);
        if (tokens.empty()) return 0;

        // Split by semicolons into separate pipelines
        std::vector<std::vector<Token>> segments;
        segments.push_back({});
        for (auto& tok : tokens) {
            if (tok.type == TokenType::Semicolon) {
                segments.push_back({});
            } else {
                segments.back().push_back(tok);
            }
        }

        int last_status = 0;
        for (auto& seg : segments) {
            if (seg.empty()) continue;
            auto pipeline = parse_pipeline(seg);
            last_status = execute_pipeline(pipeline, ctx);
        }
        return last_status;
    }

    // Accessors
    std::string& cwd() { return cwd_; }
    const std::string& cwd() const { return cwd_; }
    std::unordered_map<std::string, std::string>& env() { return env_; }
    std::unordered_map<std::string, std::string>& aliases() { return aliases_; }
    const std::unordered_map<std::string, CmdFunc>& all_commands() const { return commands_; }

private:
    std::string cwd_;
    std::unordered_map<std::string, std::string> env_;
    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<std::string, CmdFunc> commands_;

    // ── Tokenizer (quote-aware) ─────────────────────────────────

    std::vector<Token> tokenize(const std::string& input) {
        std::vector<Token> tokens;
        size_t i = 0;
        size_t len = input.size();

        while (i < len) {
            // Skip whitespace
            while (i < len && (input[i] == ' ' || input[i] == '\t')) i++;
            if (i >= len) break;

            char c = input[i];

            // Pipe
            if (c == '|') {
                if (i + 1 < len && input[i + 1] == '|') {
                    tokens.push_back({TokenType::Or, "||"});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Pipe, "|"});
                    i++;
                }
                continue;
            }

            // Redirect
            if (c == '>') {
                if (i + 1 < len && input[i + 1] == '>') {
                    tokens.push_back({TokenType::RedirectAppend, ">>"});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::RedirectOut, ">"});
                    i++;
                }
                continue;
            }

            // Semicolon
            if (c == ';') {
                tokens.push_back({TokenType::Semicolon, ";"});
                i++;
                continue;
            }

            // And
            if (c == '&' && i + 1 < len && input[i + 1] == '&') {
                tokens.push_back({TokenType::And, "&&"});
                i += 2;
                continue;
            }

            // Quoted string
            if (c == '"' || c == '\'') {
                char quote = c;
                i++;
                std::string word;
                while (i < len && input[i] != quote) {
                    if (input[i] == '\\' && i + 1 < len && quote == '"') {
                        i++;
                        switch (input[i]) {
                            case 'n': word += '\n'; break;
                            case 't': word += '\t'; break;
                            case '\\': word += '\\'; break;
                            case '"': word += '"'; break;
                            default: word += '\\'; word += input[i]; break;
                        }
                    } else {
                        word += input[i];
                    }
                    i++;
                }
                if (i < len) i++; // skip closing quote
                tokens.push_back({TokenType::Word, word});
                continue;
            }

            // Regular word
            std::string word;
            while (i < len && input[i] != ' ' && input[i] != '\t'
                   && input[i] != '|' && input[i] != '>' && input[i] != ';'
                   && input[i] != '&' && input[i] != '<') {
                if (input[i] == '\\' && i + 1 < len) {
                    i++;
                    word += input[i];
                } else {
                    word += input[i];
                }
                i++;
            }
            if (!word.empty()) {
                tokens.push_back({TokenType::Word, word});
            }
        }

        return tokens;
    }

    // ── Variable expansion ──────────────────────────────────────

    std::string expand_variables(const std::string& input) {
        std::string result;
        result.reserve(input.size());
        size_t i = 0;
        bool in_single_quote = false;

        while (i < input.size()) {
            if (input[i] == '\'') {
                in_single_quote = !in_single_quote;
                result += input[i++];
                continue;
            }

            if (in_single_quote) {
                result += input[i++];
                continue;
            }

            if (input[i] == '$' && i + 1 < input.size()) {
                i++;
                if (input[i] == '?') {
                    result += "0"; // last exit code - simplified
                    i++;
                } else if (input[i] == '{') {
                    i++;
                    std::string var;
                    while (i < input.size() && input[i] != '}') var += input[i++];
                    if (i < input.size()) i++;
                    auto it = env_.find(var);
                    if (it != env_.end()) result += it->second;
                } else {
                    std::string var;
                    while (i < input.size() && (isalnum(input[i]) || input[i] == '_'))
                        var += input[i++];
                    auto it = env_.find(var);
                    if (it != env_.end()) result += it->second;
                }
                continue;
            }

            if (input[i] == '~' && (i == 0 || input[i-1] == ' ' || input[i-1] == '=')) {
                auto it = env_.find("HOME");
                result += (it != env_.end()) ? it->second : "/home";
                i++;
                continue;
            }

            result += input[i++];
        }

        return result;
    }

    // ── Parse tokens into a pipeline ────────────────────────────

    Pipeline parse_pipeline(const std::vector<Token>& tokens) {
        Pipeline pipeline;
        SimpleCommand current;

        for (size_t i = 0; i < tokens.size(); i++) {
            auto& tok = tokens[i];

            if (tok.type == TokenType::Pipe) {
                pipeline.commands.push_back(std::move(current));
                current = SimpleCommand{};
            } else if (tok.type == TokenType::RedirectOut || tok.type == TokenType::RedirectAppend) {
                if (i + 1 < tokens.size() && tokens[i + 1].type == TokenType::Word) {
                    current.redirect_out = tokens[i + 1].value;
                    current.redirect_append = (tok.type == TokenType::RedirectAppend);
                    i++;
                }
            } else if (tok.type == TokenType::Word) {
                current.args.push_back(tok.value);
            }
        }

        if (!current.args.empty() || !current.redirect_out.empty()) {
            pipeline.commands.push_back(std::move(current));
        }

        return pipeline;
    }

    // ── Execute a pipeline ──────────────────────────────────────

    int execute_pipeline(Pipeline& pipeline, CmdContext& ctx) {
        if (pipeline.commands.empty()) return 0;

        std::string pipe_data = ctx.stdin_data;
        int last_status = 0;

        for (size_t i = 0; i < pipeline.commands.size(); i++) {
            auto& cmd = pipeline.commands[i];
            if (cmd.args.empty()) continue;

            // Resolve aliases
            std::string cmd_name = cmd.args[0];
            int alias_depth = 0;
            while (aliases_.count(cmd_name) && alias_depth < 10) {
                std::string alias_val = aliases_[cmd_name];
                // Re-tokenize the alias + remaining args
                auto alias_tokens = tokenize(alias_val);
                std::vector<std::string> new_args;
                for (auto& t : alias_tokens) {
                    if (t.type == TokenType::Word) new_args.push_back(t.value);
                }
                for (size_t j = 1; j < cmd.args.size(); j++) {
                    new_args.push_back(cmd.args[j]);
                }
                cmd.args = new_args;
                cmd_name = cmd.args.empty() ? "" : cmd.args[0];
                alias_depth++;
            }

            if (cmd_name.empty()) continue;

            // Set up context for this command
            ctx.stdin_data = pipe_data;
            ctx.stdout_data.clear();

            auto it = commands_.find(cmd_name);
            if (it != commands_.end()) {
                last_status = it->second(cmd.args, ctx);
            } else {
                ctx.outln("herosh: command not found: " + cmd_name);
                last_status = 127;
            }

            // Handle redirect
            if (!cmd.redirect_out.empty() && ctx.fs) {
                std::string rpath = resolve_path(cwd_, cmd.redirect_out);
                if (cmd.redirect_append) {
                    std::string existing = ctx.fs->read(rpath);
                    ctx.fs->write(rpath, existing + ctx.stdout_data);
                } else {
                    ctx.fs->write(rpath, ctx.stdout_data);
                }
                ctx.stdout_data.clear();
            }

            // Pass stdout to next command's stdin
            pipe_data = ctx.stdout_data;
        }

        // Final output is in ctx.stdout_data (= pipe_data)
        ctx.stdout_data = pipe_data;
        return last_status;
    }
};
