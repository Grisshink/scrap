![Scrap splash](/assets/scrap_splash_v2.png)

# Scrap

![CI build](https://img.shields.io/github/actions/workflow/status/Grisshink/scrap/makefile.yml)
![Version](https://img.shields.io/github/v/release/Grisshink/scrap)
![Downloads](https://img.shields.io/github/downloads/Grisshink/scrap/total)
![License](https://img.shields.io/github/license/Grisshink/scrap)

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
- [Raylib](https://github.com/raysan5/raylib) *(Built in)* (Patched to support SVGs) (Needs additional dependencies to build)
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) *(Built in)*
- [cfgpath](https://github.com/Malvineous/cfgpath) *(Built in)* (Heavily modified to workaround `windows.h` conflicts)
- [gettext](https://www.gnu.org/software/gettext/)

Download commands for MSYS2 UCRT64 (Windows):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc make
ln -sf "${MSYSTEM_PREFIX}/bin/windres.exe" "${MSYSTEM_PREFIX}/bin/x86_64-w64-mingw32-windres"
```

Download command for debian-based distributions:

```bash
sudo apt install libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev gettext
```

Download command for arch-based distributions:

```bash
sudo pacman -S libx11 libxrandr libxi libxcursor libxinerama gettext
```

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

Currently Scrap can be built for *Windows*, *Linux*, *MacOS* and *FreeBSD*. 

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

#### MacOS build

⚠️ WARNING ⚠️ MacOS build is not being tested right now, so it may not work properly or not at all, you have been warned!

To build and run Scrap on macOS, you need to install `gcc` and `make`.
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

All scrap code is licensed under the terms of [GPLv3 license](/LICENSE).

## Project Website Deployment

The website for this project is deployed via **GitHub Pages**, served from the `gh-pages` branch. The project is organized into a structured directory layout to support clarity and maintainability:

* **HTML** — Main HTML files are located at the root level
* **CSS** — Stylesheets for layout and design
* **JS** — JavaScript files for interactivity and functionality
* **assets** — Static resources including images, fonts, and other media

## File Organization and Pathing

All file references should use **relative paths** (e.g., `./assets/image.jpg`) to ensure compatibility across both local development and GitHub Pages deployment. This avoids hardcoded URLs and simplifies environment transitions.

### Reference Examples

* **HTML Files**
  All HTML files, including reusable components like `nav.html`, are placed at the root of the repository.
  Example: `./nav.html`

* **CSS Files**
  Example: `./CSS/FAQ.css`

* **JavaScript Files**
  Example: `./JS/nav.js`

* **Images and Media**
  Example: `./assets/tiger.jpg`

* **Fonts**
  Example usage in a CSS file:

  ```css
  @font-face {
      font-family: 'NK57 Monospace';
      font-weight: normal;
      font-style: normal;
      src: url('./assets/nk57.otf');
  }
  ```

* **Dynamic Content Loading**
  When loading an HTML partial such as a navigation bar using `fetch()` in JavaScript:
  Example: `./nav.html`
---

## Local Development Workflow

To preview the site locally during development, it's recommended to use a lightweight HTTP server. This ensures proper resolution of relative paths and simulates GitHub Pages behavior.

To start a local server using Python, run the following command from the root of the repository:

```bash
python -m http.server
```

The site will be accessible at:
`http://localhost:8000`

Open this URL in your browser to preview and test the site. No path changes are required between local and production environments, as all paths are defined relatively.