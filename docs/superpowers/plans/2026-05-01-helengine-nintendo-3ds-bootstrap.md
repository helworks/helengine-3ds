# Helengine Nintendo 3DS Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first Docker-only Nintendo 3DS host that compiles to `build/helengine_3ds.3dsx` and boots in Citra or Lime3DS with a green top screen and cyan bottom screen.

**Architecture:** The 3DS project stays intentionally small: one devkitPro-based Docker image, one `Makefile`, one tiny `main.cpp`, and one `Nintendo3DsBootHost` class that owns libctru plus citro3d plus citro2d initialization and the infinite verification frame loop. The build exposes a reserved generated-core root variable now, but phase 1 does not compile generated code or add fake engine integration.

**Tech Stack:** Docker, devkitPro, devkitARM, libctru, citro3d, citro2d, C++17, GNU Make.

---

## File Structure

- `Dockerfile`
  Responsible for provisioning the Linux-based devkitPro Nintendo 3DS build environment and dropping the container into `/workspace`.
- `Makefile`
  Responsible for compiling the Nintendo 3DS sources, linking the executable, packaging the `.3dsx`, and exposing the future generated-core root variable.
- `README.md`
  Responsible for documenting the Docker build flow, the output artifact, and the Citra/Lime3DS verification steps.
- `src/main.cpp`
  Responsible only for constructing `Nintendo3DsBootHost`, calling `Run()`, and returning the exit code.
- `src/platform/3ds/Nintendo3DsBootHost.hpp`
  Responsible for declaring the thin Nintendo 3DS bootstrap boundary and its native renderer state.
- `src/platform/3ds/Nintendo3DsBootHost.cpp`
  Responsible for libctru startup, citro3d and citro2d initialization, screen-target creation, and the infinite frame loop that clears the top screen to green and the bottom screen to cyan.

### Task 1: Docker build environment and toolchain inspection

**Files:**
- Create: `Dockerfile`
- Create: `README.md`

- [ ] **Step 1: Create the Docker image definition**

```dockerfile
FROM devkitpro/devkitarm:latest

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=${DEVKITPRO}/devkitARM
ENV PORTLIBS=${DEVKITPRO}/portlibs/3ds
ENV PATH=${DEVKITARM}/bin:${DEVKITPRO}/tools/bin:${PATH}

RUN dkp-pacman -Syu --noconfirm \
    && dkp-pacman -S --noconfirm 3ds-dev

WORKDIR /workspace
CMD ["/bin/bash"]
```

- [ ] **Step 2: Build the image**

Run:

```bash
rtk docker build -t helengine-3ds .
```

Expected:

```text
The build completes successfully and produces a local image tagged helengine-3ds.
```

- [ ] **Step 3: Inspect the 3DS toolchain layout inside the container**

Run:

```bash
rtk docker run --rm helengine-3ds sh -lc 'find /opt/devkitpro \( -name 3ds_rules -o -name libctru.a -o -name libcitro2d.a -o -name libcitro3d.a \) | sort'
```

Expected:

```text
The output includes the current 3ds_rules path plus the libctru, libcitro2d, and libcitro3d static libraries.
```

- [ ] **Step 4: Create the initial README focused on the first milestone**

````markdown
# Helengine Nintendo 3DS Host

This repository contains the native Nintendo 3DS host scaffold for Helengine.

## Current milestone

- Docker-only build using devkitPro Nintendo 3DS tooling
- Native `.3dsx` output for direct loading in Citra or Lime3DS
- First boot check with a green top screen and cyan bottom screen
- Real citro2d and citro3d renderer startup for later 3D rendering work

## Build

```bash
docker build -t helengine-3ds .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

The build emits `build/helengine_3ds.3dsx`.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/helengine_3ds.3dsx` in Citra or Lime3DS. The expected result for this milestone is a green top screen and a cyan bottom screen with no immediate crash or reset loop.
````

- [ ] **Step 5: Commit the Docker/bootstrap documentation**

Run:

```bash
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds add Dockerfile README.md
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds commit -m "Add Nintendo 3DS Docker build scaffold"
```

Expected:

```text
A commit is created with the Docker image and first-pass README.
```

### Task 2: Native build script and entrypoint seam

**Files:**
- Create: `Makefile`
- Create: `src/main.cpp`

- [ ] **Step 1: Create the Nintendo 3DS build script with a reserved generated-core variable**

```makefile
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB ?= $(DEVKITPRO)/libctru
PORTLIBS ?= $(DEVKITPRO)/portlibs/3ds
HELENGINE_CORE_CPP_ROOT ?=

include $(DEVKITARM)/3ds_rules

TARGET := helengine_3ds
BUILD := build
SOURCES := \
	src \
	src/platform/3ds
INCLUDES := \
	src

CFLAGS := \
	-g \
	-O2 \
	-Wall \
	-Wextra \
	-ffunction-sections \
	-fdata-sections

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CFLAGS += -DHELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE=0
else
CFLAGS += -DHELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif

CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

LIBS := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(BUILD)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) \
	$(CFILES:.c=.o) \
	$(SFILES:.s=.o) \
	$(BINFILES:.bin=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

%.o: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
```

