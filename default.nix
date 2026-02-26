with import <nixpkgs> {};
pkgs.mkShell {
  stdenv = pkgs.clangStdenv;
  packages = with pkgs; [
    glfw
    cmake
    clang
    wayland
    libx11
  ];
  nativeBuildInputs = [
    pkgs.libGL
  ];

  LD_LIBRARY_PATH = with pkgs; lib.makeLibraryPath [
    libGL
    libxrandr
    libxinerama
    libxcursor
    libxi
  ];
  LIBCLANG_PATH = "${pkgs.libclang.lib}/lib";
}

# thanks to https://github.com/deltaphc/raylib-rs/issues/187 !
# TODO: implement buildFHSEnv
