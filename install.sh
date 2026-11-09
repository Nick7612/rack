#!/bin/sh
# rack installer.
#
# Installs a prebuilt rack — it never compiles anything. It uses the local
# build/rack when run from a checkout that has one, otherwise it downloads the
# latest release. Run it as root, or let it call sudo.

set -eu

REPO="Nick7612/rack"
PREFIX="${PREFIX:-/usr}"

usage() {
    cat <<EOF
Usage: install.sh [ARCHIVE]

Install a prebuilt rack from ARCHIVE, the local build/, or the latest release.

  sudo ./install.sh                          install into /usr
  PREFIX=\$HOME/.local ./install.sh          install into a user prefix
  sudo ./install.sh rack-0.1.0-x86_64.tar.gz install a specific release

This script never builds rack. Run bin/build first if you want to install
your own build.
EOF
}

case "${1:-}" in
    -h|--help)
        usage
        exit 0
        ;;
esac

say() { printf '%s\n' "$*"; }
die() { printf 'rack install: %s\n' "$*" >&2; exit 1; }

# Use sudo only when the destination is not writable by this user. Find the
# nearest existing ancestor so a missing $PREFIX still gets the right answer.
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    destination="$PREFIX"
    while [ ! -e "$destination" ] && [ "$destination" != "/" ]; do
        destination=$(dirname "$destination")
    done
    if [ ! -w "$destination" ]; then
        command -v sudo >/dev/null 2>&1 || die "sudo is required to install into $PREFIX"
        SUDO="sudo"
    fi
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" 2>/dev/null && pwd || pwd)
archive="${1:-}"
tmp=""

cleanup() {
    if [ -n "$tmp" ]; then
        rm -rf "$tmp"
    fi
}
trap cleanup EXIT INT TERM

# A checkout with a local build installs that build. Nothing is rebuilt.
if [ -z "$archive" ] && [ -f "$script_dir/rack.pro" ] && [ -x "$script_dir/build/rack" ]; then
    binary="$script_dir/build/rack"
    desktop="$script_dir/pkgbuild/rack.desktop"
    icon="$script_dir/pkgbuild/rack.svg"
    license="$script_dir/LICENSE"
    say "Installing the local build at $binary"
else
    if [ -z "$archive" ]; then
        command -v curl >/dev/null 2>&1 || die "curl is required to download the release"
        case "$(uname -m)" in
            x86_64) asset="rack-x86_64.tar.gz" ;;
            *) die "No prebuilt release for $(uname -m). Build from source with bin/build." ;;
        esac
        tmp=$(mktemp -d)
        url="https://github.com/$REPO/releases/latest/download/$asset"
        say "Downloading the latest rack release..."
        curl -fsSL "$url" -o "$tmp/$asset" || die "Could not download $url"
        archive="$tmp/$asset"
    fi

    [ -f "$archive" ] || die "no such archive: $archive"
    if [ -z "$tmp" ]; then
        tmp=$(mktemp -d)
    fi
    tar -xzf "$archive" -C "$tmp"
    root="$tmp/rack"
    binary="$root/rack"
    desktop="$root/rack.desktop"
    icon="$root/rack.svg"
    license="$root/LICENSE"
    [ -x "$binary" ] || die "$archive does not contain a rack binary"
    say "Installing $(basename "$archive")"
fi

$SUDO install -Dm755 "$binary" "$PREFIX/bin/rack"
$SUDO install -Dm644 "$desktop" "$PREFIX/share/applications/rack.desktop"
$SUDO install -Dm644 "$icon" "$PREFIX/share/icons/hicolor/scalable/apps/rack.svg"
if [ -f "$license" ]; then
    $SUDO install -Dm644 "$license" "$PREFIX/share/licenses/rack/LICENSE"
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    $SUDO update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    $SUDO gtk-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" 2>/dev/null || true
fi

say "Installed rack. Launch it from the app launcher or run 'rack'."
