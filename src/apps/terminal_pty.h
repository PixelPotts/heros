#pragma once
// ── PTY Backend ─────────────────────────────────────────────────
// Manages a pseudo-terminal: fork → exec /bin/bash, non-blocking
// I/O on the master fd, window-size updates, child reaping.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <functional>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>          // openpty()
#include <termios.h>

class PtyBackend {
public:
    PtyBackend() = default;
    ~PtyBackend() { shutdown(); }

    // No copy
    PtyBackend(const PtyBackend&) = delete;
    PtyBackend& operator=(const PtyBackend&) = delete;

    // ── Start the shell ──────────────────────────────────────────
    bool start(int cols, int rows) {
        struct winsize ws{};
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;

        int master_fd = -1, slave_fd = -1;
        if (openpty(&master_fd, &slave_fd, nullptr, nullptr, &ws) < 0) {
            perror("PtyBackend: openpty");
            return false;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("PtyBackend: fork");
            close(master_fd);
            close(slave_fd);
            return false;
        }

        if (pid == 0) {
            // ── Child process ────────────────────────────────────
            close(master_fd);
            setsid();

            // Make slave the controlling terminal
            if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
                perror("TIOCSCTTY");
            }

            dup2(slave_fd, STDIN_FILENO);
            dup2(slave_fd, STDOUT_FILENO);
            dup2(slave_fd, STDERR_FILENO);
            if (slave_fd > STDERR_FILENO) close(slave_fd);

            // Set TERM so apps know we support color
            setenv("TERM", "xterm-256color", 1);
            setenv("COLORTERM", "truecolor", 1);

            // Exec login shell
            const char* shell = getenv("SHELL");
            if (!shell || !shell[0]) shell = "/bin/bash";
            execlp(shell, shell, "-l", nullptr);
            perror("execlp");
            _exit(127);
        }

        // ── Parent process ───────────────────────────────────────
        close(slave_fd);
        master_fd_ = master_fd;
        child_pid_ = pid;

        // Non-blocking reads
        int flags = fcntl(master_fd_, F_GETFL);
        fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);

        alive_ = true;
        return true;
    }

    // ── Read available data (non-blocking) ───────────────────────
    // Returns number of bytes read, 0 if nothing, -1 on error/EOF.
    int read_some(char* buf, int bufsize) {
        if (master_fd_ < 0) return -1;
        ssize_t n = ::read(master_fd_, buf, (size_t)bufsize);
        if (n > 0) return (int)n;
        if (n == 0) { alive_ = false; return -1; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        alive_ = false;
        return -1;
    }

    // ── Write bytes to the shell ─────────────────────────────────
    bool write_bytes(const char* data, int len) {
        if (master_fd_ < 0) return false;
        int written = 0;
        while (written < len) {
            ssize_t n = ::write(master_fd_, data + written, (size_t)(len - written));
            if (n <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                return false;
            }
            written += (int)n;
        }
        return true;
    }

    bool write_str(const std::string& s) {
        return write_bytes(s.data(), (int)s.size());
    }

    // ── Resize the PTY ──────────────────────────────────────────
    void set_size(int cols, int rows) {
        if (master_fd_ < 0) return;
        struct winsize ws{};
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;
        ioctl(master_fd_, TIOCSWINSZ, &ws);
        // Signal the child about the size change
        if (child_pid_ > 0) kill(child_pid_, SIGWINCH);
    }

    // ── Check if child is still running ─────────────────────────
    bool is_alive() {
        if (!alive_) return false;
        if (child_pid_ <= 0) { alive_ = false; return false; }
        int status = 0;
        pid_t r = waitpid(child_pid_, &status, WNOHANG);
        if (r == child_pid_) {
            alive_ = false;
            child_pid_ = -1;
        }
        return alive_;
    }

    // ── Shutdown ────────────────────────────────────────────────
    void shutdown() {
        if (master_fd_ >= 0) {
            close(master_fd_);
            master_fd_ = -1;
        }
        if (child_pid_ > 0) {
            kill(child_pid_, SIGHUP);
            int status;
            waitpid(child_pid_, &status, 0);
            child_pid_ = -1;
        }
        alive_ = false;
    }

    int master_fd() const { return master_fd_; }
    pid_t child_pid() const { return child_pid_; }

private:
    int master_fd_ = -1;
    pid_t child_pid_ = -1;
    bool alive_ = false;
};
