#!/bin/sh
# Fetch just what linking needs for a foreign architecture, into vendor/<arch>/.
#
#   ./scripts/fetch-target-libs.sh arm64
#   ./scripts/fetch-target-libs.sh armhf
#   ./scripts/fetch-target-libs.sh x86_64
#
# Downloads the runtime libraries (libcurl4, zlib1g, libsqlite3-0) and the
# matching headers straight from the distribution archive and unpacks them
# locally. No root, no dpkg --add-architecture, no ARM dev packages on the host.
set -eu

ARCH=${1:-arm64}
case "$ARCH" in
  arm64|aarch64) ARCH=arm64; DEBARCH=arm64 ;;
  armhf|armv7|arm|arm32) ARCH=armhf; DEBARCH=armhf ;;
  x86_64|amd64|x64) ARCH=x86_64; DEBARCH=amd64 ;;
  *) echo "usage: $0 [arm64|armhf|x86_64]" >&2; exit 2 ;;
esac

HERE=$(cd "$(dirname "$0")/.." && pwd)
VENDOR="$HERE/vendor/$ARCH"
LIBDIR="$VENDOR/lib"
INCDIR="$VENDOR/include"
CACHE="$HERE/vendor/cache"
mkdir -p "$LIBDIR" "$INCDIR" "$CACHE"

if [ -f "$LIBDIR/libcurl.so.4" ] && [ -f "$LIBDIR/libz.so.1" ] \
   && [ -f "$INCDIR/curl/curl.h" ]; then
  echo "vendor/$ARCH already populated"
  exit 0
fi

CODENAME=""
BASE=""
DISTRO="unknown"
LIKE=""
if [ -r /etc/os-release ]; then
  . /etc/os-release
  DISTRO=${ID:-unknown}
  LIKE=${ID_LIKE:-}
  # Mint and other derivatives set VERSION_CODENAME to their own name, so
  # prefer UBUNTU_CODENAME when it exists.
  CODENAME=${UBUNTU_CODENAME:-${VERSION_CODENAME:-}}
fi
case "$DISTRO $LIKE" in
  *ubuntu*)
    if [ "$DEBARCH" = amd64 ]; then
      BASE="https://archive.ubuntu.com/ubuntu"
    else
      BASE="https://ports.ubuntu.com/ubuntu-ports"
    fi
    [ -n "$CODENAME" ] || CODENAME=noble ;;
  *)
    BASE="${ARCHIVE_URL:-https://deb.debian.org/debian}"
    [ -n "$CODENAME" ] || CODENAME=bookworm ;;
esac
echo "fetching $DEBARCH packages from $BASE ($CODENAME)"

get() { curl -fsSL --retry 2 "$1"; }

IDX="$CACHE/Packages-$DEBARCH"
: > "$IDX"
# Newer suites ship Packages.xz only, older ones Packages.gz. Try both.
fetch_index() {
  get "$BASE/dists/$1/main/binary-$DEBARCH/Packages.gz" 2>/dev/null \
    | gzip -dc >> "$IDX" 2>/dev/null || \
  get "$BASE/dists/$1/main/binary-$DEBARCH/Packages.xz" 2>/dev/null \
    | xz -dc >> "$IDX" 2>/dev/null || true
}
for suite in "$CODENAME-updates" "$CODENAME" "$CODENAME-security"; do
  fetch_index "$suite"
done
if [ ! -s "$IDX" ]; then
  echo "error: could not read the $DEBARCH package index from $BASE" >&2
  echo "       retry with: ARCHIVE_URL=https://deb.debian.org/debian $0 $ARCH" >&2
  exit 1
fi

find_deb() {
  for pkg in $1; do
    fn=$(awk -v want="$pkg" '
      BEGIN { RS = "" }
      {
        name = ""; file = ""
        n = split($0, lines, "\n")
        for (i = 1; i <= n; i++) {
          if (lines[i] ~ /^Package: /) name = substr(lines[i], 10)
          if (lines[i] ~ /^Filename: /) file = substr(lines[i], 11)
        }
        if (name == want && file != "") { print file; exit }
      }' "$IDX")
    if [ -n "$fn" ]; then echo "$fn"; return 0; fi
  done
  return 1
}

extract_deb() {
  deb="$CACHE/$(basename "$1")"
  [ -f "$deb" ] || get "$1" > "$deb"
  if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb -x "$deb" "$2"
  else
    ( cd "$2" && ar p "$deb" data.tar.xz | tar -xJ 2>/dev/null \
      || ar p "$deb" data.tar.gz | tar -xz )
  fi
}

want() {
  fn=$(find_deb "$2") || { echo "  MISSING $1 ($2)"; return 1; }
  echo "  $1 <- $(basename "$fn")"
  tmp="$VENDOR/tmp-extract"
  mkdir -p "$tmp"
  extract_deb "$BASE/$fn" "$tmp" || { rm -r "$tmp"; return 1; }
  find "$tmp" \( -name 'libcurl.so.4*' -o -name 'libz.so.1*' \
              -o -name 'libsqlite3.so.0*' \) \( -type f -o -type l \) \
       -exec cp -a {} "$LIBDIR/" \;
  find "$tmp/usr/include" -maxdepth 2 \( -name 'curl' -o -name 'curl.h' \
       -o -name 'zlib.h' -o -name 'zconf.h' -o -name 'sqlite3.h' \
       -o -name 'sqlite3ext.h' \) -exec cp -aL {} "$INCDIR/" \; 2>/dev/null || true
  rm -r "$tmp"
  return 0
}

echo "downloading:"
want "libcurl (runtime)"  "libcurl4t64 libcurl4"
want "zlib (runtime)"     "zlib1g"
want "sqlite (runtime)"   "libsqlite3-0"
want "curl headers"       "libcurl4-openssl-dev"
want "zlib headers"       "zlib1g-dev"
want "sqlite headers"     "libsqlite3-dev"

echo
status=0
for f in libcurl.so.4 libz.so.1; do
  if [ -f "$LIBDIR/$f" ]; then echo "  ok  vendor/$ARCH/lib/$f"
  else echo "  MISSING vendor/$ARCH/lib/$f"; status=1; fi
done
if [ -f "$INCDIR/curl/curl.h" ]; then echo "  ok  vendor/$ARCH/include/curl/curl.h"
else echo "  MISSING vendor/$ARCH/include/curl/curl.h"; status=1; fi
if [ -f "$LIBDIR/libsqlite3.so.0" ] && [ -f "$INCDIR/sqlite3.h" ]; then
  echo "  ok  vendor/$ARCH/lib/libsqlite3.so.0"
else
  echo "  note: no sqlite3 for $ARCH, build that target with HAVE_SQLITE=0"
fi

if [ "$status" -ne 0 ]; then
  echo "error: could not assemble the $ARCH link environment" >&2
  exit 1
fi
echo "vendor/$ARCH ready"

# The downloaded packages and index are only a cache; keep the disk footprint
# small unless the caller asked to hold on to them.
if [ "${KEEP_CACHE:-0}" != "1" ] && [ -d "$CACHE" ]; then
  for f in "$CACHE"/*; do [ -e "$f" ] && rm -f "$f"; done
fi
