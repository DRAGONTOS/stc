pkgname=stc-git
_gitname=stc
pkgver=0.1.7
pkgrel=1
pkgdesc="Easily download collections and mods from steam."
url="https://github.com/DRAGONTOS/stc"
arch=('x86_64')
license=('GPLv3')
makedepends=('git' 'cmake' 'ninja')
provides=('stc')
conflicts=('stc')
source=('git+https://github.com/DRAGONTOS/stc.git')
md5sums=('SKIP')

prepare() {
  rm -rf build
}

build() {
  cd ${_gitname}
  cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -S . -B ./build -G Ninja
  cmake --build ./build
  chmod -R 777 ./build
}

package() {
  cd ${_gitname}
  sudo make install
}
