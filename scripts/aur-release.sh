#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tobias Richter
# GitHub: https://github.com/DerTobiasRichter
#
# Created on: 2026-11-20
# Last Modified: 2026-11-20
#
#
#

set -Eeuo pipefail

usage() {
  cat <<'HELP'
Usage: scripts/aur-release.sh VERSION [--publish] [--aur-dir PATH]

Examples:
  scripts/aur-release.sh 0.1.1
  scripts/aur-release.sh 0.1.1 --publish --aur-dir "$HOME/aur/matrix-wallpaper"

Requirements: makepkg, updpkgsums (pacman-contrib), namcap, git, curl,
              and optionally extra-x86_64-build (devtools).
HELP
}

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
info() { printf '\n==> %s\n' "$*"; }

[[ $# -ge 1 ]] || { usage; exit 2; }
[[ $1 != -h && $1 != --help ]] || { usage; exit 0; }
version=$1; shift
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][a-zA-Z0-9]+)*$ ]] || die "Invalid version: $version"
publish=false
aur_dir=
while (($#)); do
  case "$1" in
    --publish) publish=true; shift ;;
    --aur-dir) [[ $# -ge 2 ]] || die '--aur-dir needs a path'; aur_dir=$2; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown argument: $1" ;;
  esac
done

for cmd in git curl makepkg updpkgsums namcap; do
  command -v "$cmd" >/dev/null || die "Missing tool: $cmd"
done

root=$(git rev-parse --show-toplevel)
cd "$root"
[[ -f PKGBUILD ]] || die 'PKGBUILD not found in repository root'
[[ -f CMakeLists.txt ]] || die 'CMakeLists.txt not found'
[[ -z $(git status --porcelain) ]] || die 'Working tree must be clean before release preparation'

tag="v${version}"
info "Checking remote tag ${tag}"
git ls-remote --exit-code --tags origin "refs/tags/${tag}" >/dev/null || die "Remote tag ${tag} not found"

# Work in a separate directory: never modify the source repository or its tags.
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT
cp PKGBUILD "$workdir/PKGBUILD"
cd "$workdir"

# Require the source URL to be derived from pkgver (rather than hardcoded).
grep -Fq 'v${pkgver}.tar.gz' PKGBUILD || die 'PKGBUILD source URL must contain v${pkgver}.tar.gz'

sed -i -E "s/^pkgver=.*/pkgver=${version}/; s/^pkgrel=.*/pkgrel=1/" PKGBUILD
info 'Downloading release source and updating checksum'
updpkgsums
makepkg --verifysource

info 'Generating .SRCINFO'
makepkg --printsrcinfo > .SRCINFO

info 'Building package'
makepkg -Cfs --noconfirm

info 'Checking PKGBUILD and built package(s)'
namcap PKGBUILD
shopt -s nullglob
packages=("${workdir}"/matrix-wallpaper-*.pkg.tar.*)
((${#packages[@]})) || die 'No built package found'
for pkg in "${packages[@]}"; do
  namcap "$pkg"
done

if command -v extra-x86_64-build >/dev/null; then
  info 'Running clean-chroot build (extra-x86_64-build)'
  extra-x86_64-build
else
  info 'Clean-chroot build skipped (install devtools for extra-x86_64-build)'
fi

out="${XDG_CACHE_HOME:-$HOME/.cache}/matrix-wallpaper/aur/${version}"
mkdir -p "$out"
cp PKGBUILD .SRCINFO "$out/"
info "Prepared AUR metadata: $out"

if ! $publish; then
  printf '\nReview the generated files, then publish with:\n  %q %q --publish --aur-dir %q\n' \
    "$root/scripts/aur-release.sh" "$version" "${aur_dir:-$HOME/aur/matrix-wallpaper}"
  exit 0
fi

[[ -n $aur_dir ]] || die 'Publishing requires --aur-dir PATH'
[[ -d $aur_dir/.git ]] || die "Not a local AUR git clone: $aur_dir"
[[ -z $(git -C "$aur_dir" status --porcelain) ]] || die 'AUR checkout is not clean'
remote=$(git -C "$aur_dir" remote get-url origin)
[[ $remote == *aur.archlinux.org/matrix-wallpaper.git ]] || die "Unexpected AUR remote: $remote"

cp PKGBUILD .SRCINFO "$aur_dir/"
git -C "$aur_dir" add PKGBUILD .SRCINFO
if git -C "$aur_dir" diff --cached --quiet; then
  info 'AUR files unchanged; nothing to publish'
  exit 0
fi

git -C "$aur_dir" diff --cached --stat
printf '\nPush matrix-wallpaper %s to AUR? Type YES: ' "$version"
read -r answer
[[ $answer == YES ]] || die 'Publication cancelled; changes remain staged in AUR checkout'
git -C "$aur_dir" commit -m "Update to ${version}"
git -C "$aur_dir" push origin HEAD:master
info 'AUR publication completed'
