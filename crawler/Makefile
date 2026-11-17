# Gateway Crawler 3.1.0 - C port of crawler.py
#
# Sources live in src/ (15 modules sharing src/crawler.h) and link into one
# executable. Everything lands in a folder:
#
#   make          native build              -> ./crawler
#   make dist     all three architectures   -> dist/crawler-x86_64
#                                             dist/crawler-arm64
#                                             dist/crawler-armhf
#
# `make dist` installs nothing at all. The compiler (Zig, which includes a full
# C compiler and the glibc/arm sysroots for every target) is downloaded into this
# folder once, and so are the libcurl/zlib/sqlite3 runtime libraries and headers
# for each architecture. Both live inside the project:
#
#   toolchain/zig/        the compiler            (~300 MB, one download)
#   vendor/<arch>/        headers + runtime libs  (~2 MB per architecture)
#
# No root, no apt packages, no dpkg --add-architecture, no ports sources, no ARM
# dev packages, nothing to install on the Pi either: the binaries use the libcurl,
# zlib and sqlite3 that Raspberry Pi OS already ships.
#
# OTHER TARGETS
#   make x86_64       portable 64-bit x86 build
#   make x86_64-v3    modern x86-64 (AVX2) build
#   make arm64        build on the Pi itself, for the Pi
#   make rpi3b        same as dist-arm64 (cross build for 64-bit Pi OS)
#   make rpi3b-32     same as dist-armhf (cross build for 32-bit Pi OS)
#   make no-sqlite    build without SQLite
#   make debug        AddressSanitizer + UBSan build
#   make check        version and --help smoke test
#   make clean        remove binaries, dist/ contents and the build stamp
#   make help         targets, host CPU and cross status
#
# Host dependencies for a native build:
#   Debian/Ubuntu: sudo apt install build-essential libcurl4-openssl-dev zlib1g-dev libsqlite3-dev
#   Fedora/RHEL:   sudo dnf install gcc make libcurl-devel zlib-devel sqlite-devel
#   macOS (brew):  brew install curl sqlite3

CC       ?= cc
# OPT stays a private variable so an inherited CFLAGS cannot strip the -O3 build.
# Override it directly: make OPT="-O2 -march=znver4"
OPT      ?= -O3 -march=native -pipe
CPPFLAGS ?=
EXTRA_CFLAGS ?=
WARN      = -Wall -Wextra

# Folder-local toolchain. `make dist` fetches it on first use.
ZIG      ?= toolchain/zig/zig
ZIG_ENV   = ZIG_GLOBAL_CACHE_DIR=$(CURDIR)/toolchain/zig-cache \
            ZIG_LOCAL_CACHE_DIR=$(CURDIR)/toolchain/zig-local
ZIG_GLIBC = 2.28

# Optional fallback: system cross compilers, only used by the *-gcc targets.
CC_ARM64  ?= aarch64-linux-gnu-gcc
CC_ARMHF  ?= arm-linux-gnueabihf-gcc

HAVE_SQLITE ?= 1
ifeq ($(HAVE_SQLITE),1)
SQLITE_DEF  = -DHAVE_SQLITE3
SQLITE_HOST = -lsqlite3
else
SQLITE_DEF  =
SQLITE_HOST =
endif

