#pragma once
#include "terminal_shell.h"
#include <zlib.h>
#include <openssl/evp.h>
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <sstream>
#include <iomanip>

// ── Helper: compute hash with EVP ──────────────────────────────

static inline std::string compute_hash(const std::string& data, const EVP_MD* md) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

// ── 7 Archive/Checksum Commands ─────────────────────────────────

inline void register_archive_commands(ShellEngine& shell) {

    // 1. tar — create/extract/list archives (custom VFS-based)
    shell.register_cmd("tar", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("tar: usage: tar [cxtf] [-f FILE] [FILES...]"); return 1; }

        bool create = false, extract = false, list = false;
        std::string archive_file;
        std::vector<std::string> files;

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-' || (i == 1 && args[i].find_first_of("cxtf") != std::string::npos)) {
                std::string flags = args[i];
                if (flags[0] == '-') flags = flags.substr(1);
                for (char c : flags) {
                    switch (c) {
                        case 'c': create = true; break;
                        case 'x': extract = true; break;
                        case 't': list = true; break;
                        case 'f':
                            if (i + 1 < args.size()) archive_file = args[++i];
                            break;
                        case 'v': break; // verbose (ignored, always verbose)
                        case 'z': break; // gzip (handled separately)
                    }
                }
            } else {
                files.push_back(args[i]);
            }
        }

        if (archive_file.empty()) { ctx.outln("tar: no archive file specified (-f)"); return 1; }
        std::string abs_archive = resolve_path(ctx.cwd, archive_file);

        if (create) {
            // Simple custom archive format: header lines + content
            // Format: PATH\tSIZE\n then SIZE bytes of content
            std::string archive_data;
            for (auto& f : files) {
                std::string abs = resolve_path(ctx.cwd, f);
                auto info = ctx.fs->stat(abs);
                if (info.is_directory) {
                    // Recurse
                    auto entries = ctx.fs->list(abs);
                    for (auto& e : entries) {
                        std::string sub = f + "/" + e.name;
                        if (!e.is_directory) {
                            std::string content = ctx.fs->read(abs + "/" + e.name);
                            archive_data += sub + "\t" + std::to_string(content.size()) + "\n";
                            archive_data += content;
                        }
                    }
                } else {
                    std::string content = ctx.fs->read(abs);
                    archive_data += f + "\t" + std::to_string(content.size()) + "\n";
                    archive_data += content;
                }
                ctx.outln(f);
            }
            ctx.fs->write(abs_archive, archive_data);
            return 0;
        }

        if (extract || list) {
            std::string data = ctx.fs->read(abs_archive);
            if (data.empty()) { ctx.outln("tar: cannot open: " + archive_file); return 1; }

            size_t pos = 0;
            while (pos < data.size()) {
                size_t nl = data.find('\n', pos);
                if (nl == std::string::npos) break;
                std::string header = data.substr(pos, nl - pos);
                pos = nl + 1;

                size_t tab = header.find('\t');
                if (tab == std::string::npos) break;
                std::string path = header.substr(0, tab);
                size_t size = 0;
                try { size = std::stoull(header.substr(tab + 1)); } catch (...) { break; }

                if (list) {
                    ctx.outln(path);
                }

                if (extract && pos + size <= data.size()) {
                    std::string content = data.substr(pos, size);
                    std::string abs = resolve_path(ctx.cwd, path);
                    // Ensure parent dirs
                    auto slash = abs.rfind('/');
                    if (slash != std::string::npos) {
                        std::string dir = abs.substr(0, slash);
                        ctx.fs->mkdir(dir);
                    }
                    ctx.fs->write(abs, content);
                    ctx.outln(path);
                }

                pos += size;
            }
            return 0;
        }

        ctx.outln("tar: must specify -c, -x, or -t");
        return 1;
    });

    // 2. gzip — compress file using zlib
    shell.register_cmd("gzip", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("gzip: missing file operand"); return 1; }

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            if (content.empty() && !ctx.fs->exists(path)) {
                ctx.outln("gzip: " + args[i] + ": No such file");
                continue;
            }

            uLongf dest_len = compressBound(content.size());
            std::string compressed(dest_len, '\0');
            int ret = compress2((Bytef*)compressed.data(), &dest_len,
                               (const Bytef*)content.data(), content.size(), Z_DEFAULT_COMPRESSION);
            if (ret != Z_OK) {
                ctx.outln("gzip: compression failed");
                continue;
            }
            compressed.resize(dest_len);

            // Store original size at start (8 bytes)
            uint64_t orig_size = content.size();
            std::string output(8, '\0');
            memcpy(&output[0], &orig_size, 8);
            output += compressed;

            ctx.fs->write(path + ".gz", output);
            ctx.fs->remove(path);
            ctx.outln(args[i] + " -> " + args[i] + ".gz");
        }
        return 0;
    });

    // 3. gunzip — decompress file using zlib
    shell.register_cmd("gunzip", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("gunzip: missing file operand"); return 1; }

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            if (content.size() < 8) {
                ctx.outln("gunzip: " + args[i] + ": not in gzip format");
                continue;
            }

            uint64_t orig_size = 0;
            memcpy(&orig_size, content.data(), 8);
            if (orig_size > 100 * 1024 * 1024) {
                ctx.outln("gunzip: file too large");
                continue;
            }

            std::string decompressed(orig_size, '\0');
            uLongf dest_len = orig_size;
            int ret = uncompress((Bytef*)decompressed.data(), &dest_len,
                                (const Bytef*)content.data() + 8, content.size() - 8);
            if (ret != Z_OK) {
                ctx.outln("gunzip: decompression failed (error " + std::to_string(ret) + ")");
                continue;
            }
            decompressed.resize(dest_len);

            // Strip .gz extension
            std::string out_path = path;
            if (out_path.size() > 3 && out_path.substr(out_path.size() - 3) == ".gz") {
                out_path = out_path.substr(0, out_path.size() - 3);
            } else {
                out_path += ".ungz";
            }

            ctx.fs->write(out_path, decompressed);
            ctx.fs->remove(path);

            std::string out_name = args[i];
            if (out_name.size() > 3 && out_name.substr(out_name.size() - 3) == ".gz") {
                out_name = out_name.substr(0, out_name.size() - 3);
            }
            ctx.outln(args[i] + " -> " + out_name);
        }
        return 0;
    });

    // 4. zip — create zip archive (libarchive)
    shell.register_cmd("zip", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 3) { ctx.outln("zip: usage: zip ARCHIVE FILE..."); return 1; }

        std::string archive_name = args[1];
        if (archive_name.find(".zip") == std::string::npos) archive_name += ".zip";
        std::string abs_archive = resolve_path(ctx.cwd, archive_name);

        struct archive* a = archive_write_new();
        archive_write_set_format_zip(a);

        // Write to memory buffer
        std::string zip_data;
        archive_write_open(a, &zip_data,
            nullptr,
            [](struct archive*, void* ud, const void* buf, size_t len) -> la_ssize_t {
                std::string* data = static_cast<std::string*>(ud);
                data->append(static_cast<const char*>(buf), len);
                return (la_ssize_t)len;
            },
            nullptr);

        for (size_t i = 2; i < args.size(); i++) {
            std::string abs = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(abs);

            struct archive_entry* entry = archive_entry_new();
            archive_entry_set_pathname(entry, args[i].c_str());
            archive_entry_set_size(entry, content.size());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_write_header(a, entry);
            archive_write_data(a, content.data(), content.size());
            archive_entry_free(entry);
            ctx.outln("  adding: " + args[i]);
        }

        archive_write_close(a);
        archive_write_free(a);

        ctx.fs->write(abs_archive, zip_data);
        return 0;
    });

    // 5. unzip — extract zip archive (libarchive)
    shell.register_cmd("unzip", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (!ctx.fs || args.size() < 2) { ctx.outln("unzip: usage: unzip ARCHIVE"); return 1; }

        std::string abs_archive = resolve_path(ctx.cwd, args[1]);
        std::string zip_data = ctx.fs->read(abs_archive);
        if (zip_data.empty()) { ctx.outln("unzip: cannot find: " + args[1]); return 1; }

        struct archive* a = archive_read_new();
        archive_read_support_format_zip(a);
        archive_read_support_filter_all(a);

        int r = archive_read_open_memory(a, zip_data.data(), zip_data.size());
        if (r != ARCHIVE_OK) {
            ctx.outln("unzip: failed to open archive");
            archive_read_free(a);
            return 1;
        }

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            std::string pathname = archive_entry_pathname(entry);
            size_t size = archive_entry_size(entry);

            if (archive_entry_filetype(entry) == AE_IFDIR) {
                std::string abs = resolve_path(ctx.cwd, pathname);
                ctx.fs->mkdir(abs);
                ctx.outln("   creating: " + pathname);
            } else {
                std::string content(size, '\0');
                archive_read_data(a, &content[0], size);
                std::string abs = resolve_path(ctx.cwd, pathname);
                // Ensure parent dir
                auto slash = abs.rfind('/');
                if (slash != std::string::npos) ctx.fs->mkdir(abs.substr(0, slash));
                ctx.fs->write(abs, content);
                ctx.outln("  inflating: " + pathname);
            }
        }

        archive_read_free(a);
        return 0;
    });

    // 6. md5sum — compute MD5 hash
    shell.register_cmd("md5sum", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) {
            // Hash stdin
            std::string hash = compute_hash(ctx.stdin_data, EVP_md5());
            ctx.outln(hash + "  -");
            return 0;
        }

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            if (!ctx.fs) continue;
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            if (content.empty() && !ctx.fs->exists(path)) {
                ctx.outln("md5sum: " + args[i] + ": No such file");
                continue;
            }
            std::string hash = compute_hash(content, EVP_md5());
            ctx.outln(hash + "  " + args[i]);
        }
        return 0;
    });

    // 7. sha256sum — compute SHA-256 hash
    shell.register_cmd("sha256sum", [](std::vector<std::string>& args, CmdContext& ctx) -> int {
        if (args.size() < 2) {
            std::string hash = compute_hash(ctx.stdin_data, EVP_sha256());
            ctx.outln(hash + "  -");
            return 0;
        }

        for (size_t i = 1; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            if (!ctx.fs) continue;
            std::string path = resolve_path(ctx.cwd, args[i]);
            std::string content = ctx.fs->read(path);
            if (content.empty() && !ctx.fs->exists(path)) {
                ctx.outln("sha256sum: " + args[i] + ": No such file");
                continue;
            }
            std::string hash = compute_hash(content, EVP_sha256());
            ctx.outln(hash + "  " + args[i]);
        }
        return 0;
    });
}
