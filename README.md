![Scrap splash](/extras/scrap_splash_v2.png)

# Scrap

Scrap is a new block based programming language with the aim towards advanced users. 
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

## Notable advantages from scratch

- Faster runtime (Still not faster than Turbowarp because Scrap is interpreted *for now*)
- The addition of separate else if, else blocks (C-end blocks as i call them), which eliminates a lot of nested checks with if-else blocks (i.e. more flexible variant of if-else block in Snap!)
- Variables can have a lifetime, which avoids variable name conflicts and allows to make temporary variables
- Custom blocks can return values and can be used as an argument for other block
- Various string manipulation blocks and bitwise operator blocks
- Data type conversion functions
- More strict checks for [[] = []] and [[] != []] blocks. Now they are case sensitive and will check data type for equality
- Lists are now a data type instead of a different type of variable, this allows nesting lists inside a list (although it's not very convenient as of right now)
- The code runs in a separate thread. This solves some performance issues compared to Scratch
- Modularized interface. Most of the interface can be rearranged or moved to another tab

## ⚠️ WARNING ⚠️

Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Controls

- Click on blocks to pick up them, click again to drop them
- You can use `Ctrl` to take only one block and `Alt` to pick up its duplicate
- Hold left mouse button to move around code space
- Holding middle mouse button will do the same, except it works everywhere
- Press `Space` to jump to chain in code base (Useful if you got lost in code base)
- Press `F5` to run the project. Press `F6` to stop it.

## Screenshots

![Screenshot1](/extras/scrap_screenshot1.png)
![Screenshot2](/extras/scrap_screenshot2.png)
![Screenshot3](/extras/scrap_screenshot3.png)

## Building

### Dependencies

Scrap requires these dependencies to run:
- [Raylib](https://github.com/raysan5/raylib) *(Built in)* (Patched to support SVGs) (Needs additional dependencies to build)
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) *(Built in)*
- [cfgpath](https://github.com/Malvineous/cfgpath) *(Built in)* (Heavily modified to workaround `windows.h` conflicts)
- [gettext](https://www.gnu.org/software/gettext/)

Download command for debian-based distributions:

```bash
sudo apt install libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev gettext
```

Download command for arch-based distributions:

```bash
sudo pacman -S libx11 libxrandr libxi libxcursor libxinerama gettext
```

### Build

Before building the repo needs to be cloned along with its submodules. To do this, run:

```bash
git clone --recursive https://github.com/Grisshink/scrap.git
cd scrap
```

Currently Scrap can be built for *Windows*, *Linux*, *MacOS* and *FreeBSD*. 

#### Linux build

To build and run Scrap on linux you need to install `gcc` and `make`. After install, just run following commands:

```bash
make -j$(nproc)
./scrap
```

#### FreeBSD build

To build and run Scrap on FreeBSD you need to install `gcc` and `gmake`. After install, just run following commands:

```bash
gmake MAKE=gmake -j$(nproc)
./scrap
```

#### Windows build

To build and run Scrap on Windows you need to have [mingw-w64](https://www.mingw-w64.org/) installed. 
The most recommended way to use it is through [MSYS2](https://www.msys2.org/). 
After that, just run following commands:

```bash
make -B TARGET=WINDOWS
```

This will build `scrap.exe` binary which can be run normally.

#### MacOS build

⚠️ WARNING ⚠️ MacOS build is not being tested right now, so it may not work properly or not at all, you have been warned!

To build and run Scrap on macOS, you need to install `gcc` and `make`.
First, install Homebrew:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

After that, you need to run the following commands:

```bash
brew install gcc
make -j$(nproc) TARGET=MACOS
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

All scrap code is licensed under the terms of [GPLv3 license](/LICENSE).
