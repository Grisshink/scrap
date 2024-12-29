SCRAP_VERSION := 0.1-beta

CC := gcc
CFLAGS := -Wall -Wextra -O3 -s -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -fmax-errors=5
LDFLAGS := -lraylib -lGL -lm -lpthread -lX11
OBJFILES := scrap.o filedialogs.o
EXE_NAME := scrap

all: $(EXE_NAME)

clean:
	rm $(OBJFILES) $(EXE_NAME)

$(EXE_NAME): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

scrap.o: scrap.c external/raylib-nuklear.h vm.h
	$(CC) $(CFLAGS) -c -o $@ scrap.c

filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ external/tinyfiledialogs.c
