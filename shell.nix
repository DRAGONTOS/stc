{ pkgs ? import <nixpkgs> {}}:
  pkgs.mkShell {
    packages = with pkgs; [
      cmake
      gnumake
      ninja 
      tomlplusplus
      cpptoml
    ];
}
