![Scrap splash](/extras/scrap_splash.png)

# Scrap

Scrap is a new block based programming language with the aim towards advanced users. 
It is written in pure C and mostly inspired by other block based languages such as [Scratch](https://scratch.mit.edu/) and
its forks such as [Turbowarp](https://turbowarp.org).

## ⚠️ WARNING ⚠️

Scrap is currently in **Beta** stage. Some features may be missing or break, so use with caution!

## Building

### Dependencies

Before building you need to have [Raylib](https://github.com/raysan5/raylib) built and installed on your system 
**(Make sure you use Raylib 5.0 and enabled SUPPORT_FILEFORMAT_SVG in `config.h` or else it will not build properly!)**.

### Build

Currently only *linux* is supported for building. To build and run Scrap, just run following commands:

```bash
make
./scrap
```

## Wait, there is more?

In `examples/` folder you can find some example code writen in Scrap that uses most features from Scrap

In `extras/` folder you can find some various artwork made for Scrap. 
The splash art was made by [@FlaffyTheBest](https://scratch.mit.edu/users/FlaffyTheBest/), 
the logo was made by [@Grisshink](https://github.com/Grisshink) with some inspiration for logo from [@encoded3](https://github.com/encoded3)

## License

All scrap code is licensed under the terms of [GPLv3 license](/LICENSE).
