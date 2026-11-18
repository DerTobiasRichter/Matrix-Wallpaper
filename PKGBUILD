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
url='https://github.com/DerTobiasRichter'
license=('GPL-3.0-only')

depends=('gtk4' 'gtk4-layer-shell' 'systemd-libs' 'cairo')
makedepends=('cmake' 'ninja' 'pkgconf')

source=()

prepare() {
    install -d "$srcdir/source"

    cp -- "$startdir/CMakeLists.txt" \
        "$startdir"/*.cpp \
        "$startdir"/*.h \
        "$srcdir/source/"
}

build() {
    cmake -S "$srcdir/source" \
          -B "$srcdir/build" \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr

    cmake --build "$srcdir/build" --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
}