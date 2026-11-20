# Maintainer: Tobias Richter
#
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tobias Richter
# GitHub: https://github.com/DerTobiasRichter

pkgname=matrix-wallpaper
pkgver=0.1.0
pkgrel=1
pkgdesc='Journald-driven digital rain wallpaper for Wayland/Hyprland'
arch=('x86_64')
url='https://github.com/DerTobiasRichter/Matrix-Wallpaper'
license=('GPL-3.0-only')

depends=('gtk4' 'gtk4-layer-shell' 'systemd-libs' 'cairo' 'glibc' 'libgcc' 'glib2' 'libstdc++')
makedepends=('cmake>=4.4' 'ninja' 'pkgconf')

source=("${pkgname}-${pkgver}.tar.gz::${url}/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('6229f1622a5ffa60411997403de8b46ac79f308c0f4063c5ba712651a35740a2')

build() {
    cmake -S "${srcdir}/Matrix-Wallpaper-${pkgver}" \
          -B "${srcdir}/build" \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr

    cmake --build "${srcdir}/build" --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
}
