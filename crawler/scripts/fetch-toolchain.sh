#!/bin/sh
# Download the folder-local C compiler (Zig) into toolchain/.
#
#   ./scripts/fetch-toolchain.sh
#
# Zig ships a complete C compiler plus the headers and start files for
# x86_64-linux-gnu, aarch64-linux-gnu and arm-linux-gnueabihf, so all three
# builds work with nothing installed on the machine doing the building.
# Everything lands in this project folder; nothing goes to /usr or the PATH.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE"
ZIGDIR=toolchain/zig
VERSION=${ZIG_VERSION:-0.14.1}

if [ -x "$ZIGDIR/zig" ]; then
  echo "toolchain/zig already present ($("$ZIGDIR/zig" version))"
  exit 0
fi

case "$(uname -m)" in
  x86_64|amd64)  HOST=x86_64 ;;
  aarch64|arm64) HOST=aarch64 ;;
  *) echo "error: no Zig build for $(uname -m)" >&2
     echo "       set ZIG=/path/to/zig and re-run make" >&2
     exit 1 ;;
esac
case "$(uname -s)" in
  Linux)  OS=linux ;;
  Darwin) OS=macos ;;
  *) echo "error: unsupported host $(uname -s)" >&2; exit 1 ;;
esac

TARBALL="zig-$HOST-$OS-$VERSION.tar.xz"
URL="https://ziglang.org/download/$VERSION/$TARBALL"
mkdir -p toolchain
echo "downloading $URL"
echo "(about 50 MB, once; the compiler stays inside this folder)"
curl -fL --retry 3 --progress-bar -o "toolchain/$TARBALL" "$URL"
tar -xJf "toolchain/$TARBALL" -C toolchain
mv "toolchain/zig-$HOST-$OS-$VERSION" "$ZIGDIR"
rm -f "toolchain/$TARBALL"
echo
echo "toolchain/zig ready ($("$ZIGDIR/zig" version))"
