CFLAGS+=-DNDEBUG -DLINUX -O2 -fno-strict-aliasing -Wall -ffloat-store -pipe -g -MF .depends/$*.d -MMD
ALLOBJ:=$(ALLSRC:.c=.o)

.DELETE_ON_ERROR:

$(ALLOBJ): | .depends
$(ALLOBJ): Makefile ../make.inc

.depends:
	mkdir -p $@

.PHONY: clean distclean

clean:
	-rm -f *.o $(TARGETS)

DEPENDS=$(foreach file, $(ALLSRC:.c=.d), .depends/$(file))

distclean: clean
	-rm -f $(DEPENDS)

-include $(DEPENDS)

