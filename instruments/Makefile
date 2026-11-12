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
# from GENRA Test (2.38)
SOURCES += $(wildcard src/math/*.cpp)
SOURCES += $(wildcard src/kurbel/*.cpp)
SOURCES += $(wildcard src/swarm/*.cpp)
SOURCES += $(wildcard src/lied/*.cpp)
SOURCES += $(wildcard src/canta/*.cpp)
SOURCES += $(wildcard src/miaou/*.cpp)
# MIAOU's engine (miaoucore/: MIAOU's toy.c with its voices in a struct) is plain C, built into the plugin itself
SOURCES += miaoucore/miaoucore.c
SOURCES += $(wildcard src/saurus/*.cpp)
SOURCES += $(wildcard src/fumi/*.cpp)
SOURCES += $(wildcard src/kessel/*.cpp)

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

# ---------------------------------------------------------------- the cores from GENRA Test (2.38)
# MATH, KURBEL, SWARM, LIED, CANTA, MIAOU, SAURUS, FUMI and KESSEL were built and tested in the plugin GENRA Test
# (repo Camillowup/testmodulesvcv) and moved here. Their firmware snapshots are packed in this folder (*.tar.gz) and
# unpacked here on the first build. Note: instruments/jiant.tar.gz is a different JIANT snapshot (branch codex/swarm,
# for MATH and SWARM) from ../jiant.tar.gz (VOICE, ANIMAL, DRUM-X); it unpacks into instruments/jiant/.
T_CORE_FLAGS := -std=gnu11 -O2 -w -ffp-contract=off -DFELUCCA_VERSION='"VCV"' -Ijiant/build/gen -Ijiant/firmware/src
ifdef ARCH_LIN
	T_CORE_FLAGS += -fPIC -fvisibility=hidden -shared
endif
ifdef ARCH_MAC
	T_CORE_FLAGS += -fPIC -fvisibility=hidden -dynamiclib -include mathcore/compat/mac_section.h
endif
ifdef ARCH_WIN
	T_CORE_FLAGS += -shared -Imathcore/compat -static
endif
T_CORE_LIBS := -lm
ifdef ARCH_WIN
	T_CORE_LIBS := -lm -lwinpthread
endif
T_CORES := res/mathcore.$(CORE_EXT) res/kurbelcore.$(CORE_EXT) res/swarmcore.$(CORE_EXT) res/liedcore.$(CORE_EXT) \
	res/cantacore.$(CORE_EXT) res/sauruscore.$(CORE_EXT) res/fumicore.$(CORE_EXT) res/kesselcore.$(CORE_EXT)

T_JIANT_STAMP := jiant/.unpacked
$(T_JIANT_STAMP): jiant.tar.gz
	tar xzf jiant.tar.gz
	touch $@

res/mathcore.$(CORE_EXT): mathcore/mathcore.c mathcore/mathcore.h $(T_JIANT_STAMP)
	@mkdir -p res
	$(CORE_CC) $(T_CORE_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(T_CORE_LIBS)

# ---------------------------------------------------------------- the SWARM core
# The same JIANT snapshot with swarmcore/swarmcore.c on top (SWARM: eng_input.c, with virtual devices instead of a radio).
res/swarmcore.$(CORE_EXT): swarmcore/swarmcore.c swarmcore/swarmcore.h $(T_JIANT_STAMP)
	@mkdir -p res
	$(CORE_CC) $(T_CORE_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(T_CORE_LIBS)

# ---------------------------------------------------------------- the KURBEL core
# ESTAMPIE's firmware (estampie.tar.gz, a snapshot: estampie/SNAPSHOT.txt) with kurbelcore/kurbelcore.c on top, built
# the same way (its own includes instead of JIANT's).
KURBEL_FLAGS := $(subst -Ijiant/build/gen -Ijiant/firmware/src,-Iestampie/build/gen -Iestampie/firmware/src,$(T_CORE_FLAGS))
ESTAMPIE_STAMP := estampie/.unpacked
$(ESTAMPIE_STAMP): estampie.tar.gz
	tar xzf estampie.tar.gz
	touch $@

res/kurbelcore.$(CORE_EXT): kurbelcore/kurbelcore.c kurbelcore/kurbelcore.h $(ESTAMPIE_STAMP)
	@mkdir -p res
	$(CORE_CC) $(KURBEL_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(T_CORE_LIBS)

# ---------------------------------------------------------------- the LIED core
# eSpeak NG 1.52.0 (espeak.tar.gz, a snapshot of its library sources: espeak/SNAPSHOT.txt) with liedcore/liedcore.c on
# top, as one small shared library. Loaded once per process (eSpeak's state is static; LIED calls it under a lock).
# Its language data (compiled from the same release) is in res/espeak-ng-data.
ESPEAK_STAMP := espeak/.unpacked
$(ESPEAK_STAMP): espeak.tar.gz
	tar xzf espeak.tar.gz
	touch $@

ESPEAK_LIB := espeak/src/libespeak-ng
ESPEAK_C := common mnemonics error ieee80 compiledata compiledict dictionary encoding intonation langopts numbers \
	phoneme phonemelist readclause setlengths soundicon spect ssml synthdata synthesize tr_languages translate \
	translateword voices wavegen speech espeak_api klatt
UCD_C := case categories ctype proplist scripts tostring
LIED_SRC := liedcore/liedcore.c $(addprefix $(ESPEAK_LIB)/,$(addsuffix .c,$(ESPEAK_C))) \
	$(addprefix espeak/src/ucd-tools/src/,$(addsuffix .c,$(UCD_C)))
LIED_FLAGS := -std=gnu11 -O2 -w -DLIBESPEAK_NG_EXPORT=1 -Iliedcore -Iespeak/src/include -Iespeak/src/include/compat \
	-Iespeak/src/ucd-tools/src/include \
	-I$(ESPEAK_LIB) -DPATH_ESPEAK_DATA='"espeak-ng-data"' \
	-Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration
# (PATH_ESPEAK_DATA: what eSpeak's CMake defines; LIED always passes its own path. The -Wno-error: GCC 14 made these
#  errors, eSpeak's Windows code still has one)
ifdef ARCH_LIN
	LIED_FLAGS += -fPIC -fvisibility=hidden -shared
	LIED_LIBS := -lm
endif
ifdef ARCH_MAC
	LIED_FLAGS += -fPIC -fvisibility=hidden -dynamiclib
	LIED_LIBS := -lm
endif
ifdef ARCH_WIN
	LIED_FLAGS += -shared -static
	LIED_LIBS := -lm -ladvapi32
endif

res/liedcore.$(CORE_EXT): liedcore/liedcore.c liedcore/liedcore.h liedcore/config.h $(ESPEAK_STAMP)
	@mkdir -p res
	$(CORE_CC) $(LIED_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $(LIED_SRC) $(LIED_LIBS)

# ---------------------------------------------------------------- the CANTA core
# MIKU-FM's singing voice (canta.tar.gz, a snapshot of the voice's code only: canta/SNAPSHOT.txt) with
# cantacore/cantacore.c on top, as its own small shared library; every instance loads a private copy (the effects keep
# their buffers in static variables). Plain C, no libc.
CANTA_STAMP := canta/.unpacked
$(CANTA_STAMP): canta.tar.gz
	tar xzf canta.tar.gz
	touch $@

CANTA_SRC := cantacore/cantacore.c canta/firmware/audio/fvoice/fvoice.c canta/firmware/audio/vfx/vfx.c \
	canta/firmware/song/mora.c canta/firmware/song/lyrics.c
CANTA_FLAGS := $(subst -Ijiant/build/gen -Ijiant/firmware/src,-Icanta,$(T_CORE_FLAGS))

res/cantacore.$(CORE_EXT): cantacore/cantacore.c cantacore/cantacore.h $(CANTA_STAMP)
	@mkdir -p res
	$(CORE_CC) $(CANTA_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $(CANTA_SRC) $(T_CORE_LIBS)

# ---------------------------------------------------------------- the SAURUS core
# PALEOTONE's firmware (paleotone.tar.gz, a snapshot of what its host build needs: paleotone/SNAPSHOT.txt) with
# sauruscore/sauruscore.c on top, built the same way (its own includes). Every instance loads a private copy.
SAURUS_FLAGS := $(subst -Ijiant/build/gen -Ijiant/firmware/src,-Ipaleotone/build/gen -Ipaleotone/firmware/src,$(T_CORE_FLAGS))
PALEOTONE_STAMP := paleotone/.unpacked
$(PALEOTONE_STAMP): paleotone.tar.gz
	tar xzf paleotone.tar.gz
	touch $@

res/sauruscore.$(CORE_EXT): sauruscore/sauruscore.c sauruscore/sauruscore.h $(PALEOTONE_STAMP)
	@mkdir -p res
	$(CORE_CC) $(SAURUS_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(T_CORE_LIBS)
# ---------------------------------------------------------------- the FUMI core
# FuMi-1's instrument (fumi.tar.gz, a snapshot of its firmware/src/dsp: fumi/SNAPSHOT.txt; the patch table reduced to
# FuMi-1's own six) with fumicore/fumicore.c on top, as its own small shared library; every instance loads a private
# copy (the instrument's state is static). Plain C, no libm calls (fastmath.h).
FUMI_STAMP := fumi/.unpacked
$(FUMI_STAMP): fumi.tar.gz
	tar xzf fumi.tar.gz
	touch $@

FUMI_FLAGS := $(subst -Ijiant/build/gen -Ijiant/firmware/src,-DFM_HOST -Ifumicore -Ifumi/firmware/src -Ifumi/firmware/src/dsp,$(T_CORE_FLAGS))

res/fumicore.$(CORE_EXT): fumicore/fumicore.c fumicore/fumicore.h $(FUMI_STAMP)
	@mkdir -p res
	$(CORE_CC) $(FUMI_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(T_CORE_LIBS)

# ---------------------------------------------------------------- the KESSEL core
# The LXR firmware's sound engine (Erica Synths; kessel.tar.gz, a snapshot of its headers and sound engine sources,
# with two reads past a table's end put right: kessel/SNAPSHOT.txt) with kesselcore/ on top (kesselcore.c, kc_cc.c: the firmware's parameter handler), as its own
# small shared library; every instance loads a private copy (the voices are static). kesselcore/compat stands in for
# the STM32 HAL; kc_sections.h turns the firmware's memory sections into plain ones (forced in on every system).
KESSEL_STAMP := kessel/.unpacked
$(KESSEL_STAMP): kessel.tar.gz
	tar xzf kessel.tar.gz
	touch $@

KESSEL_SRC := kesselcore/kesselcore.c kesselcore/kc_cc.c \
	kessel/Core/Src/Preset/initKit.c kessel/Core/Src/Soundengine/DSPAudio/1PoleLp.c \
	kessel/Core/Src/Soundengine/DSPAudio/BufferTools.c \
	kessel/Core/Src/Soundengine/DSPAudio/CymbalVoice.c kessel/Core/Src/Soundengine/DSPAudio/Decay.c \
	kessel/Core/Src/Soundengine/DSPAudio/DrumVoice.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/Compressor.c kessel/Core/Src/Soundengine/DSPAudio/FX/Delay.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/Distortion.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/FxRamBuffer.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/Reverb.c kessel/Core/Src/Soundengine/DSPAudio/FX/Ringmod.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/VerbTools/AllpassDelay.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/VerbTools/VerbDelay.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/VerbTools/VerbModDelay.c \
	kessel/Core/Src/Soundengine/DSPAudio/FX/VerbTools/sinCos.c \
	kessel/Core/Src/Soundengine/DSPAudio/HiHat.c kessel/Core/Src/Soundengine/DSPAudio/Oscillator.c \
	kessel/Core/Src/Soundengine/DSPAudio/ResonantFilter.c kessel/Core/Src/Soundengine/DSPAudio/Samples.c \
	kessel/Core/Src/Soundengine/DSPAudio/SlopeEg2.c kessel/Core/Src/Soundengine/DSPAudio/Snare.c \
	kessel/Core/Src/Soundengine/DSPAudio/TiltEq.c kessel/Core/Src/Soundengine/DSPAudio/distortion.c \
	kessel/Core/Src/Soundengine/DSPAudio/dither.c kessel/Core/Src/Soundengine/DSPAudio/lfo.c \
	kessel/Core/Src/Soundengine/DSPAudio/random.c kessel/Core/Src/Soundengine/DSPAudio/snapEg.c \
	kessel/Core/Src/Soundengine/DSPAudio/squareRootLut.c \
	kessel/Core/Src/Soundengine/DSPAudio/transientGenerator.c \
	kessel/Core/Src/Soundengine/DSPAudio/transientTables.c \
	kessel/Core/Src/Soundengine/DSPAudio/wavetable.c kessel/Core/Src/Soundengine/FxBus.c
KESSEL_FLAGS := $(subst -Ijiant/build/gen -Ijiant/firmware/src,-Ikesselcore/compat -Ikesselcore -Ikessel/Core/Inc \
	-include kesselcore/compat/kc_sections.h,$(T_CORE_FLAGS))
KESSEL_FLAGS := $(subst -include mathcore/compat/mac_section.h,,$(KESSEL_FLAGS))

res/kesselcore.$(CORE_EXT): kesselcore/kesselcore.c kesselcore/kesselcore.h kesselcore/kc_cc.c kesselcore/kc_lxr.h $(KESSEL_STAMP)
	@mkdir -p res
	$(CORE_CC) $(KESSEL_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $(KESSEL_SRC) $(T_CORE_LIBS)

all: $(CORES) $(KWCORE) $(GLCORE) $(T_CORES)
dist: $(CORES) $(KWCORE) $(GLCORE) $(T_CORES)

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
