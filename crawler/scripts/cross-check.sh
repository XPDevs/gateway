#!/bin/sh
# Report whether the cross builds can run, and name the fix for anything missing.
#
#   ./scripts/cross-check.sh          both architectures
#   ./scripts/cross-check.sh arm64    just one
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE" || exit 1

want=${1:-all}
missing=0
ok()    { printf '  ok       %s\n' "$1"; }
bad()   { printf '  MISSING  %s\n' "$1"; missing=$((missing+1)); }

check_arch() {
  arch=$1; cc=$2; pkg=$3
  echo "$arch"
  if command -v "$cc" >/dev/null 2>&1; then
    ok "cross compiler $cc ($($cc -dumpversion))"
  else
    bad "cross compiler $cc  -> sudo apt install $pkg"
  fi
  if [ -f "vendor/$arch/lib/libcurl.so.4" ] && [ -f "vendor/$arch/include/curl/curl.h" ]; then
    ok "vendor/$arch (libcurl, zlib, sqlite3, headers)"
  else
    bad "vendor/$arch  -> ./scripts/fetch-target-libs.sh $arch"
  fi
  echo
}

case "$want" in
  all|'') check_arch arm64 aarch64-linux-gnu-gcc gcc-aarch64-linux-gnu
          check_arch armhf arm-linux-gnueabihf-gcc gcc-arm-linux-gnueabihf ;;
  arm64|aarch64) check_arch arm64 aarch64-linux-gnu-gcc gcc-aarch64-linux-gnu ;;
  armhf|arm32)   check_arch armhf arm-linux-gnueabihf-gcc gcc-arm-linux-gnueabihf ;;
  *) echo "usage: $0 [all|arm64|armhf]"; exit 2 ;;
esac

if [ "$missing" -eq 0 ]; then
  echo "ready: make dist"
  exit 0
fi
echo "$missing requirement(s) missing. make dist fixes the vendor/ half by itself."
exit 1
