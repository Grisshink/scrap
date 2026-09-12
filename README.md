![Scrap splash](/extras/scrap_splash_v2.png)

# Scrap

![CI build](https://img.shields.io/github/actions/workflow/status/Grisshink/scrap/build.yml)
![Version](https://img.shields.io/github/v/release/Grisshink/scrap)
![Downloads](https://img.shields.io/github/downloads/Grisshink/scrap/total)
![License](https://img.shields.io/github/license/Grisshink/scrap)

Scrap is a new block based programming language with the aim towards advanced users.
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

> [!WARNING]
> Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Notable advantages from scratch

- The addition of separate else if, else blocks (C-end blocks as i call them), which eliminates a lot of nested checks with if-else blocks (i.e. more flexible variant of if-else block in Snap!)
- Variables can have a lifetime, which avoids variable name conflicts and allows to make temporary variables
- Custom blocks can return values and can be used as an argument for other block
- Various string manipulation blocks and bitwise operator blocks
- Data type conversion functions
- More strict checks for [[] = []] and [[] != []] blocks. Now they are case sensitive and will check data type for equality
- Lists are now a data type instead of a different type of variable, this allows nesting lists inside a list
- The code runs in a separate thread. This solves some performance issues compared to Scratch
- Modularized interface. Most of the interface can be rearranged or moved to another tab

## Controls

- Click on blocks to pick up them, click again to drop them
- Hold `Ctrl` to take single block, `Alt` to duplicate blocks and `Shift` to pickup/drop multiple block chains
- Hold left mouse button to move around code space
- Holding middle mouse button will do the same, except it works everywhere
- Press `Tab` to jump to chain in code base (Useful if you got lost in code base)
- Press `F5` to run the project. Press `F6` to stop it.
- Press arrow keys while the block is highlighted to move the block cursor around
- Press `Enter` to enter the highlighted text box and `Esc` to leave that text box
- Press `S` to open block search menu

## Binary Installation

### Github releases

Currently Scrap only provides binary releases for *Windows* and *Linux*.
See [Releases](https://github.com/Grisshink/scrap/releases) page for all available download options.

### AUR

Scrap is now available for download from Arch User Repository (AUR) as [scrap-git](https://aur.archlinux.org/packages/scrap-git) package.
This package will download and build latest Scrap commit from git.

To install Scrap from AUR you can use your preferred AUR helper, for example with `yay`:

```bash
yay -S scrap-git
```

## Building

### Dependencies

Scrap requires these dependencies to run:
- [gettext](https://www.gnu.org/software/gettext/)
- [libffi](https://sourceware.org/libffi/)

Currently Scrap officially supports *Windows* and *Linux* platforms. Scrap also provides build steps for *NixOS*, *MacOS* and *FreeBSD*, but note that these builds are not actively maintained so they may not work properly.

#### Windows (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libffi make gettext
ln -sf "${MSYSTEM_PREFIX}/bin/windres.exe" "${MSYSTEM_PREFIX}/bin/x86_64-w64-mingw32-windres"
```

#### Debian linux

```bash
sudo apt install libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev gettext libffi-dev
```

#### Arch linux

```bash
sudo pacman -S libx11 libxrandr libxi libxcursor libxinerama gettext libffi
```

#### OpenSUSE

Download command for openSUSE:

```bash
sudo zypper install libX11-devel libXrandr-devel libXi-devel libXcursor-devel libXinerama-devel gettext libffi-devel
```

### Build

Before building the repo needs to be cloned along with its submodules. To do this, run:

```bash
git clone --recursive https://github.com/Grisshink/scrap.git
cd scrap
```

#### Windows

*NOTE: This guide will assume that you have MSYS2 installed and running on your system.
See https://www.msys2.org/ for details on installation.*

After installation, run the following commands:

```bash
make -B TARGET=WINDOWS
./scrap.exe
```

*NOTE: When running `make clean` MSYS2 will occasionally drop you into command prompt.
To fix this, just type `exit` in the cmd and the cleanup process will proceed*

#### Linux

To build and run Scrap on linux you need to install `gcc` (10+) and `make`. After installation, run the following commands:

```bash
make -j$(nproc)
./scrap
```

#### FreeBSD

To build and run Scrap on FreeBSD you need to install `gcc` (10+) and `gmake`. After installation, run the following commands:

```bash
gmake MAKE=gmake -j$(nproc)
./scrap
```

#### NixOS

To build and run Scrap on NixOS, run the following commands:

```bash
nix-shell
make -j$(nproc)
./scrap
```

#### MacOS

> [!WARNING]
> MacOS build is not being tested right now, so it may not work properly or not at all, you have been warned!

To build and run Scrap on macOS, you need to install `gcc` (10+) and `make`.
First, install Homebrew:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

After installation, run the following commands:

```bash
brew install gettext
make -j$(nproc) TARGET=OSX
./scrap
```

*(MacOS support provided by [@arducat](https://github.com/arducat))*

## Screenshots

![Screenshot1](/extras/scrap_screenshot1.png)
![Screenshot2](/extras/scrap_screenshot2.png)
![Screenshot3](/extras/scrap_screenshot3.png)

## Wait, there is more?

In `examples/` folder you can find some example code writen in Scrap that uses most features from Scrap

In `extras/` folder you can find some various artwork made for Scrap.
The splash art was made by [@FlaffyTheBest](https://scratch.mit.edu/users/FlaffyTheBest/),
the logo was made by [@Grisshink](https://github.com/Grisshink) with some inspiration for logo from [@unixource](https://github.com/unixource),
the wallpaper was made by [@Grisshink](https://github.com/Grisshink)

## License

All scrap code is licensed under the terms of [zlib license](/LICENSE).
