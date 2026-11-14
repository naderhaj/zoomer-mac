{
  description = "Tiny screen magnifier for macOS";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs =
    { self, nixpkgs }:
    let
      # Apple Silicon only: objc.h calls objc_msgSend in a way that is only
      # valid on arm64.
      system = "aarch64-darwin";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "zoomer";
        version = "0.1.1";

        src = self;

        # The Makefile ad-hoc signs the binary with `codesign`.
        nativeBuildInputs = [ pkgs.darwin.sigtool ];

        makeFlags = [
          "CC=${pkgs.stdenv.cc.targetPrefix}cc"
          "PREFIX=${placeholder "out"}"
        ];

        meta = {
          description = "Tiny screen magnifier for macOS";
          homepage = "https://github.com/naderhaj/zoomer-mac";
          license = pkgs.lib.licenses.mit;
          mainProgram = "zoomer";
          platforms = [ system ];
        };
      };
    };
}
