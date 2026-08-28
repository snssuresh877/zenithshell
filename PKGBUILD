# Maintainer: snssuresh877 <https://github.com/snssuresh877>
pkgname=zenithshell-git
pkgver=1.0.0
pkgrel=1
pkgdesc="Ultra-low memory (~26MB) native C++20 desktop shell & widget suite for Wayland/Hyprland"
arch=('x86_64' 'aarch64')
url="https://github.com/snssuresh877/zenithshell"
license=('MIT')
depends=(
    'gtk3'
    'gtk-layer-shell'
    'glib2'
    'cairo'
    'pango'
    'nlohmann-json'
    'pipewire'
    'wireplumber'
    'networkmanager'
    'bluez'
    'brightnessctl'
    'wl-clipboard'
)
makedepends=('git' 'cmake' 'ninja' 'pkgconf' 'gcc')
provides=('zenithshell')
conflicts=('zenithshell')
source=("git+https://github.com/snssuresh877/zenithshell.git")
sha256sums=('SKIP')

build() {
    cd "$srcdir/zenithshell"
    cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    ninja -C build
}

package() {
    cd "$srcdir/zenithshell"
    DESTDIR="$pkgdir" ninja -C build install
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
    install -Dm644 zenithshell.desktop "$pkgdir/usr/share/applications/zenithshell.desktop"
}
