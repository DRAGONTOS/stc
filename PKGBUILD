pkgname=stc-git
_gitname=stc
pkgver=0.2.0
pkgrel=1
pkgdesc="Easily download collections and mods from steam."
url="https://github.com/DRAGONTOS/stc"
arch=('x86_64')
license=('GPLv3')
depends=('curl' 'libarchive')
makedepends=('git' 'cmake' 'ninja' 'pkgconf')
provides=('stc')
conflicts=('stc')
source=('git+https://github.com/DRAGONTOS/stc.git')
md5sums=('SKIP')

pkgver() {
    cd ${_gitname}
    sed -n 's/.*"version": "\([^"]*\)".*/\1/p' props.json
}

prepare() {
    rm -rf build
}

build() {
    cd ${_gitname}
    cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja
    cmake --build ./build
}

package() {
    cd ${_gitname}
    install -Dm755 ./build/stc ${pkgdir}/usr/bin/stc
}
