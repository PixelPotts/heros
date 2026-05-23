#pragma once
// ── HerOS App SDK ─────────────────────────────────────────────────
// Include this single header in your app plugin .cpp.
// Provides the AppContent base class, all OS drawing/widget APIs,
// and the ABI contract for dynamic loading.

#include "window.h"
#include "draw.h"
#include "frost.h"
#include "widgets.h"
#include "vfs.h"
#include "theme.h"
#include "event_bus.h"

// ── ABI version ──────────────────────────────────────────────────

#define HEROS_ABI_VERSION 1

// ── App info struct returned by heros_app_info() ─────────────────

struct HerosAppInfo {
    int abi_version;        // must equal HEROS_ABI_VERSION
    const char* app_id;     // e.g. "com.heros.journal"
    const char* name;       // display name
    const char* version;    // semver string
};

// ── Convenience macro — place at bottom of each app .cpp ─────────
//
// Usage:
//   HEROS_APP(JournalApp, "com.heros.journal", "Journal", "0.1.0")
//
// Expands to the two C-linkage exports the loader expects:
//   heros_app_info()   — returns static metadata
//   heros_create_app() — factory that returns raw AppContent*

#define HEROS_APP(ClassName, id, display_name, ver)                 \
    extern "C" const HerosAppInfo* heros_app_info() {              \
        static const HerosAppInfo info = {                         \
            HEROS_ABI_VERSION, id, display_name, ver               \
        };                                                         \
        return &info;                                              \
    }                                                              \
    extern "C" AppContent* heros_create_app() {                    \
        return new ClassName();                                    \
    }
