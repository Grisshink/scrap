SCRAP_VERSION := 0.6-beta

MAKE ?= make
TARGET ?= LINUX
BUILD_MODE ?= RELEASE
USE_COMPILER ?= FALSE
BUILD_FOLDER := build/
PREFIX ?= /usr/local

ifeq ($(USE_COMPILER), TRUE)
	SCRAP_VERSION := $(SCRAP_VERSION)-llvm
endif

CFLAGS := -Wall -Wextra -std=c11 -D_GNU_SOURCE -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -I./raylib/src

ifeq ($(TARGET), LINUX)
	CC := gcc
	LDFLAGS := -lm -lpthread -lX11 -ldl
else ifeq ($(TARGET), OSX)
	# Thanks to @arducat for MacOS support
	CC := clang
	LDFLAGS := -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm -lpthread -lintl
else
	CC := x86_64-w64-mingw32-gcc
	LDFLAGS := -static -lole32 -lcomdlg32 -lwinmm -lgdi32 -lintl -liconv -lshlwapi -Wl,--subsystem,windows
endif

ifeq ($(ARABIC_MODE), TRUE)
	CFLAGS += -DARABIC_MODE
endif

ifeq ($(RAM_OVERLOAD), TRUE)
	CFLAGS += -DRAM_OVERLOAD
endif

ifeq ($(CC), clang)
	CFLAGS += -ferror-limit=5
else
	CFLAGS += -fmax-errors=5
endif

ifeq ($(BUILD_MODE), RELEASE)
	CFLAGS += -s -O3
else
	CFLAGS += -g -O1 -DDEBUG
	LDFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
endif

STD_CFLAGS := $(CFLAGS) -fPIC

ifeq ($(BUILD_MODE), DEBUG)
	CFLAGS += -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
endif

STD_OBJFILES := $(addprefix $(BUILD_FOLDER),vec-stand.o gc-stand.o std-stand.o scrap-runtime.o)
OBJFILES := $(addprefix $(BUILD_FOLDER),filedialogs.o render.o save.o term.o blocks.o scrap.o vec.o util.o ui.o scrap_gui.o window.o cfgpath.o platform.o ast.o gc.o std.o thread.o vm.o)
BUNDLE_FILES := data examples extras locale LICENSE README.md CHANGELOG.md
SCRAP_HEADERS := src/scrap.h src/ast.h src/config.h src/scrap_gui.h
EXE_NAME := scrap

ifeq ($(TARGET), WINDOWS)
	STD_NAME := libscrapstd-win.a
else
	STD_NAME := libscrapstd.a
endif

ifeq ($(USE_COMPILER), FALSE)
	OBJFILES += $(addprefix $(BUILD_FOLDER),interpreter.o)
	SCRAP_HEADERS += src/interpreter.h
	CFLAGS += -DUSE_INTERPRETER
else
	LLVM_CONFIG ?= llvm-config
	OBJFILES += $(addprefix $(BUILD_FOLDER),compiler.o)
	SCRAP_HEADERS += src/compiler.h

	LLVM_LDFLAGS := --ldflags --system-libs --libs core executionengine mcjit analysis native
	ifeq ($(TARGET), WINDOWS)
		LDFLAGS += `$(LLVM_CONFIG) $(LLVM_FLAGS) --link-static $(LLVM_LDFLAGS)`
	else
		ifeq ($(LLVM_LINK_STATIC), TRUE)
			LDFLAGS += -Wl,-Bstatic `$(LLVM_CONFIG) $(LLVM_FLAGS) --link-static $(LLVM_LDFLAGS)` -Wl,-Bdynamic
		else
			LDFLAGS += `$(LLVM_CONFIG) $(LLVM_FLAGS) $(LLVM_LDFLAGS)`
		endif
	endif
	CFLAGS += `$(LLVM_CONFIG) --cflags`

	LDFLAGS += -lstdc++
endif

LINUX_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-linux
MACOS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-macos
WINDOWS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-windows64

.PHONY: all clean target translations

all: target std translations

mkbuild:
	mkdir -p $(BUILD_FOLDER)

clean:
	$(MAKE) -C raylib/src clean
	rm -f scrap.res $(EXE_NAME) $(EXE_NAME).exe libscrapstd.a libscrapstd-win.a
	rm -rf locale $(BUILD_FOLDER)

translations:
	@echo === Generating locales... ===
	rm -rf locale
	cp -r translations locale
	msgfmt -o locale/ru/LC_MESSAGES/scrap.mo locale/ru/LC_MESSAGES/scrap.po
	rm locale/ru/LC_MESSAGES/scrap.po
	msgfmt -o locale/kk/LC_MESSAGES/scrap.mo locale/kk/LC_MESSAGES/scrap.po
	rm locale/kk/LC_MESSAGES/scrap.po
	msgfmt -o locale/uk/LC_MESSAGES/scrap.mo locale/uk/LC_MESSAGES/scrap.po
	rm locale/uk/LC_MESSAGES/scrap.po

