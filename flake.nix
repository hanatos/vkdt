{
  description = "raw photography workflow that sucks less";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      utils,
      ...
    }:
    utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages = rec {
          vkdt-git = pkgs.stdenv.mkDerivation rec {
            pname = "vkdt";
            version = "git";

            src = pkgs.lib.cleanSource ./.;

            cargoRoot = "src/pipe/modules/i-raw/rawloader-c";

            cargoDeps = pkgs.rustPlatform.fetchCargoVendor {
              inherit pname src cargoRoot;
              hash = "sha256-l2mOX5sVXE8nm86EP0jwXOxyu+jQqgpZrYsyXj1nsqo=";
            };

            strictDeps = true;

            nativeBuildInputs = with pkgs; [
              cargo
              clang
              cmake
              git
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
              "VKDT_USE_RAWINPUT=2" # typically sets this by checking for `rustc --version`, lets save a dependency :)
              "VKDT_USE_MCRAW=false" # TODO: support mcraw
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
