VPATH=../../ref_gl ../../linux ../../game

PKG_CONFIG ?= pkg-config
SDL2_CONFIG ?= sdl2-config

CFLAGS+=-fPIC $(shell $(SDL2_CONFIG) --cflags) $(shell $(PKG_CONFIG) --cflags libpng libjpeg)

ref_gl_SRC:=gl_draw.c gl_image.c gl_light.c gl_mesh.c gl_model.c gl_rmain.c\
			gl_rmisc.c gl_rsurf.c gl_warp.c gl_sdl2.c glob.c q_shared.c\
			q_shlinux.c qgl_linux.c

ref_gl_OBJ:=$(ref_gl_SRC:.c=.o)
ALLSRC:=$(ref_gl_SRC)

.PHONY: default combine

default: ref_gl.so

TARGETS:=ref_gl.so ref_gl-combine.so

include ../make.inc

LDFLAGS+=$(shell $(SDL2_CONFIG) --libs) -lm -lGL $(shell $(PKG_CONFIG) --libs libpng libjpeg)

ref_gl.so: $(ref_gl_OBJ)
	$(CC) -shared -g -o $@ $^ $(LDFLAGS)

# Legacy GCC --combine target; not used by the normal build.
combine: ref_gl-combine.so
ref_gl-combine.so: $(ref_gl_SRC)
	$(CC) $(shell $(SDL2_CONFIG) --cflags) -shared -O2 -ffloat-store -DNDEBUG -DLINUX -fPIC -fwhole-program --combine -g -o $@ $^ $(LDFLAGS)
