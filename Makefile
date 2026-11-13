.PHONY: all client game ref_gl r1q2ded clean distclean combine sanitize help

all client game ref_gl r1q2ded clean distclean combine sanitize help:
	$(MAKE) -C binaries $@

