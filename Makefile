SCRAP_VERSION := 0.3.1-beta

MAKE ?= make
TARGET ?= LINUX
BUILD_MODE ?= RELEASE

CFLAGS := -Wall -Wextra -std=c11 -D_GNU_SOURCE -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -I./raylib/src

ifeq ($(TARGET), LINUX)
	CC := gcc
	LDFLAGS := -lm -lpthread -lX11 -ldl
else ifeq ($(TARGET), MACOS)
	# Thanks to @arducat for MacOS support
	CC := gcc-14
	LDFLAGS := -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm -lpthread
else
	CC := x86_64-w64-mingw32-gcc
	LDFLAGS := -static -lole32 -lcomdlg32 -lwinmm -lgdi32 -Wl,--subsystem,windows
endif

ifeq ($(ARABIC_MODE), TRUE)
	CFLAGS += -DARABIC_MODE
endif

ifeq ($(CC), clang)
	CFLAGS += -ferror-limit=5
else
	CFLAGS += -fmax-errors=5
endif

ifeq ($(BUILD_MODE), RELEASE)
	CFLAGS += -s -O3
else
	CFLAGS += -g -O0 -DDEBUG
endif

OBJFILES := $(addprefix src/,filedialogs.o render.o save.o term.o blocks.o scrap.o vec.o util.o input.o scrap_gui.o window.o cfgpath.o)
BUNDLE_FILES := data examples extras LICENSE README.md CHANGELOG.md
SCRAP_HEADERS := src/scrap.h src/vm.h src/config.h src/scrap_gui.h
EXE_NAME := scrap

LINUX_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-linux
MACOS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-macos
WINDOWS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-windows64

.PHONY: all clean target translations

all: target translations

clean:
	$(MAKE) -C raylib/src clean
	rm -f $(OBJFILES) $(EXE_NAME) $(EXE_NAME).exe Scrap-x86_64.AppImage $(LINUX_DIR).tar.gz $(WINDOWS_DIR).zip $(MACOS_DIR).zip scrap.res
	rm -rf locale

translations:
	@echo === Generating locales... ===
	rm -rf locale
	cp -r translations locale
	msgfmt -o locale/ru/LC_MESSAGES/scrap.mo locale/ru/LC_MESSAGES/scrap.po
	rm locale/ru/LC_MESSAGES/scrap.po

windows-build: $(EXE_NAME).exe
	mkdir -p $(WINDOWS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME).exe $(WINDOWS_DIR)
	zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR)
	rm -r $(WINDOWS_DIR)

linux-build: $(EXE_NAME)
	mkdir -p $(LINUX_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(LINUX_DIR)
	tar czvf $(LINUX_DIR).tar.gz $(LINUX_DIR)
	rm -r $(LINUX_DIR)

macos-build: $(EXE_NAME)
	mkdir -p $(MACOS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(MACOS_DIR)
	zip -r $(MACOS_DIR).zip $(MACOS_DIR)
	rm -r $(MACOS_DIR)

appimage: $(EXE_NAME)
	mkdir -p scrap.AppDir
	cp $(EXE_NAME) scrap.AppDir/AppRun
	cp -r data scrap.desktop extras/scrap.png scrap.AppDir
	./appimagetool-x86_64.AppImage --appimage-extract-and-run scrap.AppDir
	rm -r scrap.AppDir

ifeq ($(TARGET), WINDOWS)
target: $(EXE_NAME).exe
else
target: $(EXE_NAME)
endif

$(EXE_NAME).exe: $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
	x86_64-w64-mingw32-windres scrap.rc -O coff -o scrap.res
	$(CC) $(CFLAGS) -o $@ $^ raylib/src/libraylib.a scrap.res $(LDFLAGS)

$(EXE_NAME): $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ raylib/src/libraylib.a $(LDFLAGS)

src/scrap.o: src/scrap.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/window.o: src/window.c $(SCRAP_HEADERS) external/tinyfiledialogs.h
	$(CC) $(CFLAGS) -c -o $@ $<
src/scrap_gui.o: src/scrap_gui.c src/scrap_gui.h
	$(CC) $(CFLAGS) -c -o $@ $<
src/render.o: src/render.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/save.o: src/save.c $(SCRAP_HEADERS) external/cfgpath.h
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

src/filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ $<
src/cfgpath.o: external/cfgpath.c
	$(CC) $(CFLAGS) -c -o $@ $<
