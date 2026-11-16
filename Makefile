.PHONY: all client game ref_gl r1q2ded win32 clean distclean sanitize help

all client game ref_gl r1q2ded win32 clean distclean sanitize help:
	$(MAKE) -C binaries $@

