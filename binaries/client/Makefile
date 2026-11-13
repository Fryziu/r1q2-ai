VPATH=../../game ../../linux ../../qcommon ../../server ../../client	

PKG_CONFIG ?= pkg-config
SDL2_CONFIG ?= sdl2-config
USE_CURL ?= 1

ifneq ($(filter $(USE_CURL),0 1),$(USE_CURL))
$(error USE_CURL must be 0 or 1)
endif

quake2_SRC:=cl_cin.c cl_ents.c cl_fx.c cl_input.c cl_inv.c cl_main.c\
	    cl_parse.c cl_pred.c cl_tent.c cl_scrn.c cl_view.c cl_newfx.c\
	    console.c keys.c menu.c snd_dma.c snd_mem.c snd_mix.c qmenu.c\
	    m_flash.c\
	    cmd.c cmodel.c common.c crc.c cvar.c files.c md4.c net_chan.c\
	    sv_ccmds.c sv_ents.c sv_game.c sv_init.c sv_main.c sv_send.c\
	    sv_user.c sv_world.c \
	    q_shlinux.c vid_menu.c vid_so.c sys_linux.c glob.c net_udp.c\
	    q_shared.c pmove.c mersennetwister.c le_util.c\
	    le_physics.c redblack.c cd_linux.c snd_sdl2.c unzip.c ioapi.c
#	    al_linux.c qal_linux.c

# Run distclean after changing USE_CURL.
ifeq ($(USE_CURL),1)
quake2_SRC += cl_http.c
CURL_CFLAGS = $(shell $(PKG_CONFIG) --cflags libcurl) -DUSE_CURL
CURL_LIBS = $(shell $(PKG_CONFIG) --libs libcurl)
CFLAGS += $(CURL_CFLAGS)
endif

CFLAGS+=$(shell $(SDL2_CONFIG) --cflags)

#CFLAGS+=-DUSE_OPENAL
quake2_OBJ:=$(quake2_SRC:.c=.o)
ALLSRC:=$(quake2_SRC)

.PHONY: default combine

default: quake2

TARGETS:=quake2 quake2-combine

include ../make.inc

LDFLAGS+=$(shell $(SDL2_CONFIG) --libs) $(CURL_LIBS) -lm -lz

ifeq ($(shell uname),Linux)
LDFLAGS+=-ldl
endif

ifeq ($(shell uname),FreeBSD)
LDFLAGS+=-lexecinfo
endif


quake2: $(quake2_OBJ)
	$(CC) -g -o $@ $^ $(LDFLAGS)

# Legacy GCC --combine target; not used by the normal build.
combine: quake2-combine
quake2-combine: $(quake2_SRC)
	$(CC) $(shell $(SDL2_CONFIG) --cflags) $(CURL_CFLAGS) -O2 -ffloat-store -DNDEBUG -DLINUX -fwhole-program --combine -g -o $@ $^ $(LDFLAGS)
