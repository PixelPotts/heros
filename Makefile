CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -fPIC $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image)
LDFLAGS := -rdynamic -ldl $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image)

SRC_DIR := src
BUILD_DIR := build
TARGET := heros

# ── Core OS sources (exclude app plugins) ─────────────────────────
CORE_SRCS := $(shell find $(SRC_DIR) -name '*.cpp' -not -path '$(SRC_DIR)/apps/*')
CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))

# ── App plugins (each builds to a .so) ────────────────────────────
APP_SRCS := $(wildcard $(SRC_DIR)/apps/*.cpp)
APP_SOS  := $(patsubst $(SRC_DIR)/apps/%.cpp,$(BUILD_DIR)/apps/%.so,$(APP_SRCS))

# ── App bundle directory ──────────────────────────────────────────
APPS_DIR := $(HOME)/.heros/apps

.PHONY: all clean run install-apps apps

all: $(TARGET) apps

# ── Core binary ───────────────────────────────────────────────────
$(TARGET): $(CORE_OBJS)
	$(CXX) $(CORE_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── App shared libraries ─────────────────────────────────────────
apps: $(APP_SOS)

# ── Terminal app needs extra libraries (curl, ssl, zlib, archive) ─
TERMINAL_LIBS := -lcurl -lssl -lcrypto -lz -larchive

$(BUILD_DIR)/apps/terminal_app.so: $(SRC_DIR)/apps/terminal_app.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -shared -o $@ $< $(TERMINAL_LIBS)

# ── All other apps (default rule, must come after specific rules) ─
$(BUILD_DIR)/apps/%.so: $(SRC_DIR)/apps/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -shared -o $@ $<

# ── Install app bundles into ~/.heros/apps/ ───────────────────────
install-apps: apps
	@# Journal
	@mkdir -p $(APPS_DIR)/com.heros.journal
	@cp $(BUILD_DIR)/apps/journal_app.so $(APPS_DIR)/com.heros.journal/journal.so
	@printf 'app_id=com.heros.journal\nname=Journal\nicon=journal\ncategory=productivity\nlibrary=journal.so\nsingleton=true\nstart_centered=true\ndock_pinned=true\ndock_order=4\ndefault_w=800\ndefault_h=550\nmin_w=400\nmin_h=300\n' > $(APPS_DIR)/com.heros.journal/manifest.conf
	@# Finance
	@mkdir -p $(APPS_DIR)/com.heros.finance
	@cp $(BUILD_DIR)/apps/finance_app.so $(APPS_DIR)/com.heros.finance/finance.so
	@printf 'app_id=com.heros.finance\nname=Finance\nicon=briefcase\ncategory=productivity\nlibrary=finance.so\nsingleton=true\nstart_maximized=true\nstart_centered=true\ndock_pinned=true\ndock_order=3\nautostart=true\ndefault_w=900\ndefault_h=600\nmin_w=500\nmin_h=400\n' > $(APPS_DIR)/com.heros.finance/manifest.conf
	@# Settings
	@mkdir -p $(APPS_DIR)/com.heros.settings
	@cp $(BUILD_DIR)/apps/settings_app.so $(APPS_DIR)/com.heros.settings/settings.so
	@printf 'app_id=com.heros.settings\nname=Settings\nicon=gear\ncategory=system\nlibrary=settings.so\nsingleton=true\nstart_centered=true\ndock_pinned=true\ndock_order=5\ndefault_w=750\ndefault_h=500\nmin_w=500\nmin_h=350\n' > $(APPS_DIR)/com.heros.settings/manifest.conf
	@# Task Manager
	@mkdir -p $(APPS_DIR)/com.heros.taskmanager
	@cp $(BUILD_DIR)/apps/taskmanager_app.so $(APPS_DIR)/com.heros.taskmanager/taskmanager.so
	@printf 'app_id=com.heros.taskmanager\nname=Task Manager\nicon=grid\ncategory=system\nlibrary=taskmanager.so\nsingleton=true\nstart_centered=true\ndock_pinned=true\ndock_order=6\ndefault_w=700\ndefault_h=450\nmin_w=450\nmin_h=300\n' > $(APPS_DIR)/com.heros.taskmanager/manifest.conf
	@# Files
	@mkdir -p $(APPS_DIR)/com.heros.files
	@cp $(BUILD_DIR)/apps/filemanager_app.so $(APPS_DIR)/com.heros.files/files.so
	@printf 'app_id=com.heros.files\nname=Files\nicon=book\ncategory=system\nlibrary=files.so\nsingleton=true\nstart_centered=true\ndock_pinned=true\ndock_order=7\ndefault_w=750\ndefault_h=500\nmin_w=400\nmin_h=300\n' > $(APPS_DIR)/com.heros.files/manifest.conf
	@# Terminal
	@mkdir -p $(APPS_DIR)/com.heros.terminal
	@cp $(BUILD_DIR)/apps/terminal_app.so $(APPS_DIR)/com.heros.terminal/terminal.so
	@printf 'app_id=com.heros.terminal\nname=Terminal\nicon=grid\ncategory=system\nlibrary=terminal.so\nsingleton=false\nstart_centered=true\ndock_pinned=true\ndock_order=2\ndefault_w=820\ndefault_h=520\nmin_w=400\nmin_h=300\n' > $(APPS_DIR)/com.heros.terminal/manifest.conf
	@echo "Installed 6 app bundles to $(APPS_DIR)"

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
