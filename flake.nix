{
  description = "raw photography workflow that sucks less";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils, ... }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages = rec {
          vkdt-git = pkgs.stdenv.mkDerivation {
            pname = "vkdt";
            version = "git";

            src = pkgs.lib.cleanSource ./.;

            nativeBuildInputs = with pkgs; [
              cargo
              clang
              cmake
              glslang
              llvm
              llvmPackages.openmp
              pkg-config
              rsync
              rustPlatform.cargoSetupHook
              xxd
            ];

            buildInputs = with pkgs; [
              alsa-lib
              exiv2
              ffmpeg
              freetype
              glfw
              libjpeg
              libmad
              libvorbis
              llvmPackages.openmp
              pugixml
              vulkan-headers
              vulkan-loader
              vulkan-tools
            ];

            dontUseCmakeConfigure = true;

            makeFlags = [
              "DESTDIR=$(out)"
              "prefix="
            ];
          };

          default = vkdt-git;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.vkdt-git ];
        };
      }
    );
}
