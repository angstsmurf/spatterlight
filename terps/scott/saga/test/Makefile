# Fast byte-exact harness for the Saga/Scott Apple II + Atari 8-bit vector
# renderers (apple2_flood_fill / flood_fill). Mirrors -[SpatterlightTests
# testScottDraw]: loads the drawing tables from MI.dsk, renders mi_boom /
# so_console / so_painting on each platform and byte-compares the full screen
# buffer(s) against the saved .result fixtures.
#
#   make test    build and run against the SpatterlightTests fixtures
#   make clean

CC      ?= cc
SCOTT   := ../..
TERPS   := $(SCOTT)/..
CFLAGS  := -std=c11 -g -O0 -w -DSPATTERLIGHT \
           -I$(SCOTT) -I$(SCOTT)/saga -I$(SCOTT)/ai_uk \
           -I$(TERPS)/common_utils -I$(TERPS)/common_sagadraw \
           -I$(TERPS)/readwoz -I$(TERPS)/aiukgraphics -I$(TERPS)/../glkimp

SRCS := filltest.c headless_glk_stubs.c \
        $(SCOTT)/saga/apple2_vector_draw.c \
        $(TERPS)/common_sagadraw/gm_vector.c \
        $(SCOTT)/saga/atari_8bit_vector_draw.c \
        $(SCOTT)/saga/vector_common.c \
        $(SCOTT)/saga/apple2detect.c \
        $(TERPS)/readwoz/ciderpress.c \
        $(TERPS)/common_utils/memory_allocation.c \
        $(TERPS)/common_utils/common_file_utils.c \
        $(TERPS)/common_utils/companion_search.c

FIXTURES := $(TERPS)/../SpatterlightTests/Supporting Files/

# ---- Shared recipe pieces ----------------------------------------------------
# The golden runners below loop over a $(wildcard) of goldens, run one shell
# command per golden ($$g) and report a one-line verdict:
#   $(call no_goldens,LABEL,GOLDENS)        stop (successfully) when there are none
#   $(call each_golden,GOLDENS,COMMAND)     run COMMAND for every $$g, noting failures
#   $(call goldens_verdict,LABEL[,PASSED])  "LABEL: PASSED" or "LABEL: FAILURES" (exit 1)
# run_goldens strings the last two together. Arguments must not contain commas.
no_goldens = [ -n "$(2)" ] || { echo "$(1): no goldens yet"; exit 0; };
each_golden = for g in $(1); do $(2) || fail=1; done;
goldens_verdict = if [ $$fail -eq 0 ]; then echo "$(1): $(or $(2),all goldens byte-exact)"; \
  else echo "$(1): FAILURES"; exit 1; fi
define run_goldens
fail=0; $(call each_golden,$(2),$(3)) $(call goldens_verdict,$(1),$(4))
endef

# $(call compile_each,SOURCES,OBJDIR,FLAGS): compile every C source to
# OBJDIR/<its path with / and . turned into _>.o, leaving the object list in the
# shell variable objs (for a link command in the same recipe line).
compile_each = objs=""; for f in $(1); do \
  o="$(2)/$$(echo $$f | tr '/.' '__').o"; \
  $(CC) $(3) -c "$$f" -o "$$o" || exit 1; \
  objs="$$objs $$o"; done;

build/filltest: $(SRCS) filltest.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: build/filltest
	./build/filltest "$(FIXTURES)"

# Same as `test`, plus the Apple II and Atari 8-bit ground-truth corpora.
groundtruth: build/filltest
	./build/filltest "$(FIXTURES)" ./groundtruth ./groundtruth_atari

# ---- DOS (PC/CGA) SAGA bitmap renderer ---------------------------------------
# Byte-exact harness for common_sagadraw/pcdraw.c (DrawDOSImageFromData), the
# IBM-PC CGA RLE bitmap decoder. Each room's <name>.PAK (the raw bytes the
# interpreter feeds the renderer) is rendered into a CGA-resolution slot grid and
# compared against <name>.cga — a golden decoded from a real DOSBox CGA
# screenshot of that room (see dos_decode_png.py + groundtruth_dos/README.md).
# Goldens line up at a constant +24px (pcdraw's Spatterlight window margin).
DOS_SRCS := dostest.c \
            $(SCOTT)/../common_sagadraw/pcdraw.c \
            $(SCOTT)/../common_utils/read_le16.c
