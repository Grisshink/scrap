SCRAP_VERSION := 0.1.1-beta

TARGET ?= LINUX

CFLAGS := -Wall -Wextra -O3 -s -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -fmax-errors=5 -I./raylib/src

ifeq ($(TARGET), LINUX)
	CC := gcc
	LDFLAGS := -lGL -lm -lpthread -lX11 -ldl
else ifeq ($(TARGET), MACOS)
	# Thanks for @arducat for MacOS support
	CC := gcc-14
	LDFLAGS := -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm -lpthread
else
	CC := x86_64-w64-mingw32-gcc
	LDFLAGS := -static -lole32 -lcomdlg32 -lwinmm -lgdi32 -Wl,--subsystem,windows
endif

OBJFILES := $(addprefix src/,filedialogs.o gui.o render.o save.o term.o blocks.o scrap.o vec.o util.o input.o measure.o)
SCRAP_HEADERS := src/scrap.h src/vm.h src/config.h
EXE_NAME := scrap

LINUX_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-linux
MACOS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-macos
WINDOWS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-windows64

all: $(EXE_NAME)

clean:
	make -C raylib/src clean
	rm -f $(OBJFILES) $(EXE_NAME) $(EXE_NAME).exe Scrap-x86_64.AppImage $(LINUX_DIR).tar.gz $(WINDOWS_DIR).zip $(MACOS_DIR).zip

windows-build: $(EXE_NAME)
	mkdir -p $(WINDOWS_DIR)
	cp -r data examples extras LICENSE README.md $(EXE_NAME).exe $(WINDOWS_DIR)
	zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR)
	rm -r $(WINDOWS_DIR)

linux-build: $(EXE_NAME)
	mkdir -p $(LINUX_DIR)
	cp -r data examples extras LICENSE README.md $(EXE_NAME) $(LINUX_DIR)
	tar czvf $(LINUX_DIR).tar.gz $(LINUX_DIR)
	rm -r $(LINUX_DIR)

macos-build: $(EXE_NAME)
	mkdir -p $(MACOS_DIR)
	cp -r data examples extras LICENSE README.md $(EXE_NAME) $(MACOS_DIR)
	zip -r $(MACOS_DIR).zip $(MACOS_DIR)
	rm -r $(MACOS_DIR)

appimage: $(EXE_NAME)
	mkdir -p scrap.AppDir
	cp $(EXE_NAME) scrap.AppDir/AppRun
	cp -r data scrap.desktop extras/scrap.png scrap.AppDir
	appimagetool-x86_64.AppImage scrap.AppDir
	rm -r scrap.AppDir

$(EXE_NAME): $(OBJFILES)
	make -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ raylib/src/libraylib.a $(LDFLAGS)

scrap.o: scrap.c external/raylib-nuklear.h $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ scrap.c
src/gui.o: src/gui.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/render.o: src/render.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/save.o: src/save.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/term.o: src/term.c src/term.h $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/blocks.o: src/blocks.c src/blocks.h $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/vec.o: src/vec.c
	$(CC) $(CFLAGS) -c -o $@ $<
src/util.o: src/util.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/input.o: src/input.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/measure.o: src/measure.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ $<