STD  = -std=c11
BIN  = crawler
DIST = dist
SRC := $(sort $(wildcard src/*.c))
HDR := $(wildcard src/*.h)
HOST := $(shell uname -m)

CPPFLAGS += -Isrc

# ARM cross builds link against vendor/<arch>/ only: a few hundred KB of
# runtime libraries pulled straight from the archive. --allow-shlib-undefined
# keeps ld quiet about libcurl's own dependencies (libssl and friends), which
# the Pi supplies at run time.
V_ARM64  = vendor/arm64
V_ARMHF  = vendor/armhf
V_X86    = vendor/x86_64
# Linked by path rather than -l so the same flags work for every compiler.
V_LIBS   = $(1)/lib/libcurl.so.4 $(1)/lib/libz.so.1 \
           $(if $(filter 1,$(HAVE_SQLITE)),$(1)/lib/libsqlite3.so.0,)

.PHONY: all native dist dist-x86_64 dist-arm64 dist-armhf dist-gcc-arm64 \
        dist-gcc-armhf toolchain deps x86_64 x86_64-v3 arm64 rpi3b rpi3b-32 \
        no-sqlite debug check clean distclean help FORCE

all: $(BIN)

native: $(BIN)

# Remember the flags of the last build so switching targets always rebuilds
# instead of silently leaving the previous architecture's binary in place.
.buildflags: FORCE
	@echo '$(CC)|$(OPT)|$(CPPFLAGS)|$(HAVE_SQLITE)' > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv $@.tmp $@; else rm -f $@.tmp; fi

$(BIN): $(SRC) $(HDR) .buildflags
	@echo "  CC      $(BIN)  [$(OPT)]  ($(words $(SRC)) modules)"
	@$(CC) $(STD) $(CPPFLAGS) $(SQLITE_DEF) $(OPT) $(EXTRA_CFLAGS) $(WARN) \
	   -o $@ $(SRC) -lcurl -lz $(SQLITE_HOST) -lm

# --- native x86-64 variants ------------------------------------------------
x86_64:
	@case "$(HOST)" in \
	  x86_64|amd64) $(MAKE) -B OPT="-O3 -march=x86-64 -mtune=generic -pipe" $(BIN) ;; \
	  *) echo "error: 'make x86_64' needs an x86-64 host, this is $(HOST)."; \
	     echo "       use 'make' for this machine."; exit 1 ;; \
	esac

x86_64-v3:
	@case "$(HOST)" in \
	  x86_64|amd64) $(MAKE) -B OPT="-O3 -march=x86-64-v3 -mtune=generic -pipe" $(BIN) ;; \
	  *) echo "error: 'make x86_64-v3' needs an x86-64 host, this is $(HOST)."; exit 1 ;; \
	esac

# Build on the Pi itself
arm64:
	@case "$(HOST)" in \
	  aarch64|arm64) $(MAKE) -B OPT="-O3 -march=armv8-a+crc -mtune=generic -pipe" $(BIN) ;; \
	  *) echo "error: 'make arm64' builds on an ARMv8 machine. This is $(HOST)."; \
	     echo "       cross build instead: make dist-arm64"; exit 1 ;; \
	esac

# --- dist: all three architectures in one folder ---------------------------
dist: dist-x86_64 dist-arm64 dist-armhf
	@echo ""
	@echo "built into $(DIST)/:"
	@ls -l $(DIST)
	@echo ""
	@echo "copy the matching one to the Pi, for example:"
	@echo "  scp $(DIST)/crawler-arm64 pi@raspberrypi.local:~/crawler"

# Each of the three is built by the compiler in toolchain/, against the headers
# and runtime libraries in vendor/<arch>/, so no system packages are involved.
dist-x86_64: $(ZIG) $(V_X86)/lib/libcurl.so.4
	@mkdir -p $(DIST)
	@echo "  CC      $(DIST)/crawler-x86_64  [x86-64]"
	@$(ZIG_ENV) $(ZIG) cc $(STD) -Isrc -I$(V_X86)/include $(SQLITE_DEF) \
	   -O3 -target x86_64-linux-gnu.$(ZIG_GLIBC) -Wl,--allow-shlib-undefined \
	   $(WARN) -o $(DIST)/crawler-x86_64 $(SRC) \
	   $(call V_LIBS,$(V_X86)) -lm

dist-arm64: $(ZIG) $(V_ARM64)/lib/libcurl.so.4
	@mkdir -p $(DIST)
	@echo "  CC      $(DIST)/crawler-arm64  [aarch64 / Cortex-A53]"
	@$(ZIG_ENV) $(ZIG) cc $(STD) -Isrc -I$(V_ARM64)/include $(SQLITE_DEF) \
	   -O3 -mcpu=cortex_a53 -target aarch64-linux-gnu.$(ZIG_GLIBC) \
	   -Wl,--allow-shlib-undefined $(WARN) -o $(DIST)/crawler-arm64 $(SRC) \
	   $(call V_LIBS,$(V_ARM64)) -lm

dist-armhf: $(ZIG) $(V_ARMHF)/lib/libcurl.so.4
	@mkdir -p $(DIST)
	@echo "  CC      $(DIST)/crawler-armhf  [armv7 / Cortex-A53 + NEON]"
	@$(ZIG_ENV) $(ZIG) cc $(STD) -Isrc -I$(V_ARMHF)/include $(SQLITE_DEF) \
	   -O3 -mcpu=cortex_a53 -target arm-linux-gnueabihf.$(ZIG_GLIBC) \
	   -Wl,--allow-shlib-undefined $(WARN) -o $(DIST)/crawler-armhf $(SRC) \
	   $(call V_LIBS,$(V_ARMHF)) -lm

# Optional: same builds with a system cross compiler instead of toolchain/zig.
dist-gcc-arm64: $(V_ARM64)/lib/libcurl.so.4
	@mkdir -p $(DIST)
	@$(CC_ARM64) $(STD) -Isrc -I$(V_ARM64)/include $(SQLITE_DEF) -O3 -mcpu=cortex-a53 \
	   $(WARN) -o $(DIST)/crawler-arm64 $(SRC) \
	   -L$(V_ARM64)/lib -Wl,--allow-shlib-undefined $(call V_LIBS,$(V_ARM64)) -lm

dist-gcc-armhf: $(V_ARMHF)/lib/libcurl.so.4
	@mkdir -p $(DIST)
	@$(CC_ARMHF) $(STD) -Isrc -I$(V_ARMHF)/include $(SQLITE_DEF) -O3 -mcpu=cortex-a53 \
	   -Wall -Wextra -o $(DIST)/crawler-armhf $(SRC) \
	   -L$(V_ARMHF)/lib -Wl,--allow-shlib-undefined $(call V_LIBS,$(V_ARMHF)) -lm

# --- everything the builds need, fetched into this folder ------------------
$(ZIG):
	@echo "  DEPS    compiler -> toolchain/zig/  (one time, ~300 MB)"
	@./scripts/fetch-toolchain.sh

$(V_ARM64)/lib/libcurl.so.4:
	@echo "  DEPS    arm64 headers + runtime libraries -> $(V_ARM64)/"
	@./scripts/fetch-target-libs.sh arm64

$(V_ARMHF)/lib/libcurl.so.4:
	@echo "  DEPS    armhf headers + runtime libraries -> $(V_ARMHF)/"
	@./scripts/fetch-target-libs.sh armhf

$(V_X86)/lib/libcurl.so.4:
	@echo "  DEPS    x86_64 headers + runtime libraries -> $(V_X86)/"
	@./scripts/fetch-target-libs.sh x86_64

toolchain: $(ZIG)
deps: $(ZIG) $(V_X86)/lib/libcurl.so.4 $(V_ARM64)/lib/libcurl.so.4 $(V_ARMHF)/lib/libcurl.so.4

rpi3b: dist-arm64
rpi3b-32: dist-armhf

# --- variants -------------------------------------------------------------
no-sqlite:
	$(MAKE) -B HAVE_SQLITE=0 $(BIN)

debug:
	$(CC) $(STD) $(CPPFLAGS) $(SQLITE_DEF) -O1 -g -fno-omit-frame-pointer \
	      -fsanitize=address,undefined $(WARN) -o $(BIN) $(SRC) \
	      -lcurl -lz $(SQLITE_HOST) -lm

check: $(BIN)
	./$(BIN) --version
	./$(BIN) --help >/dev/null && echo "help OK"

clean:
	rm -f $(BIN) .buildflags $(DIST)/*
	-rmdir $(DIST) 2>/dev/null || true

# also drop the downloaded compiler and libraries (~300 MB)
distclean: clean
	@find vendor toolchain -mindepth 1 -delete 2>/dev/null || true
	@rmdir vendor toolchain 2>/dev/null || true
	@echo "removed vendor/ and toolchain/" 

help:
	@echo "host CPU detected: $(HOST)   modules: $(words $(SRC))"
	@echo ""
	@echo "  make            native build -> ./$(BIN)"
	@echo "  make dist       x86_64 + arm64 + armhf -> $(DIST)/  (installs nothing)"
	@echo "  make deps       fetch the compiler and both vendor libraries now"
	@echo "  make dist-arm64 cross build for 64-bit Pi OS ($(DIST)/crawler-arm64)"
	@echo "  make dist-armhf cross build for 32-bit Pi OS ($(DIST)/crawler-armhf)"
	@echo "  make x86_64      portable 64-bit x86 build"
	@echo "  make x86_64-v3   modern x86-64 (AVX2) build"
	@echo "  make arm64       build on the Pi itself"
	@echo "  make no-sqlite   build without SQLite"
	@echo "  make debug       ASan/UBSan build"
	@echo "  make check       smoke test"
	@echo "  make clean       remove binaries and dist/ contents"
	@echo "  make distclean   also remove toolchain/ and vendor/ (~300 MB)"
	@echo ""
	@printf '  compiler:        '
	@if [ -x $(ZIG) ]; then echo "toolchain/zig ($(shell $(ZIG) version 2>/dev/null))"; \
	 else echo "not fetched yet (make deps)"; fi
	@printf '  vendor libs:     '
	@if [ -f $(V_ARM64)/lib/libcurl.so.4 ]; then printf 'arm64 ok  '; else printf 'arm64 -  '; fi
	@if [ -f $(V_ARMHF)/lib/libcurl.so.4 ]; then printf 'armhf ok  '; else printf 'armhf -  '; fi
	@if [ -f $(V_X86)/lib/libcurl.so.4 ]; then echo 'x86_64 ok'; else echo 'x86_64 -'; fi
	@echo ""
	@echo "  make dist downloads anything missing into this folder; nothing is installed."