DOS_GOLDENS := $(wildcard groundtruth_dos/*/*.cga)

build/dostest: $(DOS_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(DOS_SRCS)

dostest: build/dostest
	@$(call run_goldens,DOS,$(DOS_GOLDENS),{ \
	  pak="$${g%.cga}.PAK"; \
	  printf '%-28s ' "$$(basename $$(dirname $$g))/$$(basename $$pak)"; \
	  ./build/dostest cmp "$$pak" "$$g"; })

# Extractor for C64 SAGA disks that ship separate R### image files (not the
# packed/decrunch games). Built on demand; not part of the default test run.
build/c64extract: c64extract.c $(TERPS)/c64diskimage/c64diskimage.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(TERPS)/c64diskimage -o $@ c64extract.c $(TERPS)/c64diskimage/c64diskimage.c

# ---- Apple II (PC/hi-res) SAGA bitmap renderer -------------------------------
# common_sagadraw/apple2draw.c:DrawApple2ImageFromData decodes a blob into
# `screenmem` (the native $2000 hi-res page); compared byte-for-byte against a
# golden $2000 dump captured under MAME (approach A, like the vector apple2
# test). Inputs come from build/extract_images (see below).
A2_SRCS := apple2test.c headless_glk_stubs.c \
           $(SCOTT)/../common_sagadraw/apple2draw.c \
           $(SCOTT)/../common_sagadraw/gm_artifact.c \
           $(SCOTT)/../common_sagadraw/gm_vector.c \
           $(SCOTT)/../common_utils/memory_allocation.c \
           $(SCOTT)/../common_utils/read_le16.c
A2_GOLDENS := $(wildcard groundtruth_apple2/*/*.page)

