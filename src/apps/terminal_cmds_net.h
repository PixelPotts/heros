#pragma once
#include "terminal_shell.h"
#include <curl/curl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>

// ── libcurl write callback ──────────────────────────────────────

static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// ── 10 Network Commands ─────────────────────────────────────────

inline void register_net_commands(ShellEngine& shell) {

    // 1. curl — HTTP client
    shell.register_cmd("curl", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("curl: usage: curl [OPTIONS] URL"); return 1; }

        std::string url;
        std::string output_file;
        bool silent = false;
        std::string method = "GET";
        std::vector<std::string> headers;
        std::string post_data;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-o" && i + 1 < args.size()) output_file = args[++i];
            else if (args[i] == "-s") silent = true;
            else if (args[i] == "-X" && i + 1 < args.size()) method = args[++i];
            else if (args[i] == "-H" && i + 1 < args.size()) headers.push_back(args[++i]);
            else if (args[i] == "-d" && i + 1 < args.size()) { post_data = args[++i]; method = "POST"; }
            else if (args[i][0] != '-') url = args[i];
        }

        if (url.empty()) { ctx.outln("curl: no URL specified"); return 1; }

        CURL* curl = curl_easy_init();
        if (!curl) { ctx.outln("curl: failed to init"); return 1; }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HerOS-curl/1.0");

        if (method == "POST") curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        if (method == "PUT") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (method == "DELETE") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

        struct curl_slist* hlist = nullptr;
        for (auto& h : headers) hlist = curl_slist_append(hlist, h.c_str());
        if (hlist) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

        CURLcode res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (hlist) curl_slist_free_all(hlist);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            if (!silent) ctx.outln(std::string("curl: (") + std::to_string(res) + ") " + curl_easy_strerror(res));
            return 1;
        }

        if (!output_file.empty() && ctx.fs) {
            std::string path = resolve_path(ctx.cwd, output_file);
            ctx.fs->write(path, response);
            if (!silent) ctx.outln("Saved to " + output_file + " (" + std::to_string(response.size()) + " bytes)");
        } else {
            ctx.out(response);
            if (!response.empty() && response.back() != '\n') ctx.out("\n");
        }
        return 0;
    });

    // 2. wget — download files
    shell.register_cmd("wget", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("wget: usage: wget [OPTIONS] URL"); return 1; }

        std::string url;
        std::string output_file;
        bool quiet = false;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-O" && i + 1 < args.size()) output_file = args[++i];
            else if (args[i] == "-q") quiet = true;
            else if (args[i][0] != '-') url = args[i];
        }

        if (url.empty()) { ctx.outln("wget: missing URL"); return 1; }

        // Derive filename from URL if not specified
        if (output_file.empty()) {
            auto slash = url.rfind('/');
            if (slash != std::string::npos && slash + 1 < url.size()) {
                output_file = url.substr(slash + 1);
                auto q = output_file.find('?');
                if (q != std::string::npos) output_file = output_file.substr(0, q);
            }
            if (output_file.empty()) output_file = "index.html";
        }

        CURL* curl = curl_easy_init();
        if (!curl) { ctx.outln("wget: failed to init"); return 1; }

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HerOS-wget/1.0");

        if (!quiet) ctx.outln("--  " + url);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            if (!quiet) ctx.outln(std::string("wget: ") + curl_easy_strerror(res));
            return 1;
        }

        if (ctx.fs) {
            std::string path = resolve_path(ctx.cwd, output_file);
            ctx.fs->write(path, response);
        }

        if (!quiet) {
            ctx.outln("Saved '" + output_file + "' [" + std::to_string(response.size()) + "]");
        }
        return 0;
    });

    // 3. ping — simulated ping
    shell.register_cmd("ping", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("ping: usage: ping [-c COUNT] HOST"); return 1; }

        int count = 4;
        std::string host;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-c" && i + 1 < args.size()) {
                try { count = std::stoi(args[++i]); } catch (...) {}
            } else {
                host = args[i];
            }
        }

        if (host.empty()) { ctx.outln("ping: missing host"); return 1; }
        if (count > 10) count = 10;

        // Resolve hostname
        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        std::string ip = host;
        int gai_err = getaddrinfo(host.c_str(), nullptr, &hints, &res);
        if (gai_err == 0 && res) {
            char buf[INET_ADDRSTRLEN];
            struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            ip = buf;
            freeaddrinfo(res);
        }

        ctx.outln("PING " + host + " (" + ip + "): 56 data bytes");

        srand((unsigned)time(nullptr));
        double total_ms = 0;
        for (int i = 0; i < count; i++) {
            double ms = 5.0 + (rand() % 300) / 10.0;
            total_ms += ms;
            char line[128];
            snprintf(line, sizeof(line), "64 bytes from %s: icmp_seq=%d ttl=64 time=%.1f ms",
                     ip.c_str(), i, ms);
            ctx.outln(line);
            if (i < count - 1) std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        ctx.outln("--- " + host + " ping statistics ---");
        char stats[128];
        snprintf(stats, sizeof(stats), "%d packets transmitted, %d received, 0%% packet loss",
                 count, count);
        ctx.outln(stats);
        snprintf(stats, sizeof(stats), "rtt avg = %.1f ms", total_ms / count);
        ctx.outln(stats);
        return 0;
    });

    // 4. dig — DNS lookup
    shell.register_cmd("dig", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("dig: usage: dig HOSTNAME"); return 1; }
        std::string host = args[1];

        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int err = getaddrinfo(host.c_str(), nullptr, &hints, &res);
        if (err != 0) {
            ctx.outln(std::string(";; connection timed out: ") + gai_strerror(err));
            return 1;
        }

        ctx.outln("; <<>> HerOS dig <<>> " + host);
        ctx.outln(";; ANSWER SECTION:");

        for (struct addrinfo* p = res; p; p = p->ai_next) {
            char buf[INET6_ADDRSTRLEN];
            if (p->ai_family == AF_INET) {
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, buf, sizeof(buf));
                ctx.outln(host + ".\t300\tIN\tA\t" + std::string(buf));
            } else if (p->ai_family == AF_INET6) {
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, buf, sizeof(buf));
                ctx.outln(host + ".\t300\tIN\tAAAA\t" + std::string(buf));
            }
        }

        freeaddrinfo(res);
        return 0;
    });

    // 5. ifconfig — simulated network interfaces
    shell.register_cmd("ifconfig", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536");
        ctx.outln("        inet 127.0.0.1  netmask 255.0.0.0");
        ctx.outln("        inet6 ::1  prefixlen 128");
        ctx.outln("");
        ctx.outln("eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500");
        ctx.outln("        inet 192.168.1.100  netmask 255.255.255.0  broadcast 192.168.1.255");
        ctx.outln("        inet6 fe80::1  prefixlen 64");
        ctx.outln("        RX packets 12345  bytes 6789012 (6.4 MB)");
        ctx.outln("        TX packets 9876  bytes 3456789 (3.2 MB)");
        return 0;
    });

    // 6. netstat — simulated network stats
    shell.register_cmd("netstat", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("Active Internet connections");
        ctx.outln("Proto  Recv-Q  Send-Q  Local Address          Foreign Address        State");
        ctx.outln("tcp         0       0  0.0.0.0:8080           0.0.0.0:*              LISTEN");
        ctx.outln("tcp         0       0  192.168.1.100:443      93.184.216.34:443      ESTABLISHED");
        ctx.outln("udp         0       0  0.0.0.0:68             0.0.0.0:*");
        return 0;
    });

    // 7. nc — netcat (stub)
    shell.register_cmd("nc", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        (void)args;
        ctx.outln("nc: full netcat not available in HerOS");
        ctx.outln("Use curl or wget for HTTP requests.");
        return 1;
    });

    // 8. host — DNS lookup (getaddrinfo)
    shell.register_cmd("host", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("host: usage: host HOSTNAME"); return 1; }
        std::string hostname = args[1];

        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
        if (err != 0) {
            ctx.outln("Host " + hostname + " not found: " + std::string(gai_strerror(err)));
            return 1;
        }

        for (struct addrinfo* p = res; p; p = p->ai_next) {
            char buf[INET6_ADDRSTRLEN];
            if (p->ai_family == AF_INET) {
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, buf, sizeof(buf));
                ctx.outln(hostname + " has address " + std::string(buf));
            } else if (p->ai_family == AF_INET6) {
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, buf, sizeof(buf));
                ctx.outln(hostname + " has IPv6 address " + std::string(buf));
            }
        }

        freeaddrinfo(res);
        return 0;
    });

    // 9. traceroute — simulated traceroute
    shell.register_cmd("traceroute", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("traceroute: usage: traceroute HOST"); return 1; }
        std::string host = args[1];

        // Resolve
        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        std::string ip = host;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, buf, sizeof(buf));
            ip = buf;
            freeaddrinfo(res);
        }

        ctx.outln("traceroute to " + host + " (" + ip + "), 30 hops max, 60 byte packets");

        srand((unsigned)time(nullptr));
        int hops = 5 + rand() % 8;
        for (int i = 1; i <= hops; i++) {
            char hop_ip[32];
            if (i == hops) {
                snprintf(hop_ip, sizeof(hop_ip), "%s", ip.c_str());
            } else {
                snprintf(hop_ip, sizeof(hop_ip), "10.%d.%d.1", i, rand() % 256);
            }
            double t1 = 1.0 + (rand() % 200) / 10.0;
            double t2 = t1 + (rand() % 50) / 10.0;
            double t3 = t1 + (rand() % 50) / 10.0;
            char line[128];
            snprintf(line, sizeof(line), "%2d  %s  %.3f ms  %.3f ms  %.3f ms",
                     i, hop_ip, t1, t2, t3);
            ctx.outln(line);
        }
        return 0;
    });

    // 10. nslookup — DNS lookup
    shell.register_cmd("nslookup", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) { ctx.outln("nslookup: usage: nslookup HOSTNAME"); return 1; }
        std::string hostname = args[1];

        ctx.outln("Server:  127.0.0.53");
        ctx.outln("Address: 127.0.0.53#53");
        ctx.outln("");

        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
        if (err != 0) {
            ctx.outln("** server can't find " + hostname + ": " + std::string(gai_strerror(err)));
            return 1;
        }

        ctx.outln("Non-authoritative answer:");
        ctx.outln("Name:\t" + hostname);
        for (struct addrinfo* p = res; p; p = p->ai_next) {
            char buf[INET6_ADDRSTRLEN];
            if (p->ai_family == AF_INET) {
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, buf, sizeof(buf));
                ctx.outln("Address: " + std::string(buf));
            }
        }

        freeaddrinfo(res);
        return 0;
    });
}
