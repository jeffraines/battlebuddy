# Host build of the UI snapshot tool. Run `pio run` in firmware/tdisplay-s3 first
# so LovyanGFX is downloaded into .pio/libdeps.

LGFX := ../.pio/libdeps/tdisplay-s3/LovyanGFX/src
CORE := ../../../core
BUILD := build

LGFX_SRCS := $(wildcard $(LGFX)/lgfx/v1/*.cpp) \
             $(wildcard $(LGFX)/lgfx/v1/misc/*.cpp) \
             $(wildcard $(LGFX)/lgfx/v1/panel/Panel_Device.cpp) \
             $(wildcard $(LGFX)/lgfx/v1/platforms/framebuffer/*.cpp) \
             $(wildcard $(LGFX)/lgfx/Fonts/efont/*.c) \
             $(wildcard $(LGFX)/lgfx/Fonts/IPA/*.c) \
             $(wildcard $(LGFX)/lgfx/utility/*.c)
SRCS := snapshot.cpp ../src/ui.cpp $(wildcard $(CORE)/src/*.c)

CPPFLAGS := -DLGFX_LINUX_FB -I$(abspath $(LGFX)) -I$(abspath $(CORE)/include)
CXXFLAGS := -std=c++17 -O1 -w
CFLAGS := -std=c99 -O1 -w

.PHONY: snapshots clean

snapshots: $(BUILD)/snapshot
	./$(BUILD)/snapshot $(BUILD)

$(BUILD)/snapshot: $(SRCS) ../src/ui.h
	@mkdir -p $(BUILD)/obj
	cd $(BUILD)/obj && for f in $(abspath $(LGFX_SRCS) $(filter %.c,$(SRCS))); do \
		case $$f in *.c) $(CC) $(CPPFLAGS) $(CFLAGS) -c $$f || exit 1;; \
		            *) $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $$f || exit 1;; esac; done
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$(SRCS)) $(BUILD)/obj/*.o -o $@

clean:
	rm -rf $(BUILD)