- [ ] **Step 2: Create the tiny native entrypoint**

```cpp
#include "platform/3ds/Nintendo3DsBootHost.hpp"

int main() {
    helengine::nintendo3ds::Nintendo3DsBootHost host;
    return host.Run();
}
```

- [ ] **Step 3: Build the project before the host implementation exists**

Run:

```bash
rtk docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

Expected:

```text
FAIL because src/platform/3ds/Nintendo3DsBootHost.hpp and .cpp do not exist yet.
```

- [ ] **Step 4: Commit the build-script seam**

Run:

```bash
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds add Makefile src/main.cpp
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds commit -m "Add Nintendo 3DS native build entrypoint"
```

Expected:

```text
A commit is created with the Makefile and thin launcher.
```

### Task 3: Nintendo 3DS renderer bootstrap and verification host

**Files:**
- Create: `src/platform/3ds/Nintendo3DsBootHost.hpp`
- Create: `src/platform/3ds/Nintendo3DsBootHost.cpp`

- [ ] **Step 1: Create the host declaration with explicit renderer ownership**

```cpp
#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

namespace helengine::nintendo3ds {
    /// Owns the first Nintendo 3DS renderer bootstrap and verification frame loop.
    class Nintendo3DsBootHost {
    public:
        /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
        Nintendo3DsBootHost();

        /// Initializes the Nintendo 3DS renderer stack and presents the verification colors until shutdown.
        int Run();

    private:
        /// Stores the top-screen clear color.
        static constexpr u32 TopScreenColor = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);

        /// Stores the bottom-screen clear color.
        static constexpr u32 BottomScreenColor = C2D_Color32(0x00, 0xFF, 0xFF, 0xFF);

        /// Stores the top-screen render target.
        C3D_RenderTarget* TopTarget;

        /// Stores the bottom-screen render target.
        C3D_RenderTarget* BottomTarget;

        /// Initializes libctru, citro3d, citro2d, and the screen render targets.
        bool InitializeRenderer();

        /// Draws one verification frame to both visible screens.
        void PresentFrame();

        /// Shuts down the renderer stack in reverse initialization order.
        void ShutdownRenderer();
    };
}
```

- [ ] **Step 2: Create the host implementation that clears both screens every frame**

```cpp
#include "platform/3ds/Nintendo3DsBootHost.hpp"

namespace helengine::nintendo3ds {
    /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
    Nintendo3DsBootHost::Nintendo3DsBootHost()
        : TopTarget(nullptr)
        , BottomTarget(nullptr) {
    }

    /// Initializes the Nintendo 3DS renderer stack and presents the verification colors until shutdown.
    int Nintendo3DsBootHost::Run() {
        if (!InitializeRenderer()) {
            ShutdownRenderer();
            return 1;
        }

        while (aptMainLoop()) {
            hidScanInput();
            PresentFrame();
        }

        ShutdownRenderer();
        return 0;
    }

    /// Initializes libctru, citro3d, citro2d, and the screen render targets.
    bool Nintendo3DsBootHost::InitializeRenderer() {
        gfxInitDefault();

        if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
            return false;
        }

        if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
            return false;
        }

        C2D_Prepare();

        TopTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        BottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        if (TopTarget == nullptr) {
            return false;
        } else if (BottomTarget == nullptr) {
            return false;
        }

        return true;
    }

    /// Draws one verification frame to both visible screens.
    void Nintendo3DsBootHost::PresentFrame() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(TopTarget, TopScreenColor);
        C2D_SceneBegin(TopTarget);

        C2D_TargetClear(BottomTarget, BottomScreenColor);
        C2D_SceneBegin(BottomTarget);

        C3D_FrameEnd(0);
    }

    /// Shuts down the renderer stack in reverse initialization order.
    void Nintendo3DsBootHost::ShutdownRenderer() {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
    }
}
```

- [ ] **Step 3: Build the `.3dsx` artifact and verify the Dockerized build passes**

Run:

```bash
rtk docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

Expected:

```text
The build succeeds and produces build/helengine_3ds.3dsx.
```

- [ ] **Step 4: Verify the manual emulator boot target**

Run:

```bash
Open build/helengine_3ds.3dsx in Citra or Lime3DS.
```

Expected:

```text
The top screen is solid green, the bottom screen is solid cyan, and the process remains stable.
```

- [ ] **Step 5: Commit the first 3DS host bootstrap**

Run:

```bash
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds add src/platform/3ds/Nintendo3DsBootHost.hpp src/platform/3ds/Nintendo3DsBootHost.cpp
rtk git -c safe.directory=/mnt/c/dev/helworks/helengine-3ds commit -m "Add Nintendo 3DS boot host scaffold"
```

Expected:

```text
A commit is created with the first 3DS renderer bootstrap host.
```
