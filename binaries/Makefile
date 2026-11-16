SANITIZE_DIR ?= /tmp/opencode/r1q2-sanitize
SANITIZE_CC ?= gcc
SANITIZE_FLAGS ?= -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: default all r1q2ded game client ref_gl win32 clean distclean sanitize help

default: all

all: r1q2ded client game ref_gl

win32:
	$(MAKE) -f Makefile.win32 all

r1q2ded:
	$(MAKE) -C r1q2ded

client:
	$(MAKE) -C client

game:
	$(MAKE) -C game

ref_gl:
	$(MAKE) -C ref_gl

clean:
	$(MAKE) -C r1q2ded clean
	$(MAKE) -C client clean
	$(MAKE) -C game clean
	$(MAKE) -C ref_gl clean

distclean:
	$(MAKE) -C r1q2ded distclean
	$(MAKE) -C client distclean
	$(MAKE) -C game distclean
	$(MAKE) -C ref_gl distclean

sanitize:
	@set -eu; \
	src="$(abspath ..)"; dst="$(SANITIZE_DIR)"; \
	case "$$dst" in /tmp/opencode/*) ;; *) \
		echo "SANITIZE_DIR must be under /tmp/opencode"; exit 1 ;; \
	esac; \
	mkdir -p "$$dst"; \
	cp -a "$$src/binaries" "$$src/client" "$$src/game" "$$src/linux" \
		"$$src/qcommon" "$$src/ref_gl" "$$src/server" "$$src/build.h" "$$dst/"; \
	$(MAKE) -C "$$dst/binaries" distclean; \
	$(MAKE) -C "$$dst/binaries" CC="$(SANITIZE_CC) $(SANITIZE_FLAGS)"; \
	printf 'Sanitizer build complete: %s\n' "$$dst"

help:
	@printf '%s\n' \
		'make [all]             Build client, dedicated server, game and renderer' \
		'make win32             Cross-compile 32-bit Windows binaries into binaries/win32' \
		'make clean             Remove objects and binaries' \
		'make distclean         Also remove generated dependencies' \
		'make sanitize          Build ASan/UBSan in /tmp/opencode' \
		'make USE_CURL=0        Build without HTTP/libcurl (run distclean first)' \
		'make help              Show this help'
