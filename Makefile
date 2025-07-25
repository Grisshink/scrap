SCRAP_VERSION := 0.4.2-beta

MAKE ?= make
TARGET ?= LINUX
BUILD_MODE ?= RELEASE
USE_COMPILER ?= FALSE

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
	CFLAGS += -g -O0 -DDEBUG
endif

STD_OBJFILES := $(addprefix src/,vec.o gc-stand.o std-stand.o scrap-runtime.o)
OBJFILES := $(addprefix src/,filedialogs.o render.o save.o term.o blocks.o scrap.o vec.o util.o input.o scrap_gui.o window.o cfgpath.o platform.o ast.o)
BUNDLE_FILES := data examples extras locale LICENSE README.md CHANGELOG.md
SCRAP_HEADERS := src/scrap.h src/ast.h src/config.h src/scrap_gui.h
EXE_NAME := scrap

ifeq ($(TARGET), WINDOWS)
	STD_NAME := libscrapstd-win.a
else
	STD_NAME := libscrapstd.a
endif

ifeq ($(USE_COMPILER), FALSE)
	OBJFILES += src/interpreter.o
	SCRAP_HEADERS += src/interpreter.h
	CFLAGS += -DUSE_INTERPRETER
else
	LLVM_CONFIG ?= llvm-config
	OBJFILES += $(addprefix src/,compiler.o gc.o std.o)
	SCRAP_HEADERS += src/compiler.h

	ifeq ($(TARGET), WINDOWS)
		LDFLAGS += -lLLVMX86TargetMCA -lLLVMMCA -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMX86Desc -lLLVMX86Info -lLLVMMCDisassembler -lLLVMInstrumentation -lLLVMIRPrinter -lLLVMGlobalISel -lLLVMSelectionDAG -lLLVMCFGuard -lLLVMAsmPrinter -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMAggressiveInstCombine -lLLVMObjCARCOpts -lLLVMTransformUtils -lLLVMCodeGenTypes -lLLVMCGData -lLLVMBitWriter -lLLVMMCJIT -lLLVMExecutionEngine -lLLVMTarget -lLLVMAnalysis -lLLVMProfileData -lLLVMSymbolize -lLLVMDebugInfoBTF -lLLVMDebugInfoPDB -lLLVMDebugInfoMSF -lLLVMDebugInfoDWARF -lLLVMRuntimeDyld -lLLVMOrcTargetProcess -lLLVMOrcShared -lLLVMObject -lLLVMTextAPI -lLLVMMCParser -lLLVMIRReader -lLLVMAsmParser -lLLVMBitReader -lLLVMMC -lLLVMDebugInfoCodeView -lLLVMCore -lLLVMRemarks -lLLVMBitstreamReader -lLLVMBinaryFormat -lLLVMTargetParser -lLLVMSupport -lLLVMDemangle -lm -lz -lzstd -lxml2 -limagehlp -lntdll -lole32 -luuid
		CFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
	else
		LLVM_LDFLAGS := --ldflags --system-libs --libs core executionengine mcjit analysis native
		ifeq ($(LLVM_LINK_STATIC), TRUE)
			LDFLAGS += -Wl,-Bstatic `$(LLVM_CONFIG) $(LLVM_FLAGS) --link-static $(LLVM_LDFLAGS)` -Wl,-Bdynamic
		else
			LDFLAGS += `$(LLVM_CONFIG) $(LLVM_FLAGS) $(LLVM_LDFLAGS)`
		endif
		CFLAGS += `$(LLVM_CONFIG) --cflags`
	endif

	LDFLAGS += -lstdc++
endif

LINUX_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-linux
MACOS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-macos
WINDOWS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-windows64

.PHONY: all clean target translations

all: target std translations

clean:
	$(MAKE) -C raylib/src clean
	rm -f src/*.o $(EXE_NAME) $(EXE_NAME).exe Scrap-x86_64.AppImage $(LINUX_DIR).tar.gz $(WINDOWS_DIR).zip $(MACOS_DIR).zip scrap.res $(STD_NAME)
	rm -rf locale

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

windows-build: translations target
	mkdir -p $(WINDOWS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME).exe $(WINDOWS_DIR)
	cp libzstd.dll $(WINDOWS_DIR)
	zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR)
	rm -r $(WINDOWS_DIR)

linux-build: translations target
	mkdir -p $(LINUX_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(LINUX_DIR)
	tar czvf $(LINUX_DIR).tar.gz $(LINUX_DIR)
	rm -r $(LINUX_DIR)

macos-build: translations target
	mkdir -p $(MACOS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(MACOS_DIR)
	zip -r $(MACOS_DIR).zip $(MACOS_DIR)
	rm -r $(MACOS_DIR)

appimage: translations target
	mkdir -p scrap.AppDir
	cp $(EXE_NAME) scrap.AppDir/AppRun
	cp -r data locale scrap.desktop extras/scrap.png scrap.AppDir
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
	$(CC) -o $@ $^ raylib/src/libraylib.a scrap.res $(LDFLAGS)

$(EXE_NAME): $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
	$(CC) -o $@ $^ raylib/src/libraylib.a $(LDFLAGS)

std: $(STD_OBJFILES)
	ar rcs $(STD_NAME) $^

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
src/blocks.o: src/blocks.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/vec.o: src/vec.c
	$(CC) $(CFLAGS) -c -o $@ $<
src/util.o: src/util.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/input.o: src/input.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/platform.o: src/platform.c
	$(CC) $(CFLAGS) -c -o $@ $<
src/ast.o: src/ast.c src/ast.h
	$(CC) $(CFLAGS) -c -o $@ $<
src/interpreter.o: src/interpreter.c $(SCRAP_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<
src/compiler.o: src/compiler.c src/compiler.h src/gc.h src/ast.h src/compiler-common.h
	$(CC) $(CFLAGS) -c -o $@ $<
src/gc.o: src/gc.c src/gc.h src/vec.h src/std-types.h src/std.h
	$(CC) $(CFLAGS) -c -o $@ $<
src/std.o: src/std.c src/std.h src/gc.h src/std-types.h src/term.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/gc-stand.o: src/gc.c src/gc.h src/vec.h src/std-types.h src/std.h
	$(CC) $(CFLAGS) -DSTANDALONE_STD -c -o $@ $<
src/std-stand.o: src/std.c src/std.h src/gc.h src/std-types.h
	$(CC) $(CFLAGS) -DSTANDALONE_STD -c -o $@ $<
src/scrap-runtime.o: src/scrap-runtime.c src/gc.h
	$(CC) $(CFLAGS) -DSTANDALONE_STD -c -o $@ $<

src/filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ $<
src/cfgpath.o: external/cfgpath.c
	$(CC) $(CFLAGS) -c -o $@ $<
