// Test harness for all 100 HerOS Terminal commands
// Compiles against core .o files and runs each command, checking for crashes
// and expected behavior.

// Include real OS headers FIRST (before terminal headers that forward-declare)
#include "../../src/vfs.h"
#include "../../src/process.h"
#include "../../src/event_bus.h"
#include "../../src/app_registry.h"
#include "../../src/window.h"

// Now include terminal headers (their forward declarations are already resolved)
#include "../../src/apps/terminal_shell.h"
#include "../../src/apps/terminal_cmds_shell.h"
#include "../../src/apps/terminal_cmds_file.h"
#include "../../src/apps/terminal_cmds_text.h"
#include "../../src/apps/terminal_cmds_system.h"
#include "../../src/apps/terminal_cmds_net.h"
#include "../../src/apps/terminal_cmds_archive.h"
#include "../../src/apps/terminal_cmds_heros.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

// ── Test infrastructure ─────────────────────────────────────────

struct TestResult {
    std::string name;
    bool passed;
    std::string detail;
};

static std::vector<TestResult> results;
static int pass_count = 0;
static int fail_count = 0;

// Colors
#define RED    "\033[1;31m"
#define GREEN  "\033[1;32m"
#define YELLOW "\033[1;33m"
#define CYAN   "\033[1;36m"
#define RESET  "\033[0m"

static void record(const std::string& name, bool pass, const std::string& detail = "") {
    results.push_back({name, pass, detail});
    if (pass) {
        pass_count++;
        printf("  " GREEN "PASS" RESET "  %-20s", name.c_str());
    } else {
        fail_count++;
        printf("  " RED "FAIL" RESET "  %-20s", name.c_str());
    }
    if (!detail.empty()) printf("  %s", detail.c_str());
    printf("\n");
}

// ── Run a command and capture output ────────────────────────────

static int run_cmd(ShellEngine& shell, FileSystem& fs, ProcessManager& pm,
                    AppRegistry& registry, WindowManager& wm,
                    NotificationManager& notif, EventBus& bus,
                    const std::string& cmdline,
                    std::string& output, const std::string& stdin_data = "") {
    CmdContext ctx{
        &fs, &pm, &registry, &wm, &notif, &bus,
        1280, 720, 0,
        shell.cwd(), shell.env(),
        stdin_data, ""
    };
    int status = shell.execute(cmdline, ctx);
    output = ctx.stdout_data;
    return status;
}

// Shorthand
#define RUN(cmd) run_cmd(shell, fs, pm, registry, wm, notif, bus, cmd, output)
#define RUN_STDIN(cmd, in) run_cmd(shell, fs, pm, registry, wm, notif, bus, cmd, output, in)

// Check output contains substring
static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ── Main ────────────────────────────────────────────────────────

