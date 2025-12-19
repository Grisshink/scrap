SCRAP_VERSION := 0.4.2-beta

MAKE ?= make
TARGET ?= LINUX
BUILD_MODE ?= RELEASE
USE_COMPILER ?= FALSE
BUILD_FOLDER := build/

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
OBJFILES := $(addprefix $(BUILD_FOLDER),filedialogs.o render.o save.o term.o blocks.o scrap.o vec.o util.o input.o scrap_gui.o window.o cfgpath.o platform.o ast.o gc.o std.o)
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

mkbuild:
	mkdir -p $(BUILD_FOLDER)

clean:
	$(MAKE) -C raylib/src clean
	rm -f scrap.res $(EXE_NAME) $(EXE_NAME).exe $(STD_NAME)
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

windows-build: translations target
	mkdir -p $(WINDOWS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME).exe $(WINDOWS_DIR)
	cp libzstd.dll $(WINDOWS_DIR)
	zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR)

linux-build: translations target
	mkdir -p $(LINUX_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(LINUX_DIR)
	tar czvf $(LINUX_DIR).tar.gz $(LINUX_DIR)

macos-build: translations target
	mkdir -p $(MACOS_DIR)
	cp -r $(BUNDLE_FILES) $(EXE_NAME) $(MACOS_DIR)
	zip -r $(MACOS_DIR).zip $(MACOS_DIR)

appimage: translations target
	mkdir -p $(BUILD_FOLDER)scrap.AppDir
	cp $(EXE_NAME) $(BUILD_FOLDER)scrap.AppDir/AppRun
	cp -r data locale scrap.desktop extras/scrap.png $(BUILD_FOLDER)scrap.AppDir
	./appimagetool-x86_64.AppImage --appimage-extract-and-run $(BUILD_FOLDER)scrap.AppDir $(BUILD_FOLDER)

ifeq ($(TARGET), WINDOWS)
target: mkbuild $(EXE_NAME).exe
else
target: mkbuild $(EXE_NAME)
endif

$(EXE_NAME).exe: $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
	x86_64-w64-mingw32-windres scrap.rc -O coff -o scrap.res
	$(CC) -o $@ $^ raylib/src/libraylib.a scrap.res $(LDFLAGS)

$(EXE_NAME): $(OBJFILES)
	$(MAKE) -C raylib/src CC=$(CC) CUSTOM_CFLAGS=-DSUPPORT_FILEFORMAT_SVG PLATFORM_OS=$(TARGET)
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
$(BUILD_FOLDER)input.o: src/input.c $(SCRAP_HEADERS)
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
