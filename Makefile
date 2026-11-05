# Path to the Rack SDK (override on the command line: make RACK_DIR=/path/to/Rack-SDK)
RACK_DIR ?= ../..

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/jiant/*.cpp)
SOURCES += $(wildcard src/monk/*.cpp) src/monk/monkface.c

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/arch.mk

# ---------------------------------------------------------------- the JIANT cores (VOICE, DRUM-X, ANIMAL)
# The engines of the JIANT FM firmware, each built as its own small shared library (jiantcore/*.c). Plain C with no
# Rack dependency; every module instance loads a private copy of its library, since the firmware keeps its state in
# static variables.
CORE_CC ?= $(CC)
CORE_FLAGS := -std=gnu11 -O2 -w -ffp-contract=off -DFELUCCA_VERSION='"VCV"' -Ijiant/build/gen -Ijiant/firmware/src
ifdef ARCH_LIN
	CORE_EXT := so
	CORE_FLAGS += -fPIC -fvisibility=hidden -shared
	CORE_LIBS := -lm
endif
ifdef ARCH_MAC
	CORE_EXT := dylib
	CORE_FLAGS += -fPIC -fvisibility=hidden -dynamiclib
	CORE_LIBS := -lm
endif
ifdef ARCH_WIN
	CORE_CC := gcc
	CORE_EXT := dll
	# the firmware's host build includes <sys/wait.h> (its test harness, unused here): jiantcore/compat stands in
	CORE_FLAGS += -shared -Ijiantcore/compat -static
	CORE_LIBS := -lm -lwinpthread
endif
CORES := res/voicecore.$(CORE_EXT) res/drumcore.$(CORE_EXT) res/animalcore.$(CORE_EXT)

# The firmware sources come packed in jiant.tar.gz (one file for all three); unpacked on the first build.
JIANT_STAMP := jiant/.unpacked
$(JIANT_STAMP): jiant.tar.gz
	tar xzf jiant.tar.gz
	touch $@

res/%core.$(CORE_EXT): jiantcore/%core.c jiantcore/%core.h $(JIANT_STAMP)
	@mkdir -p res
	$(CORE_CC) $(CORE_FLAGS) $(filter --target% -arch% -march% -mmacosx-version-min%,$(FLAGS)) -o $@ $< $(CORE_LIBS)

all: $(CORES)
dist: $(CORES)

include $(RACK_DIR)/plugin.mk

clean: clean-cores
clean-cores:
	rm -f res/*core.so res/*core.dylib res/*core.dll
	rm -rf jiant
