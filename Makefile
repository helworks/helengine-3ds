DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB ?= $(DEVKITPRO)/libctru
HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_3DS_ROMFS_ROOT ?=
_3DSXFLAGS :=
TOPDIR ?= $(CURDIR)

ifneq ($(strip $(HELENGINE_3DS_ROMFS_ROOT)),)
ROMFS_ROOT := $(abspath $(HELENGINE_3DS_ROMFS_ROOT))
$(if $(wildcard $(ROMFS_ROOT)),,$(error HELENGINE_3DS_ROMFS_ROOT '$(HELENGINE_3DS_ROMFS_ROOT)' does not exist))
_3DSXFLAGS += --romfs=$(ROMFS_ROOT)
endif

export _3DSXFLAGS

include $(DEVKITARM)/3ds_rules

TARGET := helengine_3ds
BUILD := build
SOURCES := \
	src \
	src/platform/3ds
INCLUDES := \
	src
GENERATED_CORE_SOURCE_DIRS :=
GENERATED_CORE_CPPFILES :=
GENERATED_CORE_TRANSLATION_UNIT :=
_3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := \
	-g \
	-O2 \
	-Wall \
	-Wextra \
	-mword-relocations \
	-fomit-frame-pointer \
	-ffunction-sections \
	-fdata-sections \
	$(ARCH)

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CFLAGS += -DHELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE=0
else
$(if $(wildcard $(HELENGINE_CORE_CPP_ROOT)/helcpp_config.hpp),,$(error HELENGINE_CORE_CPP_ROOT '$(HELENGINE_CORE_CPP_ROOT)' does not contain helcpp_config.hpp))
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_amalgamated.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_amalgamated.cpp
else ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_unity.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_unity.cpp
else
$(error HELENGINE_CORE_CPP_ROOT '$(HELENGINE_CORE_CPP_ROOT)' does not contain helengine_core_amalgamated.cpp or helengine_core_unity.cpp)
endif
$(if $(wildcard $(HELENGINE_CORE_CPP_ROOT)/runtime/runtime_startup_manifest.cpp),,$(error HELENGINE_CORE_CPP_ROOT '$(HELENGINE_CORE_CPP_ROOT)' does not contain runtime/runtime_startup_manifest.cpp))
$(if $(wildcard $(HELENGINE_CORE_CPP_ROOT)/runtime/runtime_standard_platform_input_manifest.cpp),,$(error HELENGINE_CORE_CPP_ROOT '$(HELENGINE_CORE_CPP_ROOT)' does not contain runtime/runtime_standard_platform_input_manifest.cpp))
CFLAGS += -DHELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
GENERATED_CORE_SOURCE_DIRS := $(HELENGINE_CORE_CPP_ROOT) $(HELENGINE_CORE_CPP_ROOT)/runtime
GENERATED_CORE_CPPFILES := $(GENERATED_CORE_TRANSLATION_UNIT) runtime_startup_manifest.cpp
ifneq ($(GENERATED_CORE_TRANSLATION_UNIT),helengine_core_unity.cpp)
GENERATED_CORE_CPPFILES += runtime_standard_platform_input_manifest.cpp
endif
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/runtime/runtime_scene_catalog_manifest.cpp),)
GENERATED_CORE_CPPFILES += runtime_scene_catalog_manifest.cpp
endif
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/runtime/runtime_code_module_manifest.cpp),)
GENERATED_CORE_CPPFILES += runtime_code_module_manifest.cpp
endif
endif

CFLAGS += $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -std=gnu++20
ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CXXFLAGS += -fno-rtti
CXXFLAGS += -fno-exceptions
endif
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB)
_3DSXDEPS := $(OUTPUT).smdh

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(BUILD)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(GENERATED_CORE_SOURCE_DIRS)
export DEPSDIR := $(CURDIR)/$(BUILD)

CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
CPPFILES += $(GENERATED_CORE_CPPFILES)
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) \
	$(CFILES:.c=.o) \
	$(SFILES:.s=.o)
export OFILES_BIN := $(BINFILES:.bin=.o) \
	$(PICAFILES:.v.pica=.shbin.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(PICAFILES:.v.pica=_shbin.h) \
	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS

.PHONY: all clean

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf: $(OFILES)

%.o: %.bin
	@echo $(notdir $<)
	@$(bin2o)

%.shbin.o %_shbin.h : %.shbin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
