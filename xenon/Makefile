#---------------------------------------------------------------------------------
# VVVVVV - libXenon (bare-metal Xbox 360) build
#
# Produces vvvvvv.elf32, launchable from Aurora's File Manager or XeLL.
# Build inside the free60/libxenon Docker image:
#   docker run --rm -v "$PWD":/app -w /app free60/libxenon:latest bash -lc "make -C xenon"
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITXENON)),)
$(error "Please set DEVKITXENON in your environment, e.g. export DEVKITXENON=/usr/local/xenon")
endif

include $(DEVKITXENON)/rules

#---------------------------------------------------------------------------------
# TARGET   : output name
# SOURCES  : libXenon platform sources (this dir)
# INCLUDES : the VVVVVV game sources + lodepng/tinyxml live in ../src/lodepng/tinyxml
#---------------------------------------------------------------------------------
TARGET   := vvvvvv
BUILD    := build
SOURCES  := source
INCLUDES := ../src ../lodepng ../tinyxml

# Keep -O2 and never use -O3 here: $(MACHDEP) adds -maltivec, and GCC's
# auto-vectorizer (turned on at -O3) emits standard AltiVec/VMX code that the
# Xenon's non-standard VMX128 ISA executes incorrectly, corrupting the
# framebuffer.
CFLAGS   = -g -O2 -fno-tree-vectorize -w $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS) -include $(CURDIR)/../source/global_compat.h

LDFLAGS  = -g $(MACHDEP) -Wl,--gc-sections -Wl,-Map,$(notdir $@).map -Wl,--wrap=fopen -Wl,--wrap=remove

LIBS     := -lxenon -lm
LIBDIRS  :=

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)

# Platform sources live in source/; game sources in ../src.
export VPATH    := $(CURDIR)/source:$(CURDIR)/../src:$(CURDIR)/../src/states:$(CURDIR)/../src/levels:$(CURDIR)/../tinyxml
export DEPSDIR  := $(CURDIR)/$(BUILD)

GAME_SRC := \
	main.o \
	BinaryBlob.o BlockV.o Editor.o Ent.o Entity.o Game.o Graphics.o \
	GraphicsResources.o GraphicsUtil.o Input.o KeyPoll.o Logic.o Map.o \
	Music.o Screen.o Script.o Scripts.o SoundSystem.o TerminalScripts.o \
	Textbox.o Utility.o titlerender.o Preloader.o FileSystemUtils.o \
	Final.o Lab.o Other.o SpaceStation.o Tower.o Warp.o \
	tinystr.o tinyxml.o tinyxmlerror.o tinyxmlparser.o

PLATFORM_SRC := sdlstub.o game_gfx.o gfx.o input.o platform_fs.o platform_file.o platform_misc.o fat32.o

export OFILES   := $(PLATFORM_SRC) $(GAME_SRC)

export INCLUDE  := -I$(CURDIR)/../src -I$(CURDIR)/../src/states -I$(CURDIR)/../src/levels -I$(CURDIR)/../lodepng -I$(CURDIR)/../tinyxml -I$(CURDIR)/source -I$(CURDIR)/$(BUILD) -I$(LIBXENON_INC)
export LIBPATHS := -L$(LIBXENON_LIB)

export LD       := $(CXX)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	+@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).elf32

else
#---------------------------------------------------------------------------------

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).elf32: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
