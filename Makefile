CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image)
LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image)

SRC_DIR := src
BUILD_DIR := build
TARGET := heros

SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