windows-build: translations target std
	mkdir -p $(BUILD_FOLDER)$(WINDOWS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME).exe $(STD_NAME) $(BUILD_FOLDER)$(WINDOWS_DIR)
	cp $(BUILD_FOLDER)$(WINDOWS_DIR)
	cd $(BUILD_FOLDER); zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR); cd ..

linux-build: translations target std
	mkdir -p $(BUILD_FOLDER)$(LINUX_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(STD_NAME) $(BUILD_FOLDER)$(LINUX_DIR)
	tar czvf $(BUILD_FOLDER)$(LINUX_DIR).tar.gz --directory=$(BUILD_FOLDER) $(LINUX_DIR)

macos-build: translations target std
	mkdir -p $(BUILD_FOLDER)$(MACOS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(STD_NAME) $(BUILD_FOLDER)$(MACOS_DIR)
	cd $(BUILD_FOLDER); zip -r $(MACOS_DIR).zip $(MACOS_DIR); cd ..

appimage: translations target std
	mkdir -p $(BUILD_FOLDER)scrap.AppDir
	cp $(EXE_NAME) $(BUILD_FOLDER)scrap.AppDir/AppRun
	cp -r data locale scrap.desktop $(STD_NAME) extras/scrap.png $(BUILD_FOLDER)scrap.AppDir
	./appimagetool-x86_64.AppImage --appimage-extract-and-run $(BUILD_FOLDER)scrap.AppDir $(BUILD_FOLDER)/Scrap-v$(SCRAP_VERSION).AppImage

install: translations target std
	mkdir -p $(PREFIX)/share/scrap
	mkdir -p $(PREFIX)/share/doc/scrap
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib
	cp -r data $(PREFIX)/share/scrap
	cp -r locale $(PREFIX)/share
	cp -r examples $(PREFIX)/share/doc/scrap
	cp $(EXE_NAME) $(PREFIX)/bin
	cp $(STD_NAME) $(PREFIX)/lib

uninstall:
	rm -rf $(PREFIX)/share/scrap
	rm -rf $(PREFIX)/share/doc/scrap
	rm -f $(PREFIX)/bin/$(EXE_NAME)
	rm -f $(PREFIX)/lib/$(STD_NAME)

ifeq ($(TARGET), WINDOWS)
target: mkbuild $(EXE_NAME).exe
else
target: mkbuild $(EXE_NAME)
endif

$(EXE_NAME).exe: $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) PLATFORM_OS=$(TARGET)
	x86_64-w64-mingw32-windres scrap.rc -O coff -o scrap.res
	$(CC) -o $@ $^ raylib/src/libraylib.a scrap.res $(LDFLAGS)

$(EXE_NAME): $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) PLATFORM_OS=$(TARGET)
	$(CC) -o $@ $^ raylib/src/libraylib.a $(LDFLAGS)

std: mkbuild $(STD_OBJFILES)
	ar rcs $(STD_NAME) $(STD_OBJFILES)

$(BUILD_FOLDER)scrap.o: src/scrap.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)window.o: src/window.c $(SCRAP_HEADERS) external/tinyfiledialogs.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)scrap_gui.o: src/scrap_gui.c src/scrap_gui.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)render.o: src/render.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)save.o: src/save.c $(SCRAP_HEADERS) external/cfgpath.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)term.o: src/term.c src/term.h $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)blocks.o: src/blocks.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)vec.o: src/vec.c
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)util.o: src/util.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)ui.o: src/ui.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)platform.o: src/platform.c
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)ast.o: src/ast.c src/ast.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)interpreter.o: src/interpreter.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)compiler.o: src/compiler.c src/compiler.h src/gc.h src/ast.h $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)gc.o: src/gc.c src/gc.h src/vec.h src/std.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)std.o: src/std.c src/std.h src/gc.h src/term.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)thread.o: src/thread.c src/thread.h
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)vm.o: src/vm.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_FOLDER)gc-stand.o: src/gc.c src/gc.h src/vec.h src/std.h
	$(CC) $(STD_CFLAGS) -DSTANDALONE_STD -c -o $@ $<
$(BUILD_FOLDER)std-stand.o: src/std.c src/std.h src/gc.h
	$(CC) $(STD_CFLAGS) -DSTANDALONE_STD -c -o $@ $<
$(BUILD_FOLDER)scrap-runtime.o: src/scrap-runtime.c src/gc.h
	$(CC) $(STD_CFLAGS) -DSTANDALONE_STD -c -o $@ $<
$(BUILD_FOLDER)vec-stand.o: src/vec.c
	$(CC) $(STD_CFLAGS) -DSTANDALONE_STD -c -o $@ $<

$(BUILD_FOLDER)filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD_FOLDER)cfgpath.o: external/cfgpath.c
	$(CC) $(CFLAGS) -c -o $@ $<
