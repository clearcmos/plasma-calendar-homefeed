pkgname=plasma-calendar-homefeed
pkgdesc='Plasma 6 digital clock calendar plugin and next-event panel widget for a self-hosted JSON feed'
pkgver=0.1.0.r0.g0000000
pkgrel=1
arch=('x86_64')
url='https://github.com/clearcmos/plasma-calendar-homefeed'
license=('MIT')
depends=(qt6-base qt6-declarative kconfig kcoreaddons kdeclarative libplasma plasma-workspace)
makedepends=(git cmake ninja extra-cmake-modules)
source=("$pkgname::git+https://github.com/clearcmos/plasma-calendar-homefeed.git")
sha256sums=('SKIP')

pkgver() {
    cd "$pkgname"
    local cmake_ver
    cmake_ver=$(sed -n 's/^project(plasma-calendar-homefeed VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
    printf "%s.r%s.g%s" "$cmake_ver" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cmake -S "$pkgname" -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=None \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=ON
    cmake --build build
}

check() {
    ctest --test-dir build --output-on-failure
}

package() {
    DESTDIR="$pkgdir" cmake --install build
    install -Dm0644 "$pkgname/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
