pkgname=RapidMenu-git
_gitname=RapidMenu
pkgver=0.2.4
pkgrel=1
pkgdesc="Utilize configuration files to create runner scripts."
url="https://github.com/DRAGONTOS/RapidMenu"
arch=('x86_64')
license=('GPLv3')
depends=('tomlplusplus' 'cpptoml')
makedepends=('git' 'cmake' 'ninja')
provides=('RapidMenu')
conflicts=('RapidMenu')
source=('git+https://github.com/DRAGONTOS/RapidMenu.git')
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
