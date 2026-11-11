# GENRA Instruments (a plugin of its own, in this folder of the GENRA repo). Build from here:
#   make RACK_DIR=/path/to/Rack-SDK dist
# Path to the Rack SDK (override on the command line: make RACK_DIR=/path/to/Rack-SDK)
RACK_DIR ?= ../../..

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/jiant/*.cpp)
SOURCES += $(wildcard src/monk/*.cpp)
SOURCES += $(wildcard src/kwasub/*.cpp)
SOURCES += $(wildcard src/umlauf/*.cpp)
SOURCES += $(wildcard src/golana/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += licenses

include $(RACK_DIR)/arch.mk

# ---------------------------------------------------------------- the JIANT cores (VOICE, ANIMAL; DRUM-X is in GENRA)
# The engines of the JIANT FM firmware, each built as its own small shared library (jiantcore/*.c). Plain C with no
# Rack dependency; every module instance loads a private copy of its library, since the firmware keeps its state in
# static variables.
CORE_CC ?= $(CC)
CORE_FLAGS := -std=gnu11 -O2 -w -ffp-contract=off -DFELUCCA_VERSION='"VCV"' -I../jiant/build/gen -I../jiant/firmware/src
ifdef ARCH_LIN
	CORE_EXT := so
	CORE_FLAGS += -fPIC -fvisibility=hidden -shared
	CORE_LIBS := -lm
endif
ifdef ARCH_MAC
	CORE_EXT := dylib
	CORE_FLAGS += -fPIC -fvisibility=hidden -dynamiclib -include ../jiantcore/mac_section.h
	CORE_LIBS := -lm
endif
ifdef ARCH_WIN
	CORE_CC := gcc
	CORE_EXT := dll
	# the firmware's host build includes <sys/wait.h> (its test harness, unused here): jiantcore/compat stands in
	CORE_FLAGS += -shared -I../jiantcore/compat -static
	CORE_LIBS := -lm -lwinpthread
endif
CORES := res/voicecore.$(CORE_EXT) res/animalcore.$(CORE_EXT) res/spellcore.$(CORE_EXT)

# The firmware sources come packed in ../jiant.tar.gz (shared with GENRA's DRUM-X); unpacked on the first build.
# (unpacked next to it in the repo's top folder: jiantcore/*.c include ../jiant/...)
JIANT_STAMP := ../jiant/.unpacked
$(JIANT_STAMP): ../jiant.tar.gz
	cd .. && tar xzf jiant.tar.gz
	touch $@

res/%core.$(CORE_EXT): ../jiantcore/%core.c ../jiantcore/%core.h $(JIANT_STAMP)
	@mkdir -p res
	$(CORE_CC) $(CORE_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(CORE_LIBS)

# VOICE's SPELL: SPELLBOX's speech chip and text to speech (spellbox/, GPL-3.0) in a core of its own, likewise
res/spellcore.$(CORE_EXT): $(wildcard spellbox/*.c spellbox/*.h)

# ---------------------------------------------------------------- the K-WASUB core
# The K-WASUB firmware (kwasub/, unchanged) with kwcore/kwcore.c on top, likewise one private copy per instance.
# Built as the firmware itself is (-ffreestanding -fno-builtin …): its own memset / memcpy must not become calls to
# themselves.
KW_FLAGS := -std=gnu11 -O2 -w -fno-strict-aliasing -ffreestanding -fno-builtin \
	-Ikwasub/firmware/src -Ikwasub/firmware/gen -Ikwasub/firmware/hal
# gcc only (clang: -fno-builtin already keeps the loop-to-memset idiom away)
ifndef ARCH_MAC
	KW_FLAGS += -fno-tree-loop-distribute-patterns
endif
ifdef ARCH_LIN
	KW_FLAGS += -fPIC -fvisibility=hidden -shared
endif
ifdef ARCH_MAC
	KW_FLAGS += -fPIC -fvisibility=hidden -dynamiclib -include ../jiantcore/mac_section.h
endif
ifdef ARCH_WIN
	KW_FLAGS += -shared -static-libgcc
endif
KWCORE := res/kwcore.$(CORE_EXT)

$(KWCORE): kwcore/kwcore.c kwcore/kwcore.h $(wildcard kwasub/firmware/src/*.c kwasub/firmware/src/*.h kwasub/firmware/gen/*.h)
	@mkdir -p res
	$(CORE_CC) $(KW_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< -lm

# ---------------------------------------------------------------- the GOLANA core
# The Golana firmware (golana/, unchanged; golana/gen: its generated tables) with glcore/glcore.c on top, as its host
# renders build it (tests/hostsim.c), likewise one private copy per instance.
GL_FLAGS := -std=gnu11 -O2 -w -ffp-contract=off -Iglcore -Igolana/gen
ifdef ARCH_LIN
	GL_FLAGS += -fPIC -fvisibility=hidden -shared
endif
ifdef ARCH_MAC
	GL_FLAGS += -fPIC -fvisibility=hidden -dynamiclib -include ../jiantcore/mac_section.h
endif
ifdef ARCH_WIN
	GL_FLAGS += -shared -static-libgcc
endif
GLCORE := res/glcore.$(CORE_EXT)

$(GLCORE): glcore/glcore.c glcore/glcore.h glcore/golana_genra.c $(wildcard golana/firmware/src/*.c golana/firmware/src/*.h golana/gen/*.h)
	@mkdir -p res
	$(CORE_CC) $(GL_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< -lm

all: $(CORES) $(KWCORE) $(GLCORE)
dist: $(CORES) $(KWCORE) $(GLCORE)

# ---------------------------------------------------------------- the VADER engine (UMLAUF)
# vader/ is VADER (github.com/sunskiefer/VADER, MIT) as tools/vader_vendor.sh copies and patches it, built the way its
# own desktop plugin builds it (desktop/CMakeLists.txt): the engine in C, desktop/view_bus.c in place of the Force's
# display plane and web server, the ZYNTH voices with Mutable Instruments' code in C++ (LXR TONE, METAL and BAZZ on
# tables of our own: tools/umlauf_wavetables.py). Linked into the plugin.
V := vader
VB := build/vader
VADER_C := tables shape paths modes shapefile screen fx pictures qr engine
VADER_INC := -I$(V)/src -I$(V)/src/zynth -I$(V)/third_party/stb -I$(V)/third_party/mpc-vst-plugins/wrapper
ZYN_C := $(V)/src/zynth/engines.c $(V)/src/zynth/eng_osc.c $(V)/src/zynth/eng_fm.c $(V)/src/zynth/eng_more.c \
	$(V)/src/zynth/eng_new.c $(V)/src/zynth/lxrtab/wavetable.c
ZYN_INC := -I$(V)/src/zynth -I$(V)/src/zynth/shim -I$(V)/src/zynth/lxrtab
M := $(V)/third_party/mutable
MI_CC := $(V)/src/zynth/mi_bridge.cc $(M)/braids/macro_oscillator.cc $(M)/braids/digital_oscillator.cc \
	$(M)/braids/analog_oscillator.cc $(M)/braids/resources.cc $(M)/plaits/dsp/engine/wavetable_engine.cc \
	$(M)/plaits/dsp/physical_modelling/modal_voice.cc $(M)/plaits/dsp/physical_modelling/resonator.cc \
	$(M)/plaits/dsp/physical_modelling/string.cc $(M)/plaits/dsp/physical_modelling/string_voice.cc \
	$(M)/plaits/resources.cc $(M)/stmlib/dsp/units.cc $(M)/stmlib/utils/random.cc
VADER_ARCH = $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS))
VADER_CFLAGS = -std=gnu11 -O2 -w -fPIC -fno-strict-aliasing -ffp-contract=off -D_GNU_SOURCE $(VADER_ARCH)
VADER_CXXFLAGS = -std=c++11 -O2 -w -fPIC -ffp-contract=off -fno-exceptions -fno-rtti -fno-threadsafe-statics -DTEST=1 -D_USE_MATH_DEFINES=1 $(VADER_ARCH)
VADER_CC ?= $(CC)
ifdef ARCH_WIN
	VADER_CC := gcc
	VADER_ENGINE_FLAGS := -include $(V)/desktop/compat.h
	LDFLAGS += -Wl,-Bstatic -lpthread -Wl,-Bdynamic
else
	LDFLAGS += -lpthread
endif
VADER_OBJS := $(patsubst %,$(VB)/%.o,$(VADER_C)) $(VB)/view_bus.o \
	$(patsubst $(V)/%.c,$(VB)/z/%.o,$(ZYN_C)) $(patsubst $(V)/%.cc,$(VB)/m/%.o,$(MI_CC))
VADER_LIB := $(VB)/libvader.a

$(VB)/%.o: $(V)/src/%.c $(wildcard $(V)/src/*.h)
	@mkdir -p $(@D)
	$(VADER_CC) $(VADER_CFLAGS) $(VADER_ENGINE_FLAGS) $(VADER_INC) -c $< -o $@
$(VB)/view_bus.o: $(V)/desktop/view_bus.c
	@mkdir -p $(@D)
	$(VADER_CC) $(VADER_CFLAGS) $(VADER_ENGINE_FLAGS) $(VADER_INC) -c $< -o $@
$(VB)/z/%.o: $(V)/%.c
	@mkdir -p $(@D)
	$(VADER_CC) $(VADER_CFLAGS) -D_USE_MATH_DEFINES=1 -DTEST=1 $(ZYN_INC) -c $< -o $@
$(VB)/m/%.o: $(V)/%.cc
	@mkdir -p $(@D)
	$(CXX) $(VADER_CXXFLAGS) -I$(V)/src/zynth -I$(M) -c $< -o $@
$(VADER_LIB): $(VADER_OBJS)
	$(AR) rcs $@ $^

OBJECTS += $(VADER_LIB)

include $(RACK_DIR)/plugin.mk

clean: clean-cores
clean-cores:
	rm -f res/*core.so res/*core.dylib res/*core.dll
	rm -rf build/vader