int main() {
    printf("\n" CYAN "═══════════════════════════════════════════════════════\n");
    printf("  HerOS Terminal — Testing All 100 Commands\n");
    printf("═══════════════════════════════════════════════════════" RESET "\n\n");

    // Set up OS services
    FileSystem fs;
    ProcessManager pm;
    EventBus bus;
    Clipboard clipboard;
    NotificationManager notif;
    AppRegistry registry;
    WindowManager wm;

    registry.set_system(&pm, &fs, nullptr, &bus, &clipboard, &notif);

    // Create shell and register all commands
    ShellEngine shell;
    register_shell_commands(shell);
    register_file_commands(shell);
    register_text_commands(shell);
    register_system_commands(shell);
    register_net_commands(shell);
    register_archive_commands(shell);
    register_heros_commands(shell);

    std::string output;

    // ── Prepare VFS test area ───────────────────────────────────
    fs.mkdir("/test");
    fs.write("/test/hello.txt", "Hello World\nfoo bar baz\nHello Again\n");
    fs.write("/test/numbers.txt", "3\n1\n4\n1\n5\n9\n2\n6\n");
    fs.write("/test/data.csv", "name,age,city\nalice,30,NYC\nbob,25,LA\ncharlie,35,CHI\n");
    fs.mkdir("/test/subdir");
    fs.write("/test/subdir/nested.txt", "nested content\n");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "── Shell & Environment (20) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 1. echo
    RUN("echo hello world");
    record("echo", output == "hello world\n", "");

    // 2. printf
    RUN("printf 'name: %s age: %d\\n' Alice 30");
    record("printf", contains(output, "name: Alice age: 30"), "");

    // 3. env
    RUN("env");
    record("env", contains(output, "USER=") || contains(output, "SHELL="), "");

    // 4. export
    RUN("export TESTVAR=hello123");
    RUN("echo $TESTVAR");
    record("export", contains(output, "hello123"), "");

    // 5. unset
    RUN("unset TESTVAR");
    RUN("echo $TESTVAR");
    record("unset", !contains(output, "hello123"), "");

    // 6. alias
    RUN("alias greet=echo");
    RUN("greet hi");
    record("alias", contains(output, "hi"), "");

    // 7. unalias
    RUN("unalias greet");
    RUN("greet test 2>/dev/null");
    record("unalias", contains(output, "not found") || !contains(output, "test"), "");

    // 8. history
    RUN("history");
    record("history", contains(output, "history"), "");

    // 9. which
    RUN("which echo");
    record("which", contains(output, "/bin/echo"), "");

    // 10. type
    RUN("type echo");
    record("type", contains(output, "builtin"), "");

    // 11. source
    fs.write("/test/script.sh", "export SOURCED=yes\n");
    RUN("source /test/script.sh");
    RUN("echo $SOURCED");
    record("source", contains(output, "yes"), "");

    // 12. read
    RUN_STDIN("read MYVAR", "test_input\n");
    RUN("echo $MYVAR");
    record("read", contains(output, "test_input"), "");

    // 13. test / [
    {
        int s = RUN("test 1 -eq 1");
        record("test", s == 0, "");
    }

    // 14. true
    {
        int s = RUN("true");
        record("true", s == 0, "");
    }

    // 15. false
    {
        int s = RUN("false");
        record("false", s == 1, "");
    }

    // 16. exit
    {
        int s = RUN("exit");
        record("exit", s == -999, "sentinel");
    }

    // 17. clear
    {
        int s = RUN("clear");
        record("clear", s == -998, "sentinel");
    }

    // 18. set
    RUN("set");
    record("set", contains(output, "USER=") || contains(output, "SHELL="), "");

    // 19. yes
    RUN("yes hello");
    {
        int lines = 0;
        for (char c : output) if (c == '\n') lines++;
        record("yes", lines == 100 && contains(output, "hello"), std::to_string(lines) + " lines");
    }

    // 20. seq
    RUN("seq 1 5");
    record("seq", contains(output, "1\n2\n3\n4\n5\n"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── File Operations (20) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 21. ls
    RUN("ls /test");
    record("ls", contains(output, "hello.txt"), "");

    // 22. cd + pwd
    RUN("cd /test");
    RUN("pwd");
    record("cd", contains(output, "/test"), "");

    // 23. pwd (already tested above)
    RUN("pwd");
    record("pwd", contains(output, "/test"), "");

    // 24. cat
    RUN("cat /test/hello.txt");
    record("cat", contains(output, "Hello World"), "");

    // 25. cp
    RUN("cp /test/hello.txt /test/hello_copy.txt");
    RUN("cat /test/hello_copy.txt");
    record("cp", contains(output, "Hello World"), "");

    // 26. mv
    RUN("mv /test/hello_copy.txt /test/hello_moved.txt");
    RUN("cat /test/hello_moved.txt");
    record("mv", contains(output, "Hello World"), "");

    // 27. rm
    RUN("rm /test/hello_moved.txt");
    {
        int s = RUN("cat /test/hello_moved.txt");
        record("rm", s != 0 || contains(output, "No such file"), "");
    }

    // 28. mkdir
    RUN("mkdir /test/newdir");
    RUN("ls /test");
    record("mkdir", contains(output, "newdir"), "");

    // 29. rmdir
    RUN("rmdir /test/newdir");
    RUN("ls /test");
    record("rmdir", !contains(output, "newdir"), "");

    // 30. touch
    RUN("touch /test/touched.txt");
    RUN("ls /test");
    record("touch", contains(output, "touched.txt"), "");

    // 31. find
    RUN("find /test -name *.txt");
    record("find", contains(output, "hello.txt") && contains(output, "nested.txt"), "");

    // 32. ln (stub)
    {
        int s = RUN("ln -s /test/a /test/b");
        record("ln", contains(output, "not supported"), "stub");
    }

    // 33. chmod (stub)
    RUN("chmod 755 /test/hello.txt");
    record("chmod", contains(output, "simulated"), "stub");

    // 34. chown (stub)
    RUN("chown hero /test/hello.txt");
    record("chown", contains(output, "simulated"), "stub");

    // 35. stat
    RUN("stat /test/hello.txt");
    record("stat", contains(output, "hello.txt") && contains(output, "Size:"), "");

    // 36. file
    RUN("file /test/hello.txt");
    record("file", contains(output, "text") || contains(output, "hello.txt"), "");

    // 37. tree
    RUN("tree /test");
    record("tree", contains(output, "├") || contains(output, "└") || contains(output, "files"), "");

    // 38. du
    RUN("du -h /test");
    record("du", output.size() > 0, "has output");

    // 39. df
    RUN("df -h");
    record("df", contains(output, "heros-vfs") && contains(output, "2.0G"), "");

    // 40. realpath
    RUN("realpath ../test/hello.txt");
    record("realpath", contains(output, "/test/hello.txt"), "");

    // Reset cwd
    RUN("cd /");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── Text Processing (20) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 41. grep
    RUN("grep Hello /test/hello.txt");
    record("grep", contains(output, "Hello World") && contains(output, "Hello Again"), "");

    // 42. sed
    RUN_STDIN("sed s/foo/bar/g", "foo baz foo\n");
    record("sed", contains(output, "bar baz bar"), "");

    // 43. awk
    RUN_STDIN("awk '{print $2}'", "one two three\nfour five six\n");
    record("awk", contains(output, "two") && contains(output, "five"), "");

    // 44. sort
    RUN("sort /test/numbers.txt");
    {
        // First line should be 1
        record("sort", output.size() > 0 && output[0] == '1', "");
    }

    // 45. uniq
    RUN_STDIN("uniq", "a\na\nb\nb\nb\nc\n");
    record("uniq", output == "a\nb\nc\n", "");

    // 46. wc
    RUN("wc /test/hello.txt");
    record("wc", contains(output, "3") && contains(output, "hello.txt"), "lines/words/bytes");

    // 47. head
    RUN("head -n 2 /test/numbers.txt");
    record("head", contains(output, "3\n1\n") && !contains(output, "4"), "");

    // 48. tail
    RUN("tail -n 2 /test/numbers.txt");
    record("tail", contains(output, "2\n6\n"), "");

    // 49. cut
    RUN("cut -d , -f 1,2 /test/data.csv");
    record("cut", contains(output, "name,age") && contains(output, "alice,30"), "");

    // 50. tr
    RUN_STDIN("tr a-z A-Z", "hello world");
    record("tr", contains(output, "HELLO WORLD"), "");

    // 51. diff
    fs.write("/test/diff1.txt", "line1\nline2\nline3\n");
    fs.write("/test/diff2.txt", "line1\nchanged\nline3\n");
    RUN("diff /test/diff1.txt /test/diff2.txt");
    record("diff", contains(output, "-line2") || contains(output, "+changed"), "");

    // 52. tee
    RUN_STDIN("tee /test/tee_out.txt", "tee test data\n");
    RUN("cat /test/tee_out.txt");
    record("tee", contains(output, "tee test data"), "");

    // 53. paste
    fs.write("/test/col1.txt", "a\nb\nc\n");
    fs.write("/test/col2.txt", "1\n2\n3\n");
    RUN("paste /test/col1.txt /test/col2.txt");
    record("paste", contains(output, "a\t1"), "");

    // 54. rev
    RUN_STDIN("rev", "hello\nworld\n");
    record("rev", contains(output, "olleh") && contains(output, "dlrow"), "");

    // 55. fold
    RUN_STDIN("fold -w 5", "abcdefghij\n");
    record("fold", contains(output, "abcde\nfghij"), "");

    // 56. column
    RUN_STDIN("column -t", "name age city\nalice 30 NYC\nbob 25 LA\n");
    record("column", contains(output, "name") && contains(output, "alice"), "");

    // 57. nl
    RUN_STDIN("nl", "first\nsecond\nthird\n");
    record("nl", contains(output, "1\tfirst") && contains(output, "2\tsecond"), "");

    // 58. strings
    fs.write("/test/binary.dat", std::string("junk\x01\x02\x03helloworld\x00more\x01testdata\x04", 34));
    RUN("strings /test/binary.dat");
    record("strings", contains(output, "helloworld") || contains(output, "testdata"), "");

    // 59. base64
    RUN_STDIN("base64", "Hello");
    {
        bool enc_ok = contains(output, "SGVsbG8");
        std::string encoded = output;
        // strip newline
        while (!encoded.empty() && encoded.back() == '\n') encoded.pop_back();
        RUN_STDIN("base64 -d", encoded);
        record("base64", enc_ok && contains(output, "Hello"), "encode+decode");
    }

    // 60. xargs
    RUN_STDIN("xargs echo", "a b c\nd e f\n");
    record("xargs", contains(output, "a") && contains(output, "f"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── System (15) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 61. ps
    RUN("ps");
    record("ps", contains(output, "PID") && contains(output, "STATE"), "");

    // 62. kill (no process to kill, just test it doesn't crash)
    {
        int s = RUN("kill 99999");
        record("kill", contains(output, "no such process"), "");
    }

    // 63. top
    RUN("top");
    record("top", contains(output, "Uptime") && contains(output, "PID"), "");

    // 64. uptime
    RUN("uptime");
    record("uptime", contains(output, "up") && contains(output, "process"), "");

    // 65. whoami
    RUN("whoami");
    record("whoami", contains(output, "hero"), "");

    // 66. hostname
    RUN("hostname");
    record("hostname", contains(output, "heros"), "");

    // 67. uname
    RUN("uname -a");
    record("uname", contains(output, "HerOS") && contains(output, "x86_64"), "");

    // 68. date
    RUN("date");
    record("date", output.size() > 5, "has output");

    // 69. cal
    RUN("cal");
    record("cal", contains(output, "Su Mo Tu We Th Fr Sa"), "");

    // 70. time
    RUN("time echo test");
    record("time", contains(output, "real") && contains(output, "test"), "");

    // 71. sleep (very short)
    {
        auto start = std::chrono::steady_clock::now();
        RUN("sleep 0.1");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        record("sleep", elapsed >= 50 && elapsed < 2000, std::to_string(elapsed) + "ms");
    }

    // 72. id
    RUN("id");
    record("id", contains(output, "uid=1000") && contains(output, "hero"), "");

    // 73. w
    RUN("w");
    record("w", contains(output, "USER") && contains(output, "hero"), "");

    // 74. free
    RUN("free -h");
    record("free", contains(output, "Mem:") && contains(output, "4.0Gi"), "");

    // 75. lsof
    RUN("lsof");
    record("lsof", contains(output, "COMMAND") && contains(output, "PID"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── Networking (10) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 76. curl
    RUN("curl -s https://httpbin.org/get");
    record("curl", contains(output, "origin") || contains(output, "url") || output.size() > 10,
           std::to_string(output.size()) + " bytes");

    // 77. wget
    RUN("wget -q -O /test/wget_out.txt https://httpbin.org/robots.txt");
    RUN("cat /test/wget_out.txt");
    record("wget", output.size() > 0, std::to_string(output.size()) + " bytes");

    // 78. ping
    RUN("ping -c 2 127.0.0.1");
    record("ping", contains(output, "PING") && contains(output, "packets transmitted"), "");

    // 79. dig
    RUN("dig localhost");
    record("dig", contains(output, "ANSWER") || contains(output, "127.0.0.1"), "");

    // 80. ifconfig
    RUN("ifconfig");
    record("ifconfig", contains(output, "lo:") && contains(output, "eth0:"), "simulated");

    // 81. netstat
    RUN("netstat");
    record("netstat", contains(output, "Proto") && contains(output, "tcp"), "simulated");

    // 82. nc (stub)
    RUN("nc");
    record("nc", contains(output, "not available"), "stub");

    // 83. host
    RUN("host localhost");
    record("host", contains(output, "127.0.0.1") || contains(output, "has address"), "");

    // 84. traceroute
    RUN("traceroute 127.0.0.1");
    record("traceroute", contains(output, "traceroute to") && contains(output, "ms"), "simulated");

    // 85. nslookup
    RUN("nslookup localhost");
    record("nslookup", contains(output, "Server") || contains(output, "127.0.0.1"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── Archive & Checksum (7) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 86. tar create + 87. tar list + 88. tar extract
    fs.write("/test/tar_file1.txt", "tar content 1");
    fs.write("/test/tar_file2.txt", "tar content 2");
    RUN("tar cf /test/archive.tar /test/tar_file1.txt /test/tar_file2.txt");
    {
        bool create_ok = !contains(output, "error");
        RUN("tar tf /test/archive.tar");
        bool list_ok = contains(output, "tar_file1.txt") && contains(output, "tar_file2.txt");
        // Remove originals, extract
        fs.remove("/test/tar_file1.txt");
        fs.remove("/test/tar_file2.txt");
        RUN("tar xf /test/archive.tar");
        RUN("cat /test/tar_file1.txt");
        bool extract_ok = contains(output, "tar content 1");
        record("tar", create_ok && list_ok && extract_ok, "create+list+extract");
    }

    // 89. gzip + 90. gunzip
    fs.write("/test/compress_me.txt", "This is test data for compression testing. Repeated data repeated data repeated data.");
    RUN("gzip /test/compress_me.txt");
    {
        bool gz_exists = fs.exists("/test/compress_me.txt.gz");
        bool orig_gone = !fs.exists("/test/compress_me.txt");
        RUN("gunzip /test/compress_me.txt.gz");
        RUN("cat /test/compress_me.txt");
        bool content_ok = contains(output, "This is test data for compression");
        record("gzip", gz_exists && orig_gone, "compress");
        record("gunzip", content_ok, "decompress");
    }

    // 91. zip + 92. unzip
    fs.write("/test/zip_file.txt", "zip content here");
    RUN("zip /test/output.zip /test/zip_file.txt");
    {
        bool zip_ok = contains(output, "adding");
        fs.remove("/test/zip_file.txt");
        RUN("unzip /test/output.zip");
        bool unzip_ok = contains(output, "inflating");
        record("zip", zip_ok, "");
        record("unzip", unzip_ok, "");
    }

    // 93. md5sum
    fs.write("/test/hash_test.txt", "hello\n");
    RUN("md5sum /test/hash_test.txt");
    record("md5sum", output.size() >= 32 && contains(output, "hash_test.txt"), "");

    // 94. sha256sum
    RUN("sha256sum /test/hash_test.txt");
    record("sha256sum", output.size() >= 64 && contains(output, "hash_test.txt"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── HerOS (8) ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    // 95. launch (no app to launch in test, check error handling)
    {
        int s = RUN("launch nonexistent");
        record("launch", contains(output, "unknown app"), "error handling");
    }

    // 96. apps
    RUN("apps");
    record("apps", contains(output, "Installed Apps") || contains(output, "APP ID"), "");

    // 97. notify
    RUN("notify TestTitle TestBody");
    record("notify", contains(output, "Notification sent"), "");

    // 98. theme
    RUN("theme");
    record("theme", contains(output, "default") || contains(output, "Available"), "");

    // 99. wallpaper
    RUN("wallpaper");
    record("wallpaper", contains(output, "wallpaper"), "");

    // 100. help
    RUN("help");
    record("help", contains(output, "100") && contains(output, "Shell"), "");

    // 101. man
    RUN("man echo");
    record("man", contains(output, "SYNOPSIS") && contains(output, "echo"), "");

    // 102. neofetch
    RUN("neofetch");
    record("neofetch", contains(output, "HerOS") && contains(output, "hero") && contains(output, "Shell"), "");

    // ═══════════════════════════════════════════════════════════
    printf(YELLOW "\n── Pipes & Redirects ──" RESET "\n");
    // ═══════════════════════════════════════════════════════════

    RUN("echo hello world | grep hello");
    record("pipe basic", contains(output, "hello world"), "");

    RUN("seq 1 10 | sort -rn | head -n 3");
    record("pipe chain", contains(output, "10\n9\n8\n"), "");

    RUN("echo redirect_test > /test/redir.txt");
    RUN("cat /test/redir.txt");
    record("redirect >", contains(output, "redirect_test"), "");

    RUN("echo append_test >> /test/redir.txt");
    RUN("cat /test/redir.txt");
    record("redirect >>", contains(output, "redirect_test") && contains(output, "append_test"), "");

    RUN("echo first ; echo second");
    record("semicolon", contains(output, "second"), "");

    RUN("echo pipe_wc | wc -w");
    record("pipe wc", contains(output, "1"), "");

    // ═══════════════════════════════════════════════════════════
    // Summary
    // ═══════════════════════════════════════════════════════════
    printf("\n" CYAN "═══════════════════════════════════════════════════════" RESET "\n");
    printf("  Results: " GREEN "%d passed" RESET ", " RED "%d failed" RESET " out of %d tests\n",
           pass_count, fail_count, pass_count + fail_count);
    printf(CYAN "═══════════════════════════════════════════════════════" RESET "\n\n");

    if (fail_count > 0) {
        printf(RED "Failed tests:" RESET "\n");
        for (auto& r : results) {
            if (!r.passed) {
                printf("  - %s", r.name.c_str());
                if (!r.detail.empty()) printf(" (%s)", r.detail.c_str());
                printf("\n");
            }
        }
        printf("\n");
    }

    // Cleanup test dir
    fs.remove("/test");

    return fail_count > 0 ? 1 : 0;
}
