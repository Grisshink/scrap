![Scrap splash](/extras/scrap_splash.png)

# Scrap

Scrap is a new block based programming language with the aim towards advanced users. 
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

## ⚠️ WARNING ⚠️

Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Controls

- Click on blocks to pick up them, click again to drop them
- You can use `Ctrl` to take only one block and `Alt` to pick up its duplicate
- Hold left mouse button to move around code space
- Holding middle mouse button will do the same, except it works all the time
- Press `Space` to jump to chain in code base (Useful if you got lost in code base)

## Building

### Dependencies

Before building you need to have [Raylib](https://github.com/raysan5/raylib) built and installed on your system 
**(Make sure you use Raylib 5.0 and enabled SUPPORT_FILEFORMAT_SVG in `config.h` or else it will not build properly!)**.

### Build

Currently Scrap can be built for *Windows* and *linux*. 

#### Linux build

To build and run Scrap on linux you need to install `gcc` and `make`. After install, just run following commands:

```bash
make
./scrap
```

#### Windows build

To build and run Scrap on Windows you need to have [mingw-w64](https://www.mingw-w64.org/) installed. 
The most recommended way to use it is through [MSYS2](https://www.msys2.org/). 
Scrap expects `libraylib.a` file to be located in `raylib/lib/` folder and its headers in `raylib/include` folder. 
After that, just run following commands:

```bash
make -B TARGET=WINDOWS
```

This will build `scrap.exe` binary which can be run normally.

## Wait, there is more?

In `examples/` folder you can find some example code writen in Scrap that uses most features from Scrap

In `extras/` folder you can find some various artwork made for Scrap. 
The splash art was made by [@FlaffyTheBest](https://scratch.mit.edu/users/FlaffyTheBest/), 
the logo was made by [@Grisshink](https://github.com/Grisshink) with some inspiration for logo from [@unixource](https://github.com/unixource), 
the wallpaper was made by [@Grisshink](https://github.com/Grisshink)

## License

All scrap code is licensed under the terms of [GPLv3 license](/LICENSE).