build/apple2test: $(A2_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -include stddef.h -o $@ $(A2_SRCS)

apple2test: build/apple2test
	@$(call run_goldens,Apple II,$(A2_GOLDENS),{ \
	  d=$$(dirname $$g); dat="$${g%.page}.dat"; ds="$$d/descrambletable.bin"; \
	  [ -f "$$ds" ] && dsarg="$$ds" || dsarg=""; \
	  ./build/apple2test cmp "$$dat" "$$g" $$dsarg; })

# ---- Headless image-blob dumper (the keystone for the packed/decrunch games) --
# Runs the real interpreter's DetectGame loading pipeline and dumps every
# USImage->imagedata (+ the Apple II descramble table). Excludes the C64 unp64
# (C++) depacker and uses cheapglk + a few glkimp stubs (extract_stubs.c). Build
# on demand: `make build/extract_images`; run `./build/extract_images <game> <outdir>`.
CG := $(TERPS)/cheapglk
CGOBJ := $(CG)/cgblorb.o $(CG)/cgdate.o $(CG)/cgfref.o $(CG)/cggestal.o $(CG)/cgmisc.o \
         $(CG)/cgschan.o $(CG)/cgstream.o $(CG)/cgstyle.o $(CG)/cgunicod.o $(CG)/cgwindow.o \
         $(CG)/gi_blorb.o $(CG)/gi_debug.o $(CG)/gi_dispa.o
EXTRACT_SRCS := extract_images.c extract_stubs.c \
  $(SCOTT)/detect_game.c $(SCOTT)/scott.c $(SCOTT)/parser.c $(SCOTT)/restore_state.c \
  $(SCOTT)/scott_actions.c $(SCOTT)/scott_display.c $(SCOTT)/scott_game_info.c \
  $(SCOTT)/scott_meta.c $(SCOTT)/title_image.c \
  $(SCOTT)/ai_uk/decompress_text.c $(SCOTT)/ai_uk/game_specific.c $(SCOTT)/ai_uk/gremlins.c \
  $(SCOTT)/ai_uk/hulk.c $(SCOTT)/ai_uk/line_drawing.c $(SCOTT)/ai_uk/ringbuffer.c \
  $(SCOTT)/ai_uk/robin_of_sherwood.c $(SCOTT)/ai_uk/sagadraw.c $(SCOTT)/ai_uk/seas_of_blood.c \
  $(SCOTT)/saga/apple2_vector_draw.c $(SCOTT)/saga/apple2detect.c $(SCOTT)/saga/atari8detect.c \
  $(SCOTT)/saga/atari_8bit_vector_draw.c $(SCOTT)/saga/c64_small.c $(SCOTT)/saga/c64a8scott.c \
  $(SCOTT)/saga/saga.c $(SCOTT)/saga/sagagraphics.c $(SCOTT)/saga/vector_common.c \
  $(SCOTT)/ti994a/load_ti99_4a.c $(SCOTT)/ti994a/rtpi_graphics.c $(SCOTT)/ti994a/ti99_4a_terp.c \
  $(SCOTT)/ti994a/unzip_in_mem.c \
  $(TERPS)/common_sagadraw/apple2draw.c $(TERPS)/common_sagadraw/c64a8draw.c \
  $(TERPS)/common_sagadraw/pcdraw.c $(TERPS)/common_sagadraw/gm_artifact.c \
  $(TERPS)/common_sagadraw/gm_vector.c \
  $(TERPS)/common_utils/memory_allocation.c $(TERPS)/common_utils/common_file_utils.c \
  $(TERPS)/common_utils/read_le16.c $(TERPS)/common_utils/companion_search.c \
  $(TERPS)/aiukgraphics/irmak.c $(TERPS)/aiukgraphics/palette.c \
  $(TERPS)/readwoz/ciderpress.c $(TERPS)/readwoz/woz2nib.c \
  $(TERPS)/decompressz80/decompressz80.c $(TERPS)/common_zxscreen/zx_title.c
EXTRACT_INC := -I$(SCOTT) -I$(SCOTT)/ai_uk -I$(SCOTT)/saga -I$(SCOTT)/ti994a \
  -I$(TERPS)/common_utils -I$(TERPS)/common_sagadraw -I$(TERPS)/aiukgraphics \
  -I$(TERPS)/readwoz -I$(TERPS)/c64diskimage -I$(TERPS)/decompressz80 -I$(TERPS)/common_zxscreen -I$(TERPS)/../glkimp -I$(CG)
EXTRACT_CFLAGS := -std=c11 -g -O0 -w -DSPATTERLIGHT -include stddef.h $(EXTRACT_INC)
# extract_stubs.c pulls in the glkimp stand-ins shared with the headless builds.
STUBS_DEPS := extract_stubs.c $(TERPS)/common_imagetest/glkimp_stubs.c

build/extract_images: $(EXTRACT_SRCS) $(STUBS_DEPS)
	@mkdir -p build
	$(CC) $(EXTRACT_CFLAGS) $(EXTRACT_SRCS) $(CGOBJ) -lz -o $@

# ZX Spectrum Irmak-graphics state dumper (same loader object set as
# extract_images, swapping in zxextract.c). Dumps tiles.bin + meta.txt +
# pic<NNN>.dat for a ZX .z80/.tzx game, on demand (`make build/zxextract`).
ZXEXTRACT_SRCS := $(filter-out extract_images.c,$(EXTRACT_SRCS)) zxextract.c
build/zxextract: $(ZXEXTRACT_SRCS) $(STUBS_DEPS)
	@mkdir -p build
	$(CC) $(EXTRACT_CFLAGS) $(ZXEXTRACT_SRCS) $(CGOBJ) -lz -o $@

# ---- TI-99/4A RTPI bitmap renderer -------------------------------------------
# ti994a/rtpi_graphics.c decodes a GROM image bytecode stream into the native
# VDP pattern + colour tables (vdp_pixels / vdp_colors). titest.c unity-includes
# rtpi_graphics.c to reach those file-static buffers, so it is dropped from the
# link list here (otherwise a duplicate definition). Everything else is the same
# object set as extract_images, so DetectGame loads the real cartridge.
# Goldens are VDP VRAM dumps captured under MAME (approach A).
TI_SRCS := titest.c extract_stubs.c \
  $(filter-out extract_images.c extract_stubs.c $(SCOTT)/ti994a/rtpi_graphics.c,$(EXTRACT_SRCS))
# One golden per image: <name>.pgrid (hardware picture) beside <name>.dat (raw
# blob); font.bin sits in the same per-game dir. <name> is ti994a_u<usage>_i<index>.
TI_GOLDENS := $(wildcard groundtruth_ti99/*/*.pgrid)

build/titest: $(TI_SRCS) $(STUBS_DEPS)
	@mkdir -p build
	$(CC) $(EXTRACT_CFLAGS) $(TI_SRCS) $(CGOBJ) -lz -o $@

titest: build/titest
	@$(call no_goldens,TI-99,$(TI_GOLDENS)) \
	$(call run_goldens,TI-99,$(TI_GOLDENS),{ \
	  d=$$(dirname $$g); base=$$(basename $$g .pgrid); \
	  u=$$(echo $$base | sed -E 's/.*_u([0-9]+)_i.*/\1/'); \
	  i=$$(echo $$base | sed -E 's/.*_i0*([0-9]+)$$/\1/'); \
	  ./build/titest cmp "$$d/font.bin" "$$d/$$base.dat" "$$u" "$$i" "$$g"; })

# ---- C64 "tiny" (mini-C64) bitmap renderer -----------------------------------
# saga/c64_small.c:DrawMiniC64ImageFromData decodes the compact RLE multicolor
# bitmaps used by the early Scott Adams C64 games (Pirate Adventure, Voodoo
# Castle), which also draw item objects as monochrome C64 sprites. c64test.c
# unity-includes c64_small.c to reach those file-static routines and composites a
# room (background bitmap + object overlays + sprites) into a 320x200 displayed-
# colour grid, byte-compared against a golden decoded from a real VICE screenshot
# (approach B; see c64_decode_png.py + groundtruth_c64/README.md). A render spec
# (<name>.spec) lists the background blob and the objects present.
#
# These games pack their images, so the blobs come from extract_images_c64 — the
# same loader as extract_images but with the real C64 decrunch pipeline
# (ai_uk/c64decrunch.c + the C++ unp64 depacker) instead of the DetectC64 stub.
UNP64 := $(TERPS)/unp64
UNP64_SRCS := $(wildcard $(UNP64)/*.cpp) $(wildcard $(UNP64)/scanners/*.cpp)
# extract_images_c64: swap the DetectC64 stub for the real decrunch pipeline.
# extract_stubs.c is shared with the other dumpers; -DSAGA_REAL_C64_DECRUNCH
# drops its DetectC64 stub so the real one (c64decrunch.c) links instead.
C64X_CSRCS := $(SCOTT)/ai_uk/c64decrunch.c \
  $(TERPS)/c64diskimage/c64diskimage.c \
  $(EXTRACT_SRCS)
C64_GOLDENS := $(wildcard groundtruth_c64/*/*.c64)

build/extract_images_c64: $(C64X_CSRCS) $(UNP64_SRCS) $(STUBS_DEPS)
	@mkdir -p build/unp64 build/c64obj
	@echo "compiling unp64 (C++)..."; for f in $(UNP64_SRCS); do \
	  c++ -std=c++14 -g -O0 -w -I$(UNP64) -c "$$f" -o "build/unp64/$$(basename $$f .cpp).o" || exit 1; done
	@echo "compiling C64 loader (C)..."; \
	$(call compile_each,$(C64X_CSRCS),build/c64obj,$(EXTRACT_CFLAGS) -DSAGA_REAL_C64_DECRUNCH -I$(UNP64)) \
	c++ -g -O0 $$objs build/unp64/*.o $(CGOBJ) -lz -o $@

build/c64test: c64test.c $(SCOTT)/saga/c64_small.c
	@mkdir -p build
	$(CC) $(EXTRACT_CFLAGS) c64test.c -lz -o $@

c64test: build/c64test
	@$(call no_goldens,C64,$(C64_GOLDENS)) \
	$(call run_goldens,C64,$(C64_GOLDENS),{ \
	  spec="$${g%.c64}.spec"; \
	  printf '%-36s ' "$$(basename $$(dirname $$g))/$$(basename $$spec)"; \
	  ./build/c64test cmp "$$spec" "$$g"; })

# ---- C64 / Atari 8-bit full bitmap renderer ----------------------------------
# common_sagadraw/c64a8draw.c:DrawC64A8ImageFromData decodes the RLE multicolor
# bitmaps of the SagaPlus C64/Atari titles (Spider-Man, Buckaroo Banzai, …),
# which ship the database and the loose R###/B###/S### image files separately
# (extracted with build/c64extract). c64a8test.c unity-includes c64a8draw.c and
# composites a room into a 320x200 displayed-colour grid, byte-compared against a
# golden decoded from a real VICE screenshot (approach B, same C64R1/C64C2
# pipeline as c64test; see c64_decode_png.py + groundtruth_c64a8/README.md). A
# render spec (<name>.spec) lists the background blob and any object overlays.
# The Atari 8-bit half of the renderer (is_c64=0) is covered by the same target
# via the `!atari8` spec directive (golden hulk_atari8/room01_chair); its blobs
# come off a companion disk side, dumped with build/extract_images.
C64A8_SRCS := c64a8test.c $(TERPS)/common_utils/read_le16.c
C64A8_GOLDENS := $(wildcard groundtruth_c64a8/*/*.c64)

build/c64a8test: $(C64A8_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -include stddef.h -o $@ $(C64A8_SRCS)

c64a8test: build/c64a8test
	@$(call no_goldens,C64/A8,$(C64A8_GOLDENS)) \
	$(call run_goldens,C64/A8,$(C64A8_GOLDENS),{ \
	  spec="$${g%.c64}.spec"; \
	  printf '%-40s ' "$$(basename $$(dirname $$g))/$$(basename $$spec)"; \
	  ./build/c64a8test cmp "$$spec" "$$g"; })

# ---- ZX Spectrum Irmak / tile renderer ---------------------------------------
# aiukgraphics/irmak.c:DrawPictureNumber decodes the tile/attribute stream of
# the ZX Spectrum Scott Adams games (Gremlins, Robin of Sherwood, Claymorgue,
# …) into a 256x96 (32x12 tile) picture. These graphics never reach USImages —
# they live in the aiukgraphics subsystem — so the loader state (256-tile font,
# per-picture data + geometry) is dumped by build/zxextract (real DetectGame ->
# SagaGraphicsSetup) into a golden dir. zxtest.c unity-includes irmak.c, supplies
# the PutPixel/RectFill sinks, links palette.c for Remap/pal, and byte-compares
# the rendered grid to a golden decoded from a real ZX screenshot (same
# C64R1/C64C2 pipeline + decoder as the C64/Atari tests), or to a SCREEN$ dump
# (.scr) written by zx_capture.lua.
ZX_SRCS := zxtest.c $(TERPS)/aiukgraphics/palette.c
ZX_GOLDENS := $(wildcard groundtruth_zx/*/*.zx groundtruth_zx/*/*.scr)

build/zxtest: $(ZX_SRCS) $(TERPS)/aiukgraphics/irmak.c
	@mkdir -p build
	$(CC) $(CFLAGS) -include stddef.h -o $@ $(ZX_SRCS)

# Howarth / Mysterious Adventures vector renderer (ai_uk/line_drawing.c). Same
# golden-dir scheme; goldens use the .zxvec extension to keep them on their own
# harness. zxextract dumps the LineImages[] vector stream.
ZXVEC_SRCS := zxvectortest.c headless_glk_stubs.c $(TERPS)/aiukgraphics/palette.c \
              $(SCOTT)/ai_uk/ringbuffer.c $(TERPS)/common_utils/memory_allocation.c
ZXVEC_GOLDENS := $(wildcard groundtruth_zx/*/*.zxvec)

build/zxvectortest: $(ZXVEC_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -include stddef.h -o $@ $(ZXVEC_SRCS)

# One golden of a ZX harness: $(call zx_golden,HARNESS)
zx_golden = { dir="$$(dirname $$g)"; \
	  printf '%-40s ' "$$(basename $$dir)/$$(basename $$g)"; \
	  ./build/$(1) cmp "$$dir" "$$g"; }

zxtest: build/zxtest build/zxvectortest
	@fail=0; \
	$(call each_golden,$(ZX_GOLDENS),$(call zx_golden,zxtest)) \
	$(call each_golden,$(ZXVEC_GOLDENS),$(call zx_golden,zxvectortest)) \
	$(call no_goldens,ZX,$(ZX_GOLDENS)$(ZXVEC_GOLDENS)) \
	$(call goldens_verdict,ZX)

# ---- Scene tests: the whole interpreter against hardware screen dumps ---------
# The picture paths that live above the renderers — the game-specific Look
# routines (Hulk's item overlays, Robin of Sherwood's forest composites, the
# Gremlins animations), Seas of Blood's Taylor-style pictures, the Howarth
# vector pictures of the Mysterious Adventures, every Irmak version, the C64
# palette remaps and image patches, and the ZX loading screen — cannot be
# reached by unity-including one renderer. scenetest.c runs the real glk_main()
# instead: a .scene command script plays the game, glk_window_fill_rect paints
# into a framebuffer, and `!check` lines compare it with a screen dump captured
# under an emulator at the same point of the same game (zx_capture.lua for
# MAME, c64_capture.py for VICE; see the header of scenetest.c for the script
# format, groundtruth_scene/README.md for the capture recipe).
#
# Same loader object set as extract_images_c64 (whose unp64 objects are reused),
# plus the Taylor picture renderer and the real random number generator. The
# interpreter sources are built with -include scene_glk.h, which redirects their
# Glk window/event calls to the fake window layer (common_imagetest/
# fake_glk_window.c) and the scripted event loop in scenetest.c, and with
# AUTOWIN, so that a scene can walk past Seas of Blood's dice battles.
SCENE_CSRCS := $(filter-out extract_images.c,$(C64X_CSRCS)) \
  $(TERPS)/aiukgraphics/taylordraw.c $(TERPS)/common_utils/randomness.c
SCENES := $(wildcard groundtruth_scene/*/*.scene)
# The fake Glk window layer, shared with the Plus/TaylorMade image probes.
IMAGETEST := $(TERPS)/common_imagetest

build/scenetest: scenetest.c scene_glk.h $(SCENE_CSRCS) build/extract_images_c64 \
                 $(IMAGETEST)/fake_glk_window.c $(IMAGETEST)/fake_glk_window.h
	@mkdir -p build/sceneobj
	@echo "compiling scene harness..."; \
	$(call compile_each,$(SCENE_CSRCS),build/sceneobj,$(EXTRACT_CFLAGS) -DSAGA_REAL_C64_DECRUNCH \
	  -DSAGA_SCENE_HARNESS -DAUTOWIN -include scene_glk.h -I$(TERPS)/taylor -I$(UNP64)) \
	$(CC) -std=c11 -g -O0 -w -DSPATTERLIGHT $(EXTRACT_INC) -I$(IMAGETEST) -c scenetest.c -o build/sceneobj/scenetest.o || exit 1; \
	$(CC) -std=c11 -g -O0 -w -I$(CG) -c $(IMAGETEST)/fake_glk_window.c -o build/sceneobj/fake_glk_window.o || exit 1; \
	c++ -g -O0 build/sceneobj/scenetest.o build/sceneobj/fake_glk_window.o $$objs build/unp64/*.o $(CGOBJ) -lz -o $@

scenetest: build/scenetest
	@$(call no_goldens,Scenes,$(SCENES)) \
	$(call run_goldens,Scenes,$(SCENES),{ \
	  echo "$$(basename $$(dirname $$g))/$$(basename $$g)"; \
	  ./build/scenetest "$$g"; },all checks passed)

# Run every per-format byte-exact renderer test in one shot.
all-tests: test dostest apple2test titest c64test c64a8test zxtest scenetest
	@echo "All image-format tests passed"

clean:
	rm -rf build

.PHONY: test groundtruth dostest apple2test titest c64test c64a8test zxtest scenetest all-tests clean
