![Scrap splash](/extras/scrap_splash_v2.png)

# Scrap

![CI build](https://img.shields.io/github/actions/workflow/status/Grisshink/scrap/makefile.yml)
![Version](https://img.shields.io/github/v/release/Grisshink/scrap)
![Downloads](https://img.shields.io/github/downloads/Grisshink/scrap/total)
![License](https://img.shields.io/github/license/Grisshink/scrap)

Scrap is a new block based programming language with the aim towards advanced users. 
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

> [!WARNING]
> Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Notable advantages from scratch

- *(LLVM only)* Faster runtime. The speed is achieved using compilation through LLVM
- The addition of separate else if, else blocks (C-end blocks as i call them), which eliminates a lot of nested checks with if-else blocks (i.e. more flexible variant of if-else block in Snap!)
- Variables can have a lifetime, which avoids variable name conflicts and allows to make temporary variables
- Custom blocks can return values and can be used as an argument for other block
- Various string manipulation blocks and bitwise operator blocks
- Data type conversion functions
- More strict checks for [[] = []] and [[] != []] blocks. Now they are case sensitive and will check data type for equality
- Lists are now a data type instead of a different type of variable, this allows nesting lists inside a list
- The code runs in a separate thread. This solves some performance issues compared to Scratch
- Modularized interface. Most of the interface can be rearranged or moved to another tab
- *(LLVM only)* Standalone builds. Scrap projects can be built and run as native executables without significant runtime overhead

## Controls

- Click on blocks to pick up them, click again to drop them
- You can use `Ctrl` to take only one block and `Alt` to pick up its duplicate
- Hold left mouse button to move around code space
- Holding middle mouse button will do the same, except it works everywhere
- Press `Tab` to jump to chain in code base (Useful if you got lost in code base)
- Press `F5` to run the project. Press `F6` to stop it.
- Press arrow keys while the block is highlighted to move the block cursor around
- Press `Enter` to enter the highlighted text box and `Esc` to leave that text box
- Press `S` to open block search menu

## Screenshots

![Screenshot1](/extras/scrap_screenshot1.png)
![Screenshot2](/extras/scrap_screenshot2.png)
![Screenshot3](/extras/scrap_screenshot3.png)

## Building

### Dependencies

Scrap requires these dependencies to run:
- [gettext](https://www.gnu.org/software/gettext/)
- [LLVM](https://llvm.org/) *(Only required if building with `USE_COMPILER=TRUE` flag)*

Currently Scrap can be built for *Windows*, *Linux*, *MacOS* and *FreeBSD*. 

#### Download commands for Windows (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc make gettext
ln -sf "${MSYSTEM_PREFIX}/bin/windres.exe" "${MSYSTEM_PREFIX}/bin/x86_64-w64-mingw32-windres"
```

#### Download commands for Windows (LLVM) *(Experimental)*

If you are going to compile with `USE_COMPILER=TRUE` flag, then you need to install additional dependencies:

```bash
pacman -S mingw-w64-ucrt-x86_64-llvm
```

#### Download commands for Debian

```bash
sudo apt install libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev gettext
```

#### Download commands for Arch linux

Download command for arch-based distributions:

```bash
sudo pacman -S libx11 libxrandr libxi libxcursor libxinerama gettext
```

#### Download commands for OpenSUSE

Download command for openSUSE:

```bash
sudo zypper install libX11-devel libXrandr-devel libXi-devel libXcursor-devel libXinerama-devel gettext
```

### Build

Before building the repo needs to be cloned along with its submodules. To do this, run:

```bash
git clone --recursive https://github.com/Grisshink/scrap.git
cd scrap
```

#### Windows build

NOTE: This guide will assume that you have [MSYS2](https://www.msys2.org/) installed and running on your system. 

After that, run the following commands:

```bash
make -B TARGET=WINDOWS
./scrap.exe
```

NOTE: When running `make clean` MSYS2 will occasionally drop you into command prompt. 
To fix this, just type `exit` in the cmd and the cleanup process will proceed

#### Linux build

To build and run Scrap on linux you need to install `gcc` (10+) and `make`. After install, just run following commands:

```bash
make -j$(nproc)
./scrap
```

#### FreeBSD build

To build and run Scrap on FreeBSD you need to install `gcc` (10+) and `gmake`. After install, just run following commands:

```bash
gmake MAKE=gmake -j$(nproc)
./scrap
```

#### MacOS build

> [!WARNING]
> MacOS build is not being tested right now, so it may not work properly or not at all, you have been warned!

To build and run Scrap on macOS, you need to install `gcc` (10+) and `make`.
First, install Homebrew:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

After that, you need to run the following commands:

```bash
brew install gettext
make -j$(nproc) TARGET=OSX
./scrap
```

Thanks to [@arducat](https://github.com/arducat) for MacOS support.

## Wait, there is more?

In `examples/` folder you can find some example code writen in Scrap that uses most features from Scrap

In `extras/` folder you can find some various artwork made for Scrap. 
The splash art was made by [@FlaffyTheBest](https://scratch.mit.edu/users/FlaffyTheBest/), 
the logo was made by [@Grisshink](https://github.com/Grisshink) with some inspiration for logo from [@unixource](https://github.com/unixource), 
the wallpaper was made by [@Grisshink](https://github.com/Grisshink)

## License

All scrap code is licensed under the terms of [zlib license](/LICENSE).
