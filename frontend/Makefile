CXX ?= g++
TARGET := frontend
IMGUI := imgui
IMGUI_REF := v1.91.5

IMGUI_SRC := $(IMGUI)/imgui.cpp $(IMGUI)/imgui_draw.cpp \
             $(IMGUI)/imgui_tables.cpp $(IMGUI)/imgui_widgets.cpp \
             $(IMGUI)/backends/imgui_impl_sdl2.cpp \
             $(IMGUI)/backends/imgui_impl_opengl3.cpp
SRC := prototype_main.cpp $(IMGUI_SRC)
OBJ := $(SRC:%.cpp=build/%.o)

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -MMD -MP -I$(IMGUI) -I$(IMGUI)/backends \
            $(shell pkg-config --cflags sdl2 SDL2_image libcurl)

LDLIBS := $(shell pkg-config --cflags sdl2 SDL2_image libcurl) -lGLESv2

.PHONY: all run clean distclean
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDLIBS)

build/%.o: %.cpp | $(IMGUI)/imgui.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(IMGUI_SRC): | $(IMGUI)/imgui.h ;

# Grab ImGUI if it isn't here yet
$(IMGUI)/imgui.h:
	git clone --depth 1 --branch $(IMGUI_REF) https://github.com/ocornut/imgui.git $(IMGUI)

run: $(TARGET)
	./$(TARGET) --windowed

clean:
	rm -rf build $(TARGET)

distclean: clean
	rm -rf $(IMGUI)

-include $(OBJ:.o=.d)
